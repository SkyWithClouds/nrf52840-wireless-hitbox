#include "protocol.h"
#include <assert.h>
#include <stdio.h>

#define SESSION UINT64_C(0x0807060504030201)
#define NONCE UINT64_C(0xf0e0d0c0b0a09080)

struct vector {
    struct gp_frame frame;
    const char *hex;
};

/* CRC bytes independently obtained by polynomial long division (generator 0x107). */
static const struct vector vectors[] = {
    {{SESSION, GP_INPUT, {.input = {65535, 0x8101, 0x66, 0x99, -32768, 32767, -1, 0}}},
     "010203040506070810ffff018166990080ff7fffff0000a1"},
    {{SESSION, GP_SYNC, {.input = {0}}},
     "010203040506070811000000000000000000000000000073"},
    {{0, GP_HELLO, {.hello_nonce = NONCE}},
     "0000000000000000128090a0b0c0d0e0f0000000000000e2"},
    {{SESSION, GP_OFFER, {.hello_nonce = NONCE}},
     "0102030405060708138090a0b0c0d0e0f0000000000000f0"},
    {{SESSION, GP_STATUS, {.status = {15, 2, 1, 0, 0x3412, 9}}},
     "0102030405060708140f02000100000012340900000000b4"},
    {{SESSION, GP_RESET, {.reset_reason = GP_OVERFLOW}},
     "010203040506070815030000000000000000000000000081"},
    {{SESSION, GP_HEARTBEAT, {.heartbeat = {0x87654321, true, 0x3412}}},
     "010203040506070816214365870112340000000000000040"},
    {{SESSION, GP_POLL, {.input = {0}}},
     "0102030405060708170000000000000000000000000000c4"}
};

static void check_vector(const struct vector *v)
{
    uint8_t wire[GP_WIRE_SIZE], golden[GP_WIRE_SIZE];
    assert(strlen(v->hex) == 2 * GP_WIRE_SIZE);
    for (size_t i = 0; i < sizeof golden; ++i) {
        unsigned byte;
        assert(sscanf(v->hex + 2 * i, "%2x", &byte) == 1);
        golden[i] = (uint8_t)byte;
    }
    assert(gp_encode(wire, sizeof wire, &v->frame));
    assert(memcmp(wire, golden, sizeof wire) == 0);
    struct gp_frame decoded;
    assert(gp_decode(&decoded, golden, sizeof golden));
    assert(decoded.type == v->frame.type && decoded.session == v->frame.session);
    assert(gp_encode(wire, sizeof wire, &decoded));
    assert(memcmp(wire, golden, sizeof wire) == 0);
    for (size_t i = 0; i < sizeof wire; ++i) {
        for (unsigned b = 0; b < 8; ++b) {
            wire[i] ^= (uint8_t)(1u << b);
            assert(!gp_decode(&decoded, wire, sizeof wire));
            wire[i] ^= (uint8_t)(1u << b);
        }
    }
}

static void check_invalid(size_t vector_index, size_t offset, uint8_t bad)
{
    uint8_t wire[GP_WIRE_SIZE];
    struct gp_frame out = {.type = GP_POLL};
    assert(gp_encode(wire, sizeof wire, &vectors[vector_index].frame));
    wire[offset] = bad;
    wire[23] = gp_crc8(wire, 23);
    assert(!gp_decode(&out, wire, sizeof wire));
    assert(out.type == GP_POLL && out.session == 0);
}

int main(void)
{
    assert(gp_crc8((const uint8_t *)"123456789", 9) == 0xf4);
    for (size_t i = 0; i < sizeof vectors / sizeof vectors[0]; ++i)
        check_vector(&vectors[i]);
    check_invalid(0, 8, 0x20);  /* unknown version */
    check_invalid(1, 9, 1);     /* SYNC requires seq 0 */
    check_invalid(2, 0, 1);     /* HELLO requires zero session */
    check_invalid(4, 9, 0x10);  /* reserved status flag */
    check_invalid(5, 9, 0);     /* unknown RESET reason */
    check_invalid(6, 13, 2);    /* invalid LED validity flag */
    check_invalid(7, 9, 1);     /* reserved POLL byte */
    uint8_t wire[GP_WIRE_SIZE];
    struct gp_frame f = vectors[0].frame;
    assert(gp_encode(wire, sizeof wire, &f));
    assert(!gp_decode(&f, wire, sizeof wire - 1));
    assert(!gp_decode(&f, wire, sizeof wire + 1));
    f.session = 0;
    assert(!gp_encode(wire, sizeof wire, &f));
    puts("8 wire vectors, 1536 single-bit errors and malformed records passed");
}
