/* SPDX-License-Identifier: MIT */
#pragma once
#include "board.h"
#include "config.h"
int radio_open(void);
void radio_close(void);
void radio_step(const struct gp_input *pad, uint32_t us, uint32_t ms);
int wired_open(void);
bool wired_tick(const struct gp_input *pad, const struct engine *engine,
                struct configuration *c, const struct battery *bat, uint32_t ms);
