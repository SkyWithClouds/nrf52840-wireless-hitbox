/* SPDX-License-Identifier: MIT */
#include "engine.h"
#define MASK(i) (1u << (i))
void config_defaults(struct configuration *c)
{
    static const uint8_t layout[KEY_COUNT] = {
        LEFT, DOWN, RIGHT, UP, X, Y, RB, LB, A, B, RT, LT,
        BACK, START, LS_CLICK, RS_CLICK, GUIDE, AUX
    };
    *c = (struct configuration){.version=3};
    effects_defaults(&c->fx);
    for (unsigned i=0; i<PROFILE_COUNT; ++i) {
        c->profiles[i].debounce = 2;
        memcpy(c->profiles[i].map, layout, sizeof(layout));
    }
}
bool profile_valid(const struct profile *p)
{
    if (p->socd > SOCD_FIRST || p->output > RIGHT_STICK ||
        !p->debounce || p->debounce > 10) return false;
    for (unsigned i=0; i<KEY_COUNT; ++i)
        if (p->map[i] >= ACTION_COUNT) return false;
    return true;
}
bool config_valid(const struct configuration *c)
{
    if (c->divider_x10 && (c->divider_x10 < 15 || c->divider_x10 > 50)) return false;
    if (!effects_valid(&c->fx) || c->version != 3 || c->eager>1 || c->locked>1 || c->neutral>1 || c->active >= PROFILE_COUNT) return false;
    for (unsigned i=0; i<PROFILE_COUNT; ++i)
        if (!profile_valid(&c->profiles[i])) return false;
    return true;
}
static int resolve(uint32_t keys, unsigned negative, unsigned positive,
                   int8_t *first, uint8_t socd, bool vertical)
{
    bool n = keys & MASK(negative), p = keys & MASK(positive);
    if (!n || !p) {
        *first = (int)p - (int)n;
        return *first;
    }
    if (socd == SOCD_UP && vertical) return 1;
    if (socd == SOCD_FIRST) return *first;
    if (socd == SOCD_LAST) return -*first;
    return 0;
}
void engine_keys(struct engine *e, const struct profile *p, uint32_t raw, uint32_t ms)
{
    uint32_t logical=0;
    for (unsigned i=0; i<KEY_COUNT; ++i) {
        uint32_t b = MASK(i);
        if ((raw ^ e->raw) & b) e->changed_ms[i] = ms;
        if ((e->eager && (raw & b)) || ms - e->changed_ms[i] >= p->debounce)
            e->stable = (e->stable & ~b) | (raw & b);
        if (e->stable & b) logical |= MASK(p->map[i]);
    }
    e->raw = raw;
    e->profile_edge = (logical & ~e->logical & MASK(NEXT_PROFILE)) != 0;
    e->logical = logical;
}
struct gp_input engine_output(struct engine *e,const struct profile *p,uint32_t logical)
{
    int x = resolve(logical, LEFT, RIGHT, &e->priority[0], e->neutral ? SOCD_NEUTRAL : p->socd, false);
    int y = resolve(logical, DOWN, UP, &e->priority[1], e->neutral ? SOCD_NEUTRAL : p->socd, true);
    struct gp_input out = {.buttons = logical & 0xfff0,
        .lt = (logical & MASK(LT)) ? 255 : 0, .rt = (logical & MASK(RT)) ? 255 : 0};
    if (p->output == HAT)
        out.buttons |= (y>0) | ((y<0)<<1) | ((x<0)<<2) | ((x>0)<<3);
    else {
        int16_t sx = x < 0 ? INT16_MIN : x * INT16_MAX;
        int16_t sy = y < 0 ? INT16_MIN : y * INT16_MAX;
        if (p->output == LEFT_STICK) { out.lx=sx; out.ly=sy; }
        else { out.rx=sx; out.ry=sy; }
    }
    return out;
}

struct gp_input engine_scan(struct engine *e,const struct profile *p,uint32_t raw,uint32_t ms)
{
    engine_keys(e,p,raw,ms);
    return engine_output(e,p,e->logical);
}
