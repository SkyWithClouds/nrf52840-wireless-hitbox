/* SPDX-License-Identifier: MIT */
#ifndef BRIDGE_AIR_H
#define BRIDGE_AIR_H
#include "protocol.h"
int air_start(bool receiver);
bool air_send(const struct gp_frame *frame);
bool air_receive(struct gp_frame *frame);
uint64_t air_nonce(void);
bool air_send_pipe(uint8_t pipe,const struct gp_frame *frame);
bool air_receive_pipe(uint8_t *pipe,struct gp_frame *frame);
#endif
