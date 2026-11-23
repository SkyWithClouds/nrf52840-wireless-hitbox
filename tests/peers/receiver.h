/* SPDX-License-Identifier: MIT */
#ifndef BRIDGE_RECEIVER_H
#define BRIDGE_RECEIVER_H
#include "protocol.h"
struct receiver {
	uint64_t session, nonce, candidate, candidate_nonce;
	struct gp_input queue[GP_WINDOW];
	uint8_t head, count;
	uint16_t accepted, done;
	bool done_valid, neutral, heartbeat_valid;
	uint32_t heartbeat, last_live, last_progress;
};
struct delivery {
	uint64_t session; /* zero denotes a neutral barrier */
	struct gp_input input;
};
void receiver_reset(struct receiver *r);
void receiver_tick(struct receiver *r, uint32_t now);
struct gp_frame receiver_frame(struct receiver *r, const struct gp_frame *f,
			       uint32_t now, uint32_t lamp, uint64_t (*nonce)(void));
bool receiver_peek(const struct receiver *r, struct delivery *d);
void receiver_complete(struct receiver *r, const struct delivery *d, uint32_t now);
#endif
