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

enum { RAIL=13, DATA=33, SDA=34, SCL=39 };
/* Called after releasing TWIM. nrfx cfg_default disconnects the input buffer,
 * disables output and selects NOPULL. EN must be the final write. */
static inline void isolate_rail(void (*high_z)(uint32_t),void (*clear_pin)(uint32_t))
{
    high_z(DATA);high_z(SDA);high_z(SCL);clear_pin(RAIL);
}
