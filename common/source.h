/* SPDX-License-Identifier: MIT */
#ifndef BRIDGE_SOURCE_H
#define BRIDGE_SOURCE_H
#include "protocol.h"

/* Owned by the main loop, including TinyUSB callbacks dispatched on that loop. */
struct source {
	struct gp_input current, sync, queue[GP_QUEUE_SIZE];
	bool have_input, sync_sent, active;
	uint64_t nonce, session;
	uint16_t produced, sent, accepted, rx_accept, done;
	bool tx_valid, early_poll;
	uint32_t last_spi, last_status, last_progress, last_heartbeat, heartbeat;
	bool led_valid, applied_valid;
	uint16_t led_gen, applied_gen;
	uint8_t led_mode;
	struct gp_frame reset;
	uint32_t changes, resets, overdue;
	enum gp_reset_reason last_reason;
};

void source_input(struct source *s, const struct gp_input *input, uint32_t us);
void source_detach(struct source *s, enum gp_reset_reason reason);
void source_tick(struct source *s, uint32_t us);
/* Call only once READY is high, immediately before asserting CS. */
bool source_next(struct source *s, uint32_t us, uint64_t (*nonce)(void), struct gp_frame *frame);
void source_reply(struct source *s, uint32_t us, const struct gp_frame *reply);
void source_led_done(struct source *s, uint64_t session, uint16_t generation);
#endif
