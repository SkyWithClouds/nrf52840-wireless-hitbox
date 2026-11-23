/* SPDX-License-Identifier: MIT */
#pragma once
#include "engine.h"
struct battery { uint16_t mv, pin_mv, last_wireless_mv; uint8_t source; };
int board_start(void);
uint32_t read_keys(void);
bool wired_power(void);
void battery_read(struct battery *b, uint8_t divider_x10, bool wired);
void idle_wait(void);
void wake_input(void);
