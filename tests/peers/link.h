/* SPDX-License-Identifier: MIT */
#pragma once
#include "receiver.h"
struct link {
	struct receiver rf;
	uint64_t peer, link_id, barrier;
	uint32_t lamp, last_spi;
	uint64_t (*nonce)(void);
};
void link_init(struct link *l, uint64_t (*nonce)(void));
void link_tick(struct link *l, uint32_t ms);
struct gp_frame link_rf(struct link *l, const struct gp_frame *f, uint32_t ms);
struct gp_frame link_spi(struct link *l, const struct gp_frame *f, uint32_t ms);
