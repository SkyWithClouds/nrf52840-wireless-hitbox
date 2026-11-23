/* SPDX-License-Identifier: MIT */
#pragma once
#include "source.h"
#include "transmitter.h"
struct uplink { struct source keys; struct transmitter tx; };
void uplink_step(struct uplink *u,const struct gp_input *pad,uint32_t us,uint32_t ms,
                 uint64_t (*nonce)(void),bool (*receive)(struct gp_frame *),
                 bool (*send)(const struct gp_frame *));
