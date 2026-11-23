/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include "source.h"
#include "transmitter.h"
#include "link.h"
#include "console_usb.h"

static uint64_t nonce(void) { static uint64_t n; return ++n; }
static struct gp_frame air_packet;
static bool radio_send(const struct gp_frame *f) { air_packet = *f; return true; }

struct bench {
	struct source pico;
	struct transmitter tx;
	struct link bridge;
	struct box_pad pad;
	struct gp_frame staged;
	bool cut_box_spi;
	struct gp_frame miso, ack;
	uint32_t us, ready_at, radio_at, lamp;
	bool drop_input, drop_ack, usb_paused, spi_paused;
	unsigned delivered;
	struct delivery log[512];
};

static struct gp_frame wire(const struct gp_frame *f)
{
	uint8_t bytes[GP_WIRE_SIZE];
	struct gp_frame copy;
	assert(gp_encode(bytes, sizeof(bytes), f) && gp_decode(&copy, bytes, sizeof(bytes)));
	return copy;
}

static void advance(struct bench *b, uint32_t duration)
{
	for (uint32_t elapsed = 0; elapsed < duration; elapsed += 50, b->us += 50) {
		uint32_t ms = b->us / 1000;
		source_tick(&b->pico, b->us);
		tx_tick(&b->tx, ms);
		link_tick(&b->bridge, ms);
		box_tick(&b->pad, ms, nonce);
		struct gp_frame f;
		if (!b->spi_paused && b->us >= b->ready_at &&
		    source_next(&b->pico, b->us, nonce, &f)) {
			b->ready_at = b->us + 450; /* clocks plus DMA rearm */
			f = wire(&f);
			tx_source(&b->tx, &f, ms);
			source_reply(&b->pico, b->us, &b->miso);
			f = tx_reply(&b->tx);
			b->miso = wire(&f);
		}
		if (b->us >= b->radio_at && tx_schedule(&b->tx, b->us, radio_send)) {
			b->radio_at = b->us + 400;
			if (b->drop_input && air_packet.type == GP_INPUT) {
				b->drop_input = false;
			} else {
				f = wire(&air_packet);
				if (b->ack.type) {
					if (b->drop_ack) b->drop_ack = false;
					else tx_ack(&b->tx, &b->ack, ms);
				}
				f = link_rf(&b->bridge, &f, ms);
				b->ack = wire(&f);
			}
		}
		if (!b->cut_box_spi && b->us % 300 == 0) {
			b->pad.led_valid = (b->lamp >> 24) != 0;
			b->pad.led_gen = (uint16_t)(b->lamp >> 8);
			b->pad.led_mode = b->lamp;
			struct gp_frame mosi = box_reply(&b->pad);
			mosi = wire(&mosi);
			box_receive(&b->pad, &b->staged, ms);
			f = link_spi(&b->bridge, &mosi, ms);
			b->staged = wire(&f);
		}
		if (!b->usb_paused && b->us % 1000 == 0 && !b->pad.delivered) {
			assert(b->delivered < sizeof(b->log)/sizeof(b->log[0]));
			b->log[b->delivered++] = (struct delivery){
				b->pad.kind == GP_RESET ? 0 : b->pad.tag, b->pad.state};
			box_complete(&b->pad, b->pad.tag, b->pad.state.seq, true);
		}
	}
}

static void set_buttons(struct bench *b, uint16_t buttons)
{
	struct gp_input s = {.buttons = buttons, .lt = (uint8_t)buttons,
		.lx = -32768, .ry = 32767};
	source_input(&b->pico, &s, b->us);
}

