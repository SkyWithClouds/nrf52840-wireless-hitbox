/* SPDX-License-Identifier: MIT */
#pragma once
#include "match.h"
enum { CHANNELS=79, PAGE_CHANNELS=40, PAIR_LIMIT=55, PAIR_WINDOW=15000 };
/* Larger RSSISAMPLE magnitudes mean weaker interference. Weight nearby MHz
 * within a Wi-Fi half-channel, normalizing truncated bands at the edges. */
static inline uint8_t quiet_channel(const uint8_t peak[CHANNELS],uint8_t current)
{
    unsigned best=0; uint8_t channel=current;
    for (int i=0;i<CHANNELS;i++) {
        unsigned sum=0, weights=0;
        for (int j=i-10;j<=i+10;j++) if (j>=0 && j<CHANNELS) {
            unsigned w=11-(j>i?j-i:i-j);
            sum+=w*peak[j]; weights+=w;
        }
        unsigned score=128*peak[i]+128*sum/weights;
        if (score>best || (score==best && i+2==current)) { best=score;channel=i+2; }
    }
    return channel;
}
static inline bool survey_page(uint8_t out[64],unsigned page,const uint8_t peak[CHANNELS],uint8_t chosen)
{
    if (page>1) return false;
    unsigned start=page*PAGE_CHANNELS,n=page?39:40;
    out[7]=page;out[8]=chosen;out[9]=start+2;out[10]=n;
    memcpy(out+11,peak+start,n);return true;
}
enum pair_op { FIND=1, OFFER, CONFIRM, READY, SAVED, DONE, FINAL, RECEIPT };
/* Management datagrams have a distinct length and address. Each peer pins
 * a fresh nonce for the lifetime of one explicitly opened window. */
struct pair_packet { uint8_t magic[3],op,tx[8],rx[8],base[4],channel; };
struct pairing {
    struct pair_packet msg;
    uint32_t start;
    bool receiver,stored,finished;
};
static inline bool near_packet(const struct pair_packet *p,int magnitude)
{
    return magnitude>0 && magnitude<PAIR_LIMIT && !memcmp(p->magic,"HB3",3) &&
           p->op>=FIND && p->op<=RECEIPT;
}
static inline bool pair_expired(const struct pairing *p,uint32_t now) {return now-p->start>=PAIR_WINDOW;}
static inline bool pair_accept(struct pairing *p,const struct pair_packet *in,int rssi)
{
    if (!near_packet(in,rssi)) return false;
    struct pair_packet *m=&p->msg;
    if (p->receiver && in->op==FIND && !m->op) {
        memcpy(m->tx,in->tx,8);m->op=OFFER;return true;
    }
    if (memcmp(m->tx,in->tx,8)) return false;
    if (p->receiver && in->op==FIND && m->op==OFFER) return true;
    if (!p->receiver && m->op==FIND && in->op==OFFER && in->channel>=2 && in->channel<=80) {
        *m=*in;m->op=CONFIRM;return true;
    }
    if (memcmp(m->rx,in->rx,8) || memcmp(m->base,in->base,4) || m->channel!=in->channel) return false;
    if (p->receiver && in->op==CONFIRM && m->op==OFFER) m->op=READY;
    if (!p->receiver && in->op==READY && m->op==CONFIRM) return true;
    if (p->receiver && in->op==SAVED && m->op==READY) return true;
    if (!p->receiver && in->op==DONE && m->op==SAVED) m->op=FINAL;
    if (p->receiver && in->op==FINAL && m->op==DONE) m->op=RECEIPT;
    if (!p->receiver && in->op==RECEIPT && m->op==FINAL) p->finished=true;
    return true;
}

/* Keep the journal when restoring the previous network fails: boot retries it. */
static inline unsigned finish_network(bool success,bool provisional,struct match previous,
                                      int (*persist)(struct match),int (*clear_journal)(void))
{
    if (!success && provisional && persist(previous)) return 4;
    if (clear_journal()) return 4;
    return success?2:3;
}
