/* SPDX-License-Identifier: MIT */
#include "transmitter.h"

void tx_reset(struct transmitter *t, enum gp_reset_reason reason)
{
	uint64_t old = t->session;
	uint32_t late = t->poll_overdue;
	*t = (struct transmitter){.reset = {old, GP_RESET, {.reset_reason = reason}},
				 .notify_reset = old != 0, .poll_overdue = late};
}

void tx_tick(struct transmitter *t, uint32_t now)
{
	if (t->phase < SYNCING) return;
	bool outstanding = t->phase == SYNCING || t->status.tx_accept != t->status.usb_done;
	if (now - t->last_live >= GP_TIMEOUT_MS || now - t->last_peer >= GP_TIMEOUT_MS ||
	    (outstanding && now - t->last_progress >= GP_TIMEOUT_MS)) tx_reset(t, GP_TIMEOUT);
}

void tx_source(struct transmitter *t, const struct gp_frame *f, uint32_t now)
{
	struct gp_status *s = &t->status;
	if (f->type == GP_HELLO) {
		if (f->data.hello_nonce != t->nonce) {
			/* Only SYNC commits an offered session at RX. Preserve a genuine
			 * pending fault, but discovery itself must not reset the peer. */
			*t = (struct transmitter){.phase = DISCOVERY, .nonce = f->data.hello_nonce,
				.reset = t->reset, .notify_reset = t->notify_reset,
				.poll_overdue = t->poll_overdue};
		}
		return;
	}
	if (!t->session || f->session != t->session) return;
	switch (f->type) {
	case GP_RESET:
		tx_reset(t, f->data.reset_reason);
		break;
	case GP_SYNC:
		if (t->phase != OFFERED) break;
		t->phase = SYNCING;
		t->sync = f->data.input;
		s->flags = GP_TX_ACCEPT_VALID;
		t->last_peer = t->last_live = t->last_progress = now;
		break;
	case GP_INPUT: {
		uint16_t seq = f->data.input.seq;
		if (t->phase != ACTIVE || seq != (uint16_t)(s->tx_accept + 1) ||
		    (uint16_t)(seq - s->usb_done) > GP_WINDOW) break;
		if (s->tx_accept == s->usb_done) t->last_progress = now;
		t->window[seq % GP_WINDOW] = f->data.input;
		s->tx_accept = seq;
		t->last_live = now;
		break;
	}
	case GP_HEARTBEAT: {
		uint32_t delta = f->data.heartbeat.number - t->heartbeat.number;
		if (t->phase < SYNCING || (t->heartbeat_valid && (!delta || delta >= 0x80000000U))) break;
		t->heartbeat = f->data.heartbeat;
		t->heartbeat_valid = t->heartbeat_pending = true;
		t->last_live = now;
		break;
	}
	default: break;
	}
}

/* Both distances are bounded by one live window. This also handles ffff -> 0000. */
static bool within(uint16_t value, uint16_t base, uint16_t limit)
{
	return (uint16_t)(value - base) <= (uint16_t)(limit - base);
}

void tx_ack(struct transmitter *t, const struct gp_frame *f, uint32_t now)
{
	if (f->type == GP_OFFER && t->phase == DISCOVERY && f->data.hello_nonce == t->nonce) {
		t->session = f->session;
		t->phase = OFFERED;
		return;
	}
	if (!t->session || f->session != t->session) return;
	if (f->type == GP_RESET) {
		tx_reset(t, f->data.reset_reason);
		t->notify_reset = false; /* RX already discarded this session. */
		return;
	}
	if (f->type != GP_STATUS || !t->sync_sent) return;
	const struct gp_status *ack = &f->data.status;
	struct gp_status *s = &t->status;
	if ((ack->flags & (GP_TX_ACCEPT_VALID | GP_RX_ACCEPT_VALID)) != GP_RX_ACCEPT_VALID ||
	    !within(ack->rx_accept, s->rx_accept, t->sent)) return;
	if ((ack->flags & GP_USB_DONE_VALID) &&
	    !within(ack->usb_done, s->usb_done, ack->rx_accept)) return;
	t->last_peer = now;
	s->flags |= GP_RX_ACCEPT_VALID;
	s->rx_accept = ack->rx_accept;
	if (t->replaying) {
		if (s->rx_accept == t->replay_end) t->replaying = false;
		else if (!within(t->replay_next, (uint16_t)(s->rx_accept + 1), t->replay_end))
			t->replay_next = (uint16_t)(s->rx_accept + 1);
	}
	if (ack->flags & GP_USB_DONE_VALID) {
		if (!(s->flags & GP_USB_DONE_VALID) || ack->usb_done != s->usb_done)
			t->last_progress = now;
		s->flags |= GP_USB_DONE_VALID;
		s->usb_done = ack->usb_done;
		t->phase = ACTIVE;
	}
	if (ack->flags & GP_LED_VALID) {
		uint16_t advance = (uint16_t)(ack->led_gen - s->led_gen);
		if (!(s->flags & GP_LED_VALID) || advance < 0x8000) {
			s->flags |= GP_LED_VALID;
			s->led_gen = ack->led_gen;
			s->led_mode = ack->led_mode;
		}
	}
}