static void bridge_run(void)
{
	struct bench b = {0};
	box_start(&b.pad, nonce());
	link_init(&b.bridge, nonce);
	b.staged = link_spi(&b.bridge, NULL, b.us/1000);
	tx_reset(&b.tx, GP_REBOOT);
	b.miso = b.tx.reset;
	b.lamp = (1u << 24) | (65535u << 8) | 2;
	advance(&b, 20000);
	assert(!b.pico.nonce && !b.bridge.rf.session);
	set_buttons(&b, 0x3000);
	advance(&b, 20000);
	assert(b.pico.active && b.pico.led_gen == 65535 && b.pico.led_mode == 2);
	assert(b.log[b.delivered-1].input.buttons == 0x3000);
	source_led_done(&b.pico, b.pico.session, b.pico.led_gen);
	advance(&b, 12000);
	assert(b.tx.heartbeat.led_applied_valid && b.tx.heartbeat.led_gen == 65535);
	b.lamp = (1u << 24) | 0x09; /* wrap LED generation, raw 4P steady */
	unsigned start = b.delivered;
	b.drop_input = b.drop_ack = true;
	for (unsigned i = 1; i <= 120; i++) {
		set_buttons(&b, i);
		set_buttons(&b, i); /* repeated USB report */
		advance(&b, 1000);
	}
	advance(&b, 20000);
	assert(b.pico.active && b.pico.done == 120 && b.pico.resets == 0);
	assert(b.delivered == start + 120);
	for (unsigned i = 1; i <= 120; i++) {
		const struct gp_input *s = &b.log[start+i-1].input;
		assert(s->seq == i && s->buttons == i && s->lt == i && s->lx == -32768 && s->ry == 32767);
	}
	assert(b.pico.led_gen == 0 && b.pico.led_mode == 9 && b.pico.applied_gen == 65535);

	/* USB stops completing while both upstream hops still exchange heartbeats. */
	uint64_t old = b.pico.session;
	b.usb_paused = true;
	for (unsigned i = 0; i < 16; i++) {
		set_buttons(&b, i+1000);
		advance(&b, 1000);
	}
	assert((uint16_t)(b.pico.sent - b.pico.done) == GP_WINDOW);
	start = b.delivered;
	advance(&b, 40000);
	assert(b.pico.resets && b.pico.session != old);
	source_led_done(&b.pico, old, 5);
	assert(!b.pico.applied_valid);
	b.usb_paused = false;
	advance(&b, 25000);
	assert(b.pico.active && b.delivered >= start + 2 && !b.log[start].session);
	assert(b.log[b.delivered-1].input.buttons == 1015 && b.log[b.delivered-1].input.seq == 0);

	/* Cable out: relay an immediate RESET and do not originate idle heartbeats. */
	source_detach(&b.pico, GP_HITBOX_REMOVED);
	advance(&b, 5000);
	assert(!b.bridge.rf.session && !b.log[b.delivered-1].input.buttons);
	assert(!b.pico.nonce);
	set_buttons(&b, 0x1000);
	advance(&b, 20000);
	assert(b.pico.active && b.log[b.delivered-1].input.buttons == 0x1000);

	b.spi_paused = true;
	set_buttons(&b, 1);
	advance(&b, 55000);
	assert(b.pico.last_reason == GP_TIMEOUT && !b.bridge.rf.session);
	set_buttons(&b, 2);
	b.spi_paused = false;
	advance(&b, 20000);
	assert(b.pico.active && b.log[b.delivered-1].input.buttons == 2);

	/* Either Nordic can reboot first while the physical Hitbox stays held. */
	old = b.pico.session;
	link_init(&b.bridge, nonce);
	b.staged = link_spi(&b.bridge, NULL, b.us/1000);
	b.ack = (struct gp_frame){0};
	advance(&b, 100000);
	assert(b.pico.active && b.pico.session != old && b.log[b.delivered-1].input.buttons == 2);
	old = b.pico.session;
	b.tx = (struct transmitter){0};
	tx_reset(&b.tx, GP_REBOOT);
	b.miso = b.tx.reset;
	advance(&b, 100000);
	assert(b.pico.active && b.pico.session != old && b.log[b.delivered-1].input.buttons == 2);
	b.cut_box_spi = true;
	advance(&b, 52000);
	assert(!b.pad.link && !b.pad.state.buttons && !b.bridge.rf.session);
	b.cut_box_spi = false;
	advance(&b, 50000);
	assert(b.pico.active && b.pad.state.buttons == 2);
	box_start(&b.pad, nonce()); /* receiver Pico reboot / USB reset */
	old = b.pico.session;
	advance(&b, 80000);
	assert(b.pico.session != old && b.pico.active && b.pad.state.buttons == 2);

}

