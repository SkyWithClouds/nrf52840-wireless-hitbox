#ifndef GP_BRIDGE_PROTOCOL_H
#define GP_BRIDGE_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    GP_RECORD_SIZE = 16, GP_WIRE_SIZE = 24, GP_WINDOW = 8, GP_QUEUE_SIZE = 32,
    GP_POLL_TARGET_US = 1000, GP_POLL_LIMIT_US = 2000,
    GP_HEARTBEAT_MS = 10, GP_TIMEOUT_MS = 50
};

enum gp_type {
    GP_INPUT = 0x10, GP_SYNC, GP_HELLO, GP_OFFER, GP_STATUS, GP_RESET,
    GP_HEARTBEAT, GP_POLL
};
enum gp_status_flag {
    GP_TX_ACCEPT_VALID = 1, GP_RX_ACCEPT_VALID = 2,
    GP_USB_DONE_VALID = 4, GP_LED_VALID = 8
};
enum gp_reset_reason {
    GP_REBOOT = 1, GP_TIMEOUT, GP_OVERFLOW, GP_HITBOX_REMOVED, GP_USB_RESET
};

struct gp_input {
    uint16_t seq, buttons;
    uint8_t lt, rt;
    int16_t lx, ly, rx, ry;
};
struct gp_status {
    uint8_t flags;
    uint16_t tx_accept, rx_accept, usb_done, led_gen;
    uint8_t led_mode;
};
struct gp_heartbeat {
    uint32_t number;
    bool led_applied_valid;
    uint16_t led_gen;
};

/* In-memory API only. No structure in this header is sent via memcpy. */
struct gp_frame {
    uint64_t session;
    enum gp_type type;
    union {
        struct gp_input input;
        uint64_t hello_nonce;
        struct gp_status status;
        enum gp_reset_reason reset_reason;
        struct gp_heartbeat heartbeat;
    } data;
};

static inline uint64_t gp_read_le(const uint8_t *p, size_t n)
{
    uint64_t value = 0;
    for (size_t i = 0; i < n; ++i)
        value |= (uint64_t)p[i] << (8 * i);
    return value;
}

static inline void gp_write_le(uint8_t *p, size_t n, uint64_t value)
{
    for (size_t i = 0; i < n; ++i) {
        p[i] = (uint8_t)value;
        value >>= 8;
    }
}

/* Avoid implementation-defined conversion of out-of-range unsigned to signed. */
static inline int16_t gp_read_axis(const uint8_t *p)
{
    int32_t value = (int32_t)gp_read_le(p, 2);
    if (value >= 32768)
        value -= 65536;
    return (int16_t)value;
}

static inline uint8_t gp_crc8(const uint8_t *p, size_t n)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < n; ++i) {
        crc ^= p[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (uint8_t)((crc << 1) ^ ((crc & 0x80) ? 0x07 : 0));
    }
    return crc;
}

static inline bool gp_valid_frame(const struct gp_frame *f)
{
    if (f->type == GP_HELLO)
        return f->session == 0 && f->data.hello_nonce != 0;
    if (f->type == GP_RESET)
        return f->data.reset_reason >= GP_REBOOT && f->data.reset_reason <= GP_USB_RESET;
    if (f->session == 0)
        return false;
    switch (f->type) {
    case GP_OFFER: return f->data.hello_nonce != 0;
    case GP_SYNC: return f->data.input.seq == 0;
    case GP_STATUS: return (f->data.status.flags & 0xf0) == 0;
    case GP_INPUT: case GP_HEARTBEAT: case GP_POLL: return true;
    default: return false;
    }
}

