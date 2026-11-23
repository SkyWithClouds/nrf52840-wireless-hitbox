/* SPDX-License-Identifier: MIT */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <hal/nrf_power.h>
#include "board.h"
static const uint8_t pins[KEY_COUNT] = {6,8,17,20,22,24,32,11,36,38,9,10,43,45,47,2,29,31};
static const struct device *const ports[] = {
    DEVICE_DT_GET(DT_NODELABEL(gpio0)), DEVICE_DT_GET(DT_NODELABEL(gpio1))};
static const struct device *const adc = DEVICE_DT_GET(DT_NODELABEL(adc));
static struct gpio_callback callbacks[2];
K_SEM_DEFINE(wake, 0, 1);
void wake_input(void) { k_sem_give(&wake); }
static void gpio_wake(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t mask)
{
    ARG_UNUSED(port); ARG_UNUSED(cb); ARG_UNUSED(mask);
    wake_input();
}
bool wired_power(void) { return nrf_power_usbregstatus_vbusdet_get(NRF_POWER); }

int board_start(void)
{
    uint32_t masks[2] = {0};
    if (!device_is_ready(ports[0]) || !device_is_ready(ports[1])) return -ENODEV;
    int rc = gpio_pin_configure(ports[0], 13, GPIO_OUTPUT_LOW);
    if (rc) return rc;
    for (unsigned i=0; i<KEY_COUNT; ++i) {
        unsigned port=pins[i]/32, pin=pins[i]%32;
        rc = gpio_pin_configure(ports[port], pin, GPIO_INPUT | GPIO_PULL_UP);
        if (rc) return rc;
        masks[port] |= BIT(pin);
    }
    for (unsigned i=0; i<2; ++i) {
        gpio_init_callback(&callbacks[i], gpio_wake, masks[i]);
        rc = gpio_add_callback(ports[i], &callbacks[i]);
        if (rc) return rc;
    }
    /* PORT/SENSE on all 18 pins, no GPIOTE channel per switch. */
    for (unsigned i=0; i<KEY_COUNT; ++i) {
        rc = gpio_pin_interrupt_configure(ports[pins[i]/32], pins[i]%32, GPIO_INT_EDGE_FALLING);
        if (rc) return rc;
    }
    return 0;
}
uint32_t read_keys(void)
{
    gpio_port_value_t levels[2];
    gpio_port_get_raw(ports[0], &levels[0]);
    gpio_port_get_raw(ports[1], &levels[1]);
    uint32_t keys=0;
    for (unsigned i=0; i<KEY_COUNT; ++i)
        if (!(levels[pins[i]/32] & BIT(pins[i]%32))) keys |= BIT(i);
    return keys;
}
void idle_wait(void)
{
    k_sem_reset(&wake);
    if (!read_keys() && !wired_power()) k_sem_take(&wake, K_FOREVER);
}
static int sample(unsigned input)
{
    const struct adc_channel_cfg cfg = {.gain=ADC_GAIN_1_6, .reference=ADC_REF_INTERNAL,
        .acquisition_time=ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS,40), .input_positive=input};
    int16_t value=0;
    struct adc_sequence seq={.channels=1,.buffer=&value,.buffer_size=sizeof(value),.resolution=12};
    if (!device_is_ready(adc) || adc_channel_setup(adc,&cfg) || adc_read(adc,&seq)) return 0;
    int32_t mv=value;
    if (adc_raw_to_millivolts(600,ADC_GAIN_1_6,12,&mv) || mv<0) return 0;
    return mv;
}
static bool plausible(unsigned mv) { return mv >= 2700 && mv <= 4350; }
void battery_read(struct battery *b, uint8_t divider_x10, bool wired)
{
    b->pin_mv=sample(SAADC_CH_PSELP_PSELP_AnalogInput2);
    unsigned divided = (b->pin_mv * divider_x10 + 5)/10;
    if (!wired) {
        unsigned supply = 5 * sample(SAADC_CH_PSELP_PSELP_VDDHDIV5);
        if (!wired_power()) b->last_wireless_mv = plausible(supply) ? supply : 0;
    }
    b->mv = plausible(divided) ? divided : b->last_wireless_mv;
    b->source = plausible(divided) ? 1 : !b->mv ? 0 : wired ? 3 : 2;
}
