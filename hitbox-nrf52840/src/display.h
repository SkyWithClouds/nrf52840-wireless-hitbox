/* SPDX-License-Identifier: MIT */
#pragma once
#include "effects.h"
struct indicators {
    uint64_t lamp_session;
    uint32_t keys;
    uint16_t mv, lamp_gen;
    uint8_t player, socd, direction, macros;
    bool wired, turbo, locked;
};
struct lighting {
    uint8_t rgb[RGB_COUNT][3], binary;
    bool oled, powered, rgb_lit;
};
unsigned player_number(bool online,int lamp,unsigned slot);
void lights_plan(const struct settings *s,const struct indicators *v,uint32_t now,
                 uint32_t idle_ms,bool sleep,struct lighting *out);
void oled_frame(uint8_t frame[1024],const struct indicators *v);
