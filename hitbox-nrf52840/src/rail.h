/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>
enum rail_action { RAIL_IDLE, RAIL_ENABLE, RAIL_WAIT, RAIL_DRIVE, RAIL_ISOLATE_OFF };
struct rail { bool on; uint32_t raised; };
static inline enum rail_action rail_step(struct rail *r,bool want,uint32_t now)
{
    if (!want) {bool old=r->on;r->on=false;return old?RAIL_ISOLATE_OFF:RAIL_IDLE;}
    if (!r->on) {r->on=true;r->raised=now;return RAIL_ENABLE;}
    return now-r->raised<13 ? RAIL_WAIT : RAIL_DRIVE;
}
