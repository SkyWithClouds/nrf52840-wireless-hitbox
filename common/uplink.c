/* SPDX-License-Identifier: MIT */
#include "uplink.h"
void uplink_step(struct uplink *u,const struct gp_input *pad,uint32_t us,uint32_t ms,
                 uint64_t (*nonce)(void),bool (*receive)(struct gp_frame *),
                 bool (*send)(const struct gp_frame *))
{
    struct gp_frame f;
    source_input(&u->keys,pad,us);
    source_tick(&u->keys,us);
    tx_tick(&u->tx,ms);
    while (receive(&f)) tx_ack(&u->tx,&f,ms);
    if (source_next(&u->keys,us,nonce,&f)) {
        tx_source(&u->tx,&f,ms);
        f=tx_reply(&u->tx);
        source_reply(&u->keys,us,&f);
    }
    tx_schedule(&u->tx,us,send);
}
