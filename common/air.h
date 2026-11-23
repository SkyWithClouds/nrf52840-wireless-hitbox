/* SPDX-License-Identifier: MIT */
#ifndef BRIDGE_AIR_H
#define BRIDGE_AIR_H
#include "protocol.h"
int air_start(bool receiver);
bool air_send(const struct gp_frame *frame);
bool air_receive(struct gp_frame *frame);
uint64_t air_nonce(void);
#endif