static void queue_edges(void)
{
	struct source s = {.session = 5, .active = true, .sync_sent = true, .tx_valid = true,
		.done = 65534, .rx_accept = 65534, .sent = 65534, .accepted = 65534, .produced = 65534,
		.last_spi = UINT32_MAX - 3000, .last_heartbeat = UINT32_MAX - 5000};
	struct gp_frame f;
	for (int i = 1; i <= GP_QUEUE_SIZE; ++i) {
		struct gp_input input = {.lt = i};
		source_input(&s, &input, 1);
		assert(s.active);
	}
	assert(source_next(&s, 20, nonce, &f) && f.type == GP_INPUT && f.data.input.seq == 65535);
	struct gp_status ack = {.flags = 7, .tx_accept = 65535, .rx_accept = 65535, .usb_done = 65535};
	f = (struct gp_frame){5, GP_STATUS, {.status = ack}};
	source_reply(&s, 30, &f);
	assert(source_next(&s, 40, nonce, &f) && f.type == GP_INPUT && f.data.input.seq == 0);
	struct gp_input input = {.lt = 33};
	source_input(&s, &input, 50);
	input.lt = 34;
	source_input(&s, &input, 60);
	assert(s.last_reason == GP_OVERFLOW && !s.session && s.current.lt == 34);
	assert(source_next(&s, 70, nonce, &f) && f.type == GP_RESET && f.session == 5);
	assert(source_next(&s, 80, nonce, &f) && f.type == GP_HELLO);
}

static void delivery_edges(void)
{
	struct box_usb u = {0};
	struct box_pad p = {.link = 10, .tag = 11, .kind = GP_INPUT, .state = {.seq = 65535}};
	box_submitted(&u, &p, 0x81, 20); /* authentication or keepalive */
	assert(!u.outstanding);
	u.capture = true;
	box_submitted(&u, &p, 0x02, 20);
	box_submitted(&u, &p, 0x80, 20);
	assert(!u.outstanding);
	box_submitted(&u, &p, 0x81, 20);
	box_finished(&u, &p, 0x82, true, 20);
	assert(u.outstanding && !p.delivered);
	box_finished(&u, &p, 0x81, true, 19);
	assert(!p.delivered && !u.outstanding);
	box_submitted(&u, &p, 0x81, 20);
	box_finished(&u, &p, 0x81, false, 20);
	assert(!p.delivered);
	box_submitted(&u, &p, 0x81, 20);
	struct gp_frame f = {21, GP_RESET, {.reset_reason = GP_TIMEOUT}};
	box_receive(&p, &f, 1);
	box_finished(&u, &p, 0x81, true, 20);
	assert(!p.delivered && p.tag == 21);
	box_submitted(&u, &p, 0x81, 20);
	box_finished(&u, &p, 0x81, true, 20);
	assert(p.delivered && box_reply(&p).data.status.flags == GP_USB_DONE_VALID);
	p.kind = GP_INPUT;
	p.state.seq = 65535;
	f = (struct gp_frame){21, GP_INPUT, {.input = {.seq = 0, .lt = 97}}};
	box_receive(&p, &f, 2);
	assert(p.state.seq == 0 && p.state.lt == 97 && !p.delivered);
}

int main(void)
{
	delivery_edges();
	bridge_run();
	queue_edges();
	puts("Console SPI and USB delivery, 1 kHz burst, LED completion, unplug, stall, overflow and wrap passed");
}
