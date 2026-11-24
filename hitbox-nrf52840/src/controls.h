/* SPDX-License-Identifier: MIT */
#pragma once
#include "engine.h"
#define MENU_BITS ((1u<<START)|(1u<<BACK)|(1u<<GUIDE))
enum gesture { TOGGLE_LOCK=1, OPEN_PAIR, ENTER_UF2 };
struct chord { uint32_t start, mask; unsigned kind; bool armed; };
/* AUX doubles as Fn. Require the full chord continuously, then release all keys. */
static inline unsigned chord_tick(struct chord *g,uint32_t keys,uint32_t ms,bool wired)
{
    uint32_t mask[]={0,(1u<<AUX)|(1u<<START)|(1u<<BACK),(1u<<AUX)|(1u<<LS_CLICK)|(1u<<RS_CLICK),
                      (1u<<AUX)|(1u<<BACK)|(1u<<GUIDE)};
    if (!keys) {
        unsigned result=g->armed && (g->kind!=ENTER_UF2 || wired) ? g->kind : 0;
        *g=(struct chord){0}; return result;
    }
    if (g->armed) { if (keys & ~g->mask || (g->kind==ENTER_UF2 && !wired)) g->armed=false; else return 0; }
    unsigned i=1;
    while (i<=3 && keys!=mask[i]) i++;
    if (i>3 || (i==ENTER_UF2 && !wired)) {g->kind=0;return 0;}
    if (g->kind!=i) {g->kind=i;g->start=ms;g->mask=keys;}
    g->armed=ms-g->start >= (i==ENTER_UF2?5000u:2000u);
    return 0;
}

static inline struct gp_input gameplay(struct engine *e,const struct profile *p,struct playback *fx,
                                       const struct configuration *cfg,bool chord,uint32_t ms)
{
    e->neutral=cfg->locked && cfg->neutral;
    uint32_t logical=e->logical;
    if (chord || (logical & (1u<<AUX))) logical &= ~MENU_BITS;
    if (cfg->locked) {
        *fx=(struct playback){.previous=logical};
        logical &= ~MENU_BITS;
    } else logical=effects_tick(fx,&cfg->fx,logical,ms);
    return engine_output(e,p,logical);
}
