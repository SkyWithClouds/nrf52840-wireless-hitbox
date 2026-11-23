/* SPDX-License-Identifier: MIT */
#include "console_usb.h"

void box_submitted(struct box_usb *u, const struct box_pad *p, uint8_t ep, uint16_t bytes)
{
	if (!u->capture || u->outstanding || p->delivered || !(ep & 0x80) || ep == 0x80) return;
	u->tag = p->tag;
	u->seq = p->state.seq;
	u->ep = ep;
	u->bytes = bytes;
	u->outstanding = true;
}
void box_finished(struct box_usb *u, struct box_pad *p, uint8_t ep, bool ok, uint32_t bytes)
{
	if (!u->outstanding || u->ep != ep) return;
	u->outstanding = false;
	box_complete(p, u->tag, u->seq, ok && bytes == u->bytes);
}
