/* SPDX-License-Identifier: MIT */
#ifndef BRIDGE_TRANSMITTER_H
#define BRIDGE_TRANSMITTER_H
#include "protocol.h"

enum tx_phase { OFFLINE, DISCOVERY, OFFERED, SYNCING, ACTIVE };
struct transmitter {
	enum tx_phase phase;
	uint64_t nonce, session;
	struct gp_frame reset;
	bool notify_reset, sync_sent, heartbeat_pending, heartbeat_valid;
	struct gp_heartbeat heartbeat;
	struct gp_input sync, window[GP_WINDOW];
	struct gp_status status;
	uint16_t sent, replay_next, replay_end;
	bool replaying;
	uint32_t last_live, last_peer, last_progress, last_air_us, poll_overdue;
};

void tx_reset(struct transmitter *t, enum gp_reset_reason reason);
void tx_tick(struct transmitter *t, uint32_t now);
void tx_source(struct transmitter *t, const struct gp_frame *f, uint32_t now);
void tx_ack(struct transmitter *t, const struct gp_frame *f, uint32_t now);
struct gp_frame tx_reply(const struct transmitter *t);
bool tx_schedule(struct transmitter *t, uint32_t now_us, bool (*send)(const struct gp_frame *));
#endif
