/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>
enum { MACRO_COUNT=6, STEP_COUNT=7, OUTPUT_COUNT=18, RGB_COUNT=18,
       RGB_BRIGHTNESS_MAX=64, RGB_BUDGET_MA=100 };
#define OUTPUT_MASK ((1u << OUTPUT_COUNT) - 1)
enum accessory_mode { ACCESS_OFF, SSD1306, SH1106, BINARY_LED };
enum rgb_mode { RGB_OFF, RGB_STATIC, RGB_KEYS, RGB_BREATHE, RGB_RAINBOW };
enum play_mode { ONCE, WHILE_HELD, PRESS_CANCEL };
/* Byte-only records shared by flash and HID. Integers use little endian. */
struct settings {
    uint8_t accessory, idle_sec, led_map[5], hz, allow[3];
    uint8_t rgb_mode, count, brightness, color[3], oled_address;
};
struct step { uint8_t keys[3], hold[2], gap[2]; };
struct macro { uint8_t mode, count; struct step steps[STEP_COUNT]; };
struct effects { struct settings settings; struct macro macros[MACRO_COUNT]; };
struct playback {
    uint32_t previous, enabled, suppressed, pressed_at[OUTPUT_COUNT];
    uint32_t started[MACRO_COUNT];
    uint8_t playing;
    bool initialized, pulsing;
};
uint32_t action_mask(const uint8_t bytes[3]);
void effects_defaults(struct effects *fx);
bool effects_valid(const struct effects *fx);
uint32_t effects_tick(struct playback *state,const struct effects *fx,uint32_t logical,uint32_t now);
