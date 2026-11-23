/* SPDX-License-Identifier: MIT */
#pragma once
#include "console_pad.h"
#ifdef __cplusplus
extern "C" {
#endif
struct box_usb {
	bool capture, outstanding;
	uint8_t ep;
	uint16_t bytes, seq;
	uint64_t tag;
};
void box_submitted(struct box_usb *u, const struct box_pad *p, uint8_t ep, uint16_t bytes);
void box_finished(struct box_usb *u, struct box_pad *p, uint8_t ep, bool ok, uint32_t bytes);
#ifdef __cplusplus
}
#endif
