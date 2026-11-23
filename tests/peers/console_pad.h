/* SPDX-License-Identifier: MIT */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include "protocol.h"
struct box_pad {
	uint64_t boot, link, tag;
	struct gp_input state;
	enum gp_type kind;
	bool delivered;
	uint32_t last_rx;
	bool led_valid;
	uint8_t led_mode;
	uint16_t led_gen;
};
void box_start(struct box_pad *p, uint64_t nonce);
void box_tick(struct box_pad *p, uint32_t ms, uint64_t (*nonce)(void));
void box_receive(struct box_pad *p, const struct gp_frame *f, uint32_t ms);
struct gp_frame box_reply(const struct box_pad *p);
void box_complete(struct box_pad *p, uint64_t tag, uint16_t seq, bool success);
#ifdef __cplusplus
}
#endif