/* Validate before writing. Reserved bytes are always encoded as zero. */
static inline bool gp_encode(uint8_t *out, size_t n, const struct gp_frame *f)
{
    if (!out || !f || n != GP_WIRE_SIZE || !gp_valid_frame(f))
        return false;
    memset(out, 0, GP_WIRE_SIZE);
    gp_write_le(out, 8, f->session);
    uint8_t *b = out + 8;
    b[0] = (uint8_t)f->type;
    switch (f->type) {
    case GP_INPUT: case GP_SYNC: {
        const struct gp_input *s = &f->data.input;
        gp_write_le(b + 1, 2, s->seq);
        gp_write_le(b + 3, 2, s->buttons);
        b[5] = s->lt;
        b[6] = s->rt;
        gp_write_le(b + 7, 2, (uint16_t)s->lx);
        gp_write_le(b + 9, 2, (uint16_t)s->ly);
        gp_write_le(b + 11, 2, (uint16_t)s->rx);
        gp_write_le(b + 13, 2, (uint16_t)s->ry);
        break;
    }
    case GP_HELLO: case GP_OFFER:
        gp_write_le(b + 1, 8, f->data.hello_nonce);
        break;
    case GP_STATUS: {
        const struct gp_status *s = &f->data.status;
        b[1] = s->flags;
        gp_write_le(b + 2, 2, s->tx_accept);
        gp_write_le(b + 4, 2, s->rx_accept);
        gp_write_le(b + 6, 2, s->usb_done);
        gp_write_le(b + 8, 2, s->led_gen);
        b[10] = s->led_mode;
        break;
    }
    case GP_RESET: b[1] = (uint8_t)f->data.reset_reason; break;
    case GP_HEARTBEAT:
        gp_write_le(b + 1, 4, f->data.heartbeat.number);
        b[5] = (uint8_t)f->data.heartbeat.led_applied_valid;
        gp_write_le(b + 6, 2, f->data.heartbeat.led_gen);
        break;
    case GP_POLL: break;
    default: return false;
    }
    out[GP_WIRE_SIZE - 1] = gp_crc8(out, GP_WIRE_SIZE - 1);
    return true;
}

/* Session ownership, progress/window bounds and RESET direction are state-machine checks. */
static inline bool gp_decode(struct gp_frame *out, const uint8_t *wire, size_t n)
{
    if (!out || !wire || n != GP_WIRE_SIZE || gp_crc8(wire, n - 1) != wire[n - 1])
        return false;
    const uint8_t *b = wire + 8;
    struct gp_frame f = {0};
    f.session = gp_read_le(wire, 8);
    f.type = (enum gp_type)b[0];
    size_t used;
    switch (f.type) {
    case GP_INPUT: case GP_SYNC: {
        struct gp_input *s = &f.data.input;
        s->seq = (uint16_t)gp_read_le(b + 1, 2);
        s->buttons = (uint16_t)gp_read_le(b + 3, 2);
        s->lt = b[5];
        s->rt = b[6];
        s->lx = gp_read_axis(b + 7);
        s->ly = gp_read_axis(b + 9);
        s->rx = gp_read_axis(b + 11);
        s->ry = gp_read_axis(b + 13);
        used = 15;
        break;
    }
    case GP_HELLO: case GP_OFFER:
        f.data.hello_nonce = gp_read_le(b + 1, 8);
        used = 9;
        break;
    case GP_STATUS: {
        struct gp_status *s = &f.data.status;
        s->flags = b[1];
        s->tx_accept = (uint16_t)gp_read_le(b + 2, 2);
        s->rx_accept = (uint16_t)gp_read_le(b + 4, 2);
        s->usb_done = (uint16_t)gp_read_le(b + 6, 2);
        s->led_gen = (uint16_t)gp_read_le(b + 8, 2);
        s->led_mode = b[10];
        used = 11;
        break;
    }
    case GP_RESET:
        f.data.reset_reason = (enum gp_reset_reason)b[1];
        used = 2;
        break;
    case GP_HEARTBEAT:
        if (b[5] > 1)
            return false;
        f.data.heartbeat.number = (uint32_t)gp_read_le(b + 1, 4);
        f.data.heartbeat.led_applied_valid = b[5] != 0;
        f.data.heartbeat.led_gen = (uint16_t)gp_read_le(b + 6, 2);
        used = 8;
        break;
    case GP_POLL: used = 1; break;
    default: return false;
    }
    for (size_t i = used; i < 15; ++i) {
        if (b[i] != 0)
            return false;
    }
    if (!gp_valid_frame(&f))
        return false;
    *out = f;
    return true;
}

#endif
