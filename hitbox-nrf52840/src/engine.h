/* SPDX-License-Identifier: MIT */
#pragma once
#include "protocol.h"
#include "effects.h"
enum { KEY_COUNT=18, PROFILE_COUNT=4, ACTION_COUNT=27 };
enum action {
    UP, DOWN, LEFT, RIGHT, START, BACK, LS_CLICK, RS_CLICK, LB, RB,
    GUIDE, AUX, A, B, X, Y, LT, RT, UNBOUND, NEXT_PROFILE, TURBO_KEY, MACRO_1
};
enum { SOCD_NEUTRAL, SOCD_UP, SOCD_LAST, SOCD_FIRST };
enum { HAT, LEFT_STICK, RIGHT_STICK };
struct profile { uint8_t socd, output, debounce, map[KEY_COUNT]; };
struct configuration { uint8_t version, active, divider_x10; struct profile profiles[PROFILE_COUNT]; struct effects fx; uint8_t eager, locked, neutral; };
struct engine {
    uint32_t raw, stable, changed_ms[KEY_COUNT], logical;
    int8_t priority[2]; /* First direction held on each opposing pair. Zero means a tie. */
    bool profile_edge, eager, neutral;
};
void config_defaults(struct configuration *c);
bool profile_valid(const struct profile *p);
bool config_valid(const struct configuration *c);
void engine_keys(struct engine *e,const struct profile *p,uint32_t raw,uint32_t ms);
struct gp_input engine_output(struct engine *e,const struct profile *p,uint32_t logical);
struct gp_input engine_scan(struct engine *e, const struct profile *p, uint32_t raw, uint32_t ms);
