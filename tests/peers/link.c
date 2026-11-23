/* SPDX-License-Identifier: MIT */
#include "link.h"

void link_init(struct link *l, uint64_t (*nonce)(void))
{
	*l = (struct link){.nonce = nonce};
	receiver_reset(&l->rf);
	l->barrier = nonce();
}

void link_tick(struct link *l, uint32_t ms)
{
	uint64_t old = l->rf.session;
	receiver_tick(&l->rf, ms);
	if (l->peer && ms - l->last_spi >= GP_TIMEOUT_MS) {
		link_init(l, l->nonce);
	} else if (old != l->rf.session) {
		l->barrier = l->nonce();
	}
}

struct gp_frame link_rf(struct link *l, const struct gp_frame *f, uint32_t ms)
{
	link_tick(l, ms);
	uint64_t old = l->rf.session;
	struct gp_frame reply = receiver_frame(&l->rf, f, ms, l->lamp, l->nonce);
	if (old != l->rf.session) l->barrier = l->nonce();
	return reply;
}

struct gp_frame link_spi(struct link *l, const struct gp_frame *f, uint32_t ms)
{
	link_tick(l, ms);
	if (f && f->type == GP_HELLO) {
		if (l->peer != f->data.hello_nonce) {
			link_init(l, l->nonce);
			l->peer = f->data.hello_nonce;
			l->link_id = l->nonce();
		}
		l->last_spi = ms;
		return (struct gp_frame){l->link_id, GP_OFFER, {.hello_nonce = l->peer}};
	}
	if (!l->peer) return (struct gp_frame){0, GP_RESET, {.reset_reason = GP_REBOOT}};
	struct delivery head;
	if (f && (f->type == GP_STATUS || f->type == GP_POLL) &&
	    (f->session == l->link_id || f->session == l->barrier || f->session == l->rf.session)) {
		l->last_spi = ms;
		if (f->type == GP_STATUS) {
			const struct gp_status *a = &f->data.status;
			l->lamp = (a->flags & GP_LED_VALID) ?
				(1u << 24) | ((uint32_t)a->led_gen << 8) | a->led_mode : 0;
			if ((a->flags & GP_USB_DONE_VALID) && receiver_peek(&l->rf, &head) &&
			    f->session == (head.session ? head.session : l->barrier) &&
			    head.input.seq == a->usb_done) receiver_complete(&l->rf, &head, ms);
		}
	}
	if (!receiver_peek(&l->rf, &head))
		return (struct gp_frame){.session = l->rf.session ? l->rf.session : l->link_id, .type = GP_POLL};
	if (!head.session)
		return (struct gp_frame){l->barrier, GP_RESET, {.reset_reason = GP_TIMEOUT}};
	return (struct gp_frame){head.session, l->rf.done_valid ? GP_INPUT : GP_SYNC,
				 {.input = head.input}};
}
