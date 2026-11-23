/* SPDX-License-Identifier: MIT */
#include <zephyr/kernel.h>
#include "hardware.h"
int main(void)
{
    static struct configuration cfg;
    static struct engine keys;
    static struct battery bat;
    int rc=board_start();
    if (rc) return rc;
    store_open(&cfg); /* Standards-compliant defaults remain playable after a storage fault. */
    rc=wired_open();
    if (rc) return rc;
    bool wireless=false, profile_latched=false;
    uint32_t activity=k_uptime_get_32(), last_battery=activity-5000;
    for (;;) {
        uint32_t ms=k_uptime_get_32();
        bool wired=wired_power();
        uint32_t raw=read_keys();
        struct gp_input s=engine_scan(&keys,&cfg.profiles[cfg.active],raw,ms);
        if (!keys.stable && !raw) profile_latched=false;
        if (keys.profile_edge && !profile_latched) {
            cfg.active=(cfg.active+1)%PROFILE_COUNT;
            profile_latched=true;
            memset(keys.priority,0,sizeof(keys.priority));
            s=engine_scan(&keys,&cfg.profiles[cfg.active],raw,ms);
        }
        if (wired || raw || keys.stable) activity=ms;
        if (wired && wireless) { radio_close(); wireless=false; last_battery=ms-5000; }
        if (!wired && !wireless) {
            rc=radio_open(); if (rc) return rc;
            wireless=true; last_battery=ms-5000;
        }
        if (ms-last_battery>=5000) { battery_read(&bat,cfg.divider_x10,wired); last_battery=ms; }
        if (wired) {
            if (wired_tick(&s,&keys,&cfg,&bat,ms)) {
                memset(keys.priority,0,sizeof(keys.priority));
                profile_latched=true;
                last_battery=ms-5000;
            }
        } else {
            radio_step(&s,k_ticks_to_us_floor32(k_uptime_ticks()),ms);
            if (ms-activity>=300000) {
                radio_close(); wireless=false;
                idle_wait(); activity=k_uptime_get_32();
            }
        }
        k_sleep(K_USEC(200));
    }
    return 0;
}
