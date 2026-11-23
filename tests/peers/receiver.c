/* SPDX-License-Identifier: MIT */
#include "receiver.h"

void receiver_reset(struct receiver *r)
{
	*r = (struct receiver){.neutral = true};
}

void receiver_tick(struct receiver *r, uint32_t now)
{
	if (r->session && (now - r->last_live >= GP_TIMEOUT_MS ||
	    (r->count && now - r->last_progress >= GP_TIMEOUT_MS))) receiver_reset(r);
}

static void enqueue(struct receiver *r, const struct gp_input *s, uint32_t now)
{
	if (!r->count) r->last_progress = now;
	r->queue[(r->head + r->count) % GP_WINDOW] = *s;
	r->count++;
	r->accepted = s->seq;
	r->last_live = now;
}

struct gp_frame receiver_frame(struct receiver *r, const struct gp_frame *f,
			       uint32_t now, uint32_t lamp, uint64_t (*nonce)(void))
{
	receiver_tick(r, now);
	if (f->type == GP_HELLO) {
		uint64_t id = r->session;
		if (f->data.hello_nonce != r->nonce) {
			if (f->data.hello_nonce != r->candidate_nonce) {
				r->candidate_nonce = f->data.hello_nonce;
				r->candidate = nonce();
			}
			id = r->candidate;
		}
		return (struct gp_frame){id, GP_OFFER, {.hello_nonce = f->data.hello_nonce}};
	}
	if (f->type == GP_SYNC && f->session == r->candidate) {
		uint64_t owner = r->candidate_nonce;
		receiver_reset(r);
		r->nonce = owner;
		r->session = f->session;
		enqueue(r, &f->data.input, now);
	}
	if (f->type == GP_RESET && r->session && f->session == r->session) receiver_reset(r);
	if (!r->session || f->session != r->session) {
		return (struct gp_frame){f->session, GP_RESET, {.reset_reason = GP_TIMEOUT}};
	}
	if (f->type == GP_INPUT && r->done_valid && r->count < GP_WINDOW &&
	    f->data.input.seq == (uint16_t)(r->accepted + 1)) enqueue(r, &f->data.input, now);
	if (f->type == GP_HEARTBEAT) {
		uint32_t diff = f->data.heartbeat.number - r->heartbeat;
		if (!r->heartbeat_valid || (diff && diff < 0x80000000U)) {
			r->heartbeat_valid = true;
			r->heartbeat = f->data.heartbeat.number;
			r->last_live = now;
		}
	}
	struct gp_status s = {.flags = GP_RX_ACCEPT_VALID, .rx_accept = r->accepted};
	if (r->done_valid) {
		s.flags |= GP_USB_DONE_VALID;
		s.usb_done = r->done;
	}
	if (lamp & (1u << 24)) {
		s.flags |= GP_LED_VALID;
		s.led_gen = (uint16_t)(lamp >> 8);
		s.led_mode = (uint8_t)lamp;
	}
	return (struct gp_frame){r->session, GP_STATUS, {.status = s}};
}

bool receiver_peek(const struct receiver *r, struct delivery *d)
{
	if (r->neutral) {
		*d = (struct delivery){0};
		return true;
	}
	if (!r->count) return false;
	*d = (struct delivery){r->session, r->queue[r->head]};
	return true;
}

void receiver_complete(struct receiver *r, const struct delivery *d, uint32_t now)
{
	if (!d->session) {
		r->neutral = false;
	} else if (d->session == r->session && r->count && r->queue[r->head].seq == d->input.seq) {
		r->done_valid = true;
		r->done = d->input.seq;
		r->last_progress = now;
		r->count--;
		r->head = (r->head + 1) % GP_WINDOW;
	}
}
