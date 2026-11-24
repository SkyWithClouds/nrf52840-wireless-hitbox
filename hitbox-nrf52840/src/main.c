/* SPDX-License-Identifier: MIT */
#include <zephyr/kernel.h>
#include "hardware.h"
#include "match.h"
#include "controls.h"
int main(void)
{
    static struct configuration cfg;
    static struct engine keys;
    static struct playback playback;
    static struct chord gesture;
    static struct battery bat;
    static struct gp_input pad;
    int rc=board_start();
    if (rc) return rc;
    match_init(HITBOX);
    store_open(&cfg);
    rc=wired_open();
    if (rc) return rc;
    bool wireless=false, profile_latched=false, rescan=true, rebind=false;
    uint32_t activity=k_uptime_get_32(), last_battery=activity-5000, last_sample=activity-1;
    for (;;) {
        uint32_t ms=k_uptime_get_32();
        bool wired=wired_power();
        if (wired && wireless) { radio_close(); wireless=false; rescan=true; last_battery=ms-5000; }
        if (!wired && !wireless) {
            rc=radio_open(); if (rc) return rc;
            wireless=true; rescan=true; last_battery=ms-5000;
        }
        if (rescan) {
            playback=(struct playback){.previous=keys.logical};
            memset(keys.priority,0,sizeof(keys.priority));
            rescan=false; rebind=true;
            activity=ms;
        }
        const struct profile *p=&cfg.profiles[cfg.active];
        if (ms!=last_sample) {
            last_sample=ms;
            uint32_t keys_before=0;
            if (rebind) for (unsigned i=0;i<KEY_COUNT;i++) {
                if (keys.stable & (1u<<i)) keys_before |= 1u<<p->map[i];
            }
            keys.eager=cfg.eager;
            engine_keys(&keys,p,read_keys(),ms);
            if (rebind) { playback.previous=keys_before; rebind=false; }
            if (!keys.stable && !keys.raw) profile_latched=false;
            if (keys.profile_edge && !profile_latched) {
                cfg.active=(cfg.active+1)%PROFILE_COUNT;
                profile_latched=true; p=&cfg.profiles[cfg.active];
                engine_keys(&keys,p,keys.raw,ms);
                playback=(struct playback){.previous=keys.logical};
                memset(keys.priority,0,sizeof(keys.priority));
            }
            unsigned action=chord_tick(&gesture,keys.logical,ms,wired);
            if (action==TOGGLE_LOCK) {
                struct configuration updated=cfg; updated.locked^=1;
                if (!store_save(&updated)) { cfg.locked=updated.locked; rescan=true; }
            } else if (action==OPEN_PAIR) match_pair_start();
            else if (action==ENTER_UF2) match_boot_request();
            pad=gameplay(&keys,p,&playback,&cfg,gesture.kind!=0,ms);
        }
        if (keys.raw || keys.stable) activity=ms;
        if (ms-last_battery>=5000) { battery_read(&bat,cfg.divider_x10,wired); last_battery=ms; }
        if (wired) {
            if (wired_tick(&pad,&keys,&cfg,&bat,ms)) {
                rescan=true; profile_latched=true; last_battery=ms-5000;
            }
        } else radio_step(&pad,k_ticks_to_us_floor32(k_uptime_ticks()),ms);
        struct indicators view={.keys=keys.stable,.mv=bat.mv,.locked=cfg.locked,.socd=keys.neutral ? SOCD_NEUTRAL : p->socd,.direction=p->output,
            .macros=playback.playing,.turbo=playback.pulsing,.wired=wired};
        if (wired) view.player=wired_player(match_saved.slots);
        else radio_indicators(&view,match_saved.slots);
        match_service();
        bool sleep=wireless && ms-activity>=300000 && !playback.playing;
        accessory_update(&cfg.fx.settings,&view,ms,ms-activity,sleep);
        if (sleep) {
            radio_close(); wireless=false; idle_wait();
            activity=k_uptime_get_32();
        }
        k_sleep(K_USEC(200));
    }
}
