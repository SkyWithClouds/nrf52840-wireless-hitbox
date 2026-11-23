/* SPDX-License-Identifier: MIT */
#include "source.h"

static void restart(struct source *s, enum gp_reset_reason reason)
{
	if (s->session)
		s->reset = (struct gp_frame){s->session, GP_RESET, {.reset_reason = reason}};
	s->last_reason = reason;
	s->resets++;
	s->nonce = s->session = 0;
	s->produced = s->sent = s->accepted = s->rx_accept = s->done = 0;
	s->active = s->sync_sent = s->tx_valid = s->early_poll = false;
	s->applied_valid = s->led_valid = false;
}

void source_detach(struct source *s, enum gp_reset_reason reason)
{
	restart(s, reason);
	s->have_input = false;
	s->current = (struct gp_input){0};
}

static bool same(const struct gp_input *a, const struct gp_input *b)
{
	return a->buttons == b->buttons && a->lt == b->lt && a->rt == b->rt &&
	       a->lx == b->lx && a->ly == b->ly && a->rx == b->rx && a->ry == b->ry;
}

void source_input(struct source *s, const struct gp_input *input, uint32_t us)
{
	if (s->have_input && same(input, &s->current)) return;
	s->current = *input;
	s->current.seq = 0;
	s->have_input = true;
	s->changes++;
	if (!s->session) return;
	unsigned buffered = (uint16_t)(s->produced - s->done);
	if (buffered + !s->active >= GP_QUEUE_SIZE) {
		restart(s, GP_OVERFLOW);
		return;
	}
	if (!buffered && s->active) s->last_progress = us;
	uint16_t seq = ++s->produced;
	struct gp_input *slot = &s->queue[seq % GP_QUEUE_SIZE];
	*slot = s->current;
	slot->seq = seq;
}

void source_tick(struct source *s, uint32_t us)
{
	const uint32_t timeout = GP_TIMEOUT_MS * 1000u;
	if (!s->session) return;
	bool pending = !s->active || s->done != s->produced;
	if (us - s->last_status >= timeout || (pending && us - s->last_progress >= timeout))
		restart(s, GP_TIMEOUT);
}

static bool within(uint16_t value, uint16_t base, uint16_t upper)
{
	return (uint16_t)(value - base) <= (uint16_t)(upper - base);
}

void source_reply(struct source *s, uint32_t us, const struct gp_frame *f)
{
	if (!s->have_input) return;
	if (!s->session && s->nonce && f->type == GP_OFFER && f->data.hello_nonce == s->nonce) {
		s->session = f->session;
		s->sync = s->current;
		s->last_status = s->last_progress = s->last_heartbeat = us;
		return;
	}
	if (!s->session || f->session != s->session) return;
	if (f->type == GP_RESET) {
		restart(s, f->data.reset_reason);
		s->reset = (struct gp_frame){0}; /* TX has already reset. */
		return;
	}
	if (f->type != GP_STATUS || !s->sync_sent) return;
	const struct gp_status *a = &f->data.status;
	if (!(a->flags & GP_TX_ACCEPT_VALID) || !within(a->tx_accept, s->accepted, s->sent)) return;
	if ((a->flags & GP_RX_ACCEPT_VALID) && !within(a->rx_accept, s->rx_accept, a->tx_accept)) return;
	if ((a->flags & GP_USB_DONE_VALID) && (!(a->flags & GP_RX_ACCEPT_VALID) ||
	    !within(a->usb_done, s->done, a->rx_accept))) return;
	s->last_status = us;
	s->accepted = a->tx_accept;
	s->tx_valid = true;
	if (a->flags & GP_RX_ACCEPT_VALID) s->rx_accept = a->rx_accept;
	if (a->flags & GP_USB_DONE_VALID) {
		if (!s->active || a->usb_done != s->done) s->last_progress = us;
		s->done = a->usb_done;
		s->active = true;
	}
	if ((a->flags & GP_LED_VALID) &&
	    (!s->led_valid || (uint16_t)(a->led_gen - s->led_gen) < 0x8000)) {
		s->led_valid = true;
		s->led_gen = a->led_gen;
		s->led_mode = a->led_mode;
	}
}

void source_led_done(struct source *s, uint64_t session, uint16_t generation)
{
	if (session && session == s->session) {
		s->applied_gen = generation;
		s->applied_valid = true;
	}
}

bool source_next(struct source *s, uint32_t us, uint64_t (*nonce)(void), struct gp_frame *f)
{
	uint32_t elapsed = us - s->last_spi;
	bool due = elapsed >= GP_POLL_TARGET_US;
	*f = (struct gp_frame){.session = s->session};
	if (s->reset.type == GP_RESET) {
		*f = s->reset;
		s->reset = (struct gp_frame){0};
	} else if (!s->have_input) {
		return false;
	} else if (!s->session) {
		if (s->nonce && !due) return false;
		if (!s->nonce) s->nonce = nonce();
		f->type = GP_HELLO;
		f->data.hello_nonce = s->nonce;
	} else if (s->sync_sent && us - s->last_heartbeat >= GP_HEARTBEAT_MS * 1000u) {
		f->type = GP_HEARTBEAT;
		f->data.heartbeat = (struct gp_heartbeat){++s->heartbeat, s->applied_valid,
			s->applied_valid ? s->applied_gen : 0};
		s->last_heartbeat = us;
	} else if (!s->tx_valid) {
		if (s->sync_sent && !due) return false;
		f->type = GP_SYNC;
		f->data.input = s->sync;
		s->sync_sent = true;
	} else if (s->active && s->accepted != s->produced &&
		   (uint16_t)(s->accepted - s->done) < GP_WINDOW) {
		uint16_t next = (uint16_t)(s->accepted + 1);
		if (next == s->sent && !due) {
			/* Another physical change is waiting. Fetch the preloaded acceptance
			 * once on READY so a busy 1 kHz source need not idle a whole ms. */
			if (s->produced == s->sent || s->early_poll) return false;
			f->type = GP_POLL;
			s->early_poll = true;
		} else {
			f->type = GP_INPUT;
			f->data.input = s->queue[next % GP_QUEUE_SIZE];
			s->sent = next;
			s->early_poll = false;
		}
	} else {
		if (!due || (s->active && s->done == s->produced)) return false;
		f->type = GP_POLL;
	}
	if (s->session && (!s->active || s->sent != s->done) && elapsed > GP_POLL_LIMIT_US &&
	    us - s->last_progress > GP_POLL_LIMIT_US) s->overdue++;
	s->last_spi = us;
	return true;
}