struct gp_frame tx_reply(const struct transmitter *t)
{
	if (t->phase < OFFERED) return t->reset;
	if (t->phase == OFFERED)
		return (struct gp_frame){t->session, GP_OFFER, {.hello_nonce = t->nonce}};
	return (struct gp_frame){t->session, GP_STATUS, {.status = t->status}};
}

bool tx_schedule(struct transmitter *t, uint32_t now_us, bool (*send)(const struct gp_frame *))
{
	uint32_t elapsed = now_us - t->last_air_us;
	struct gp_status *s = &t->status;
	struct gp_frame f = {.session = t->session};
	bool waiting = t->phase == SYNCING || (t->phase == ACTIVE && s->tx_accept != s->usb_done);
	if (t->notify_reset) {
		f = t->reset;
	} else if (t->phase == DISCOVERY || t->phase == OFFERED) {
		if (elapsed < GP_POLL_TARGET_US) return false;
		f = (struct gp_frame){0, GP_HELLO, {.hello_nonce = t->nonce}};
	} else if (t->phase < SYNCING) {
		return false;
	} else if (t->heartbeat_pending && t->sync_sent) {
		f.type = GP_HEARTBEAT;
		f.data.heartbeat = t->heartbeat;
	} else if (t->phase == SYNCING && !(s->flags & GP_RX_ACCEPT_VALID)) {
		if (t->sync_sent && elapsed < GP_POLL_TARGET_US) return false;
		f.type = GP_SYNC;
		f.data.input = t->sync;
	} else if (t->replaying && within(t->replay_next, (uint16_t)(s->rx_accept + 1), t->sent) &&
		   t->sent != s->rx_accept) {
		f.type = GP_INPUT;
		f.data.input = t->window[t->replay_next % GP_WINDOW];
	} else if (t->phase == ACTIVE && t->sent != s->tx_accept) {
		f.type = GP_INPUT;
		f.data.input = t->window[(uint16_t)(t->sent + 1) % GP_WINDOW];
	} else {
		if (!waiting || elapsed < GP_POLL_TARGET_US) return false;
		if (s->rx_accept != t->sent) {
			f.type = GP_INPUT;
			f.data.input = t->window[(uint16_t)(s->rx_accept + 1) % GP_WINDOW];
		} else {
			f.type = GP_POLL;
		}
	}
	if (!send(&f)) return false;
	if (waiting && elapsed > GP_POLL_LIMIT_US) t->poll_overdue++;
	t->last_air_us = now_us;
	if (f.type == GP_RESET) t->notify_reset = false;
	if (f.type == GP_SYNC) t->sync_sent = true;
	if (f.type == GP_HEARTBEAT) t->heartbeat_pending = false;
	if (f.type == GP_INPUT) {
		uint16_t seq = f.data.input.seq;
		if (seq == (uint16_t)(t->sent + 1)) {
			t->sent = seq;
			t->replaying = false;
		} else {
			/* RX discarded later records after a gap. Replay that suffix in
			 * order without waiting an ACK round-trip for every missing record. */
			if (!t->replaying) t->replay_end = t->sent;
			t->replaying = seq != t->replay_end;
			t->replay_next = (uint16_t)(seq + 1);
		}
	}
	return true;
}
