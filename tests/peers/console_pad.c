/* SPDX-License-Identifier: MIT */
#include "console_pad.h"

void box_start(struct box_pad *p, uint64_t nonce)
{
	/* Keep the latest console LED through a broken SPI link. USB reset clears it. */
	p->link = p->tag = 0;
	p->boot = nonce;
	p->state = (struct gp_input){0};
	p->kind = GP_RESET;
	p->delivered = false;
}

void box_tick(struct box_pad *p, uint32_t ms, uint64_t (*nonce)(void))
{
	if (p->link && ms - p->last_rx >= GP_TIMEOUT_MS) box_start(p, nonce());
}

void box_receive(struct box_pad *p, const struct gp_frame *f, uint32_t ms)
{
	if (f->type == GP_OFFER && f->data.hello_nonce == p->boot) {
		p->link = f->session;
		p->last_rx = ms;
		return;
	}
	if (!p->link) return;
	if (f->type == GP_RESET && !f->session && f->data.reset_reason == GP_REBOOT) {
		box_start(p, p->boot);
		return;
	}
	if (f->type == GP_POLL && (f->session == p->tag || f->session == p->link)) {
		p->last_rx = ms;
		return;
	}
	if (f->type == GP_RESET && f->session) {
		if (p->tag != f->session || p->kind != GP_RESET) {
			p->tag = f->session;
			p->kind = GP_RESET;
			p->state = (struct gp_input){0};
			p->delivered = false;
		}
	} else if (f->type == GP_SYNC || f->type == GP_INPUT) {
		if (p->tag != f->session) {
			if (f->type != GP_SYNC || p->kind != GP_RESET || !p->delivered) return;
		} else if (p->state.seq == f->data.input.seq && p->kind == f->type) {
			p->last_rx = ms;
			return;
		} else if (f->type != GP_INPUT || !p->delivered ||
			   f->data.input.seq != (uint16_t)(p->state.seq + 1)) return;
		p->tag = f->session;
		p->kind = f->type;
		p->state = f->data.input;
		p->delivered = false;
	} else {
		return;
	}
	p->last_rx = ms;
}

struct gp_frame box_reply(const struct box_pad *p)
{
	if (!p->link) return (struct gp_frame){0, GP_HELLO, {.hello_nonce = p->boot}};
	struct gp_status s = {0};
	if (p->tag && p->delivered) {
		s.flags |= GP_USB_DONE_VALID;
		s.usb_done = p->state.seq;
	}
	if (p->led_valid) {
		s.flags |= GP_LED_VALID;
		s.led_gen = p->led_gen;
		s.led_mode = p->led_mode;
	}
	return (struct gp_frame){p->tag ? p->tag : p->link, GP_STATUS, {.status = s}};
}

void box_complete(struct box_pad *p, uint64_t tag, uint16_t seq, bool success)
{
	if (success && p->tag == tag && p->state.seq == seq) p->delivered = true;
}
