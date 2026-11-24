/* SPDX-License-Identifier: MIT */
#include "engine.h"
#define BIT_AT(i) (1u << (i))
uint32_t action_mask(const uint8_t b[3])
{
    return b[0] | ((uint32_t)b[1]<<8) | ((uint32_t)b[2]<<16);
}
static unsigned ms16(const uint8_t b[2]) { return b[0] | ((unsigned)b[1]<<8); }
void effects_defaults(struct effects *fx)
{
    *fx=(struct effects){.settings={.idle_sec=30,.led_map={0,1,2,3,0},.hz=12,
        .count=1,.brightness=16,.color={0,200,100},.oled_address=0x3c}};
}
bool effects_valid(const struct effects *fx)
{
    const struct settings *s=&fx->settings;
    if (s->accessory>BINARY_LED || (s->idle_sec!=30 && s->idle_sec!=60) ||
        s->hz<5 || s->hz>30 || action_mask(s->allow)>OUTPUT_MASK ||
        s->rgb_mode>RGB_RAINBOW || !s->count || s->count>RGB_COUNT ||
        s->brightness>RGB_BRIGHTNESS_MAX || (s->oled_address!=0x3c && s->oled_address!=0x3d)) return false;
    for (unsigned i=0;i<5;i++) if (s->led_map[i]>3) return false;
    for (unsigned i=0;i<MACRO_COUNT;i++) {
        const struct macro *m=&fx->macros[i];
        if (m->mode>PRESS_CANCEL || m->count>STEP_COUNT) return false;
        for (unsigned j=0;j<m->count;j++)
            if (!ms16(m->steps[j].hold) || action_mask(m->steps[j].keys)>OUTPUT_MASK) return false;
    }
    return true;
}
static uint32_t macro_tick(struct playback *s,const struct macro *m,unsigned i,
                           uint32_t pressed,uint32_t rising,uint32_t now)
{
    const uint32_t trigger=BIT_AT(MACRO_1+i), bit=BIT_AT(i);
    if (!m->count) return 0;
    if (!(s->playing & bit) && !(rising & trigger)) return 0;
    unsigned period=0;
    for (unsigned j=0;j<m->count;j++) period+=ms16(m->steps[j].hold)+ms16(m->steps[j].gap);
    if (m->mode!=WHILE_HELD && now-s->started[i]>=period) s->playing &= ~bit;
    if (rising & trigger) {
        if (!(s->playing & bit)) { s->playing |= bit; s->started[i]=now; }
        else if (m->mode==PRESS_CANCEL) s->playing &= ~bit;
    }
    if (m->mode==WHILE_HELD && !(pressed & trigger)) s->playing &= ~bit;
    if (!(s->playing & bit)) return 0;
    uint32_t elapsed=now-s->started[i];
    if (elapsed>=period) { elapsed%=period; s->started[i]=now-elapsed; }
    for (unsigned j=0;j<m->count;j++) {
        const struct step *step=&m->steps[j];
        unsigned hold=ms16(step->hold), span=hold+ms16(step->gap);
        if (elapsed<hold) return action_mask(step->keys);
        if (elapsed<span) return 0;
        elapsed-=span;
    }
    return 0;
}
uint32_t effects_tick(struct playback *s,const struct effects *fx,uint32_t logical,uint32_t now)
{
    uint32_t allow=action_mask(fx->settings.allow), rising=logical & ~s->previous;
    if (!s->initialized) { s->enabled=allow; s->initialized=true; }
    s->previous=logical;
    s->suppressed &= logical;
    if (logical & BIT_AT(TURBO_KEY)) {
        uint32_t toggle=rising & allow;
        if (rising & BIT_AT(TURBO_KEY)) toggle|=logical & allow;
        s->enabled ^= toggle;
        s->suppressed |= toggle;
    }
    uint32_t out=logical & OUTPUT_MASK & ~s->suppressed, pulse=out & s->enabled;
    s->pulsing=pulse!=0;
    for (unsigned i=0;i<OUTPUT_COUNT;i++) {
        uint32_t bit=BIT_AT(i);
        if (!(pulse & bit)) continue;
        if (rising & bit) s->pressed_at[i]=now;
        unsigned phase=((now-s->pressed_at[i])%1000)*fx->settings.hz%1000;
        if (phase>=500) out &= ~bit;
    }
    for (unsigned i=0;i<MACRO_COUNT;i++) out |= macro_tick(s,&fx->macros[i],i,logical,rising,now);
    return out;
}
