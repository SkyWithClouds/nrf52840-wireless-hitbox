/* SPDX-License-Identifier: MIT */
#include "air.h"
#include "hardware.h"
#include "uplink.h"
#include <esb.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
static struct uplink radio;
int radio_open(void)
{
    radio = (struct uplink){0};
    tx_reset(&radio.tx, GP_REBOOT);
    return air_start(false);
}
void radio_step(const struct gp_input *pad, uint32_t us, uint32_t ms)
{
    uplink_step(&radio,pad,us,ms,air_nonce,air_receive,air_send);
}
void radio_close(void)
{
    /* Give the receiver an immediate neutral barrier. Its watchdog also covers
     * an unplug/busy radio or a lost final RESET. */
    if (radio.tx.session) {
        struct gp_frame reset = {radio.tx.session, GP_RESET, {.reset_reason=GP_USB_RESET}};
        if (air_send(&reset)) k_sleep(K_MSEC(2));
    }
    esb_disable();
    onoff_release(z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF));
}

void radio_indicators(struct indicators *v,unsigned slot)
{
    struct source *s=&radio.keys;
    v->player=player_number(s->active,s->led_valid ? s->led_mode : -1,slot);
    if (s->active && s->led_valid) {
        v->lamp_session=s->session; v->lamp_gen=s->led_gen;
        if ((!s->applied_valid || s->applied_gen!=s->led_gen) &&
            accessory_delivered(s->session,s->led_gen)) source_led_done(s,s->session,s->led_gen);
    }
}
