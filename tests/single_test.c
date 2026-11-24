/* Two unmodified receiver implementations. Only the transport and USB host
 * clock are simulated. Production engine + single-board uplink drive both. */
#include <assert.h>
#include <stdio.h>
#include "engine.h"
#include "uplink.h"
#include "link.h"
#include "console_pad.h"
static uint64_t nonce(void) { static uint64_t n=100; return ++n; }
static struct gp_frame ack,packet;
static bool sent;
static bool send(const struct gp_frame *f) {
    uint8_t bytes[24];
    assert(gp_encode(bytes,24,f) && gp_decode(&packet,bytes,24));
    sent=true; return true;
}
static bool receive(struct gp_frame *f) {
    if(!ack.type) return false;
    uint8_t bytes[24]; assert(gp_encode(bytes,24,&ack) && gp_decode(f,bytes,24));
    ack=(struct gp_frame){0}; return true;
}
static void pair(bool console)
{
    struct configuration cfg;config_defaults(&cfg);
    struct playback fx={0};
    cfg.fx.settings.hz=25; cfg.fx.settings.allow[1]=1u<<(X-8);
    cfg.profiles[0].map[17]=MACRO_1;
    cfg.fx.macros[0]=(struct macro){.count=2,.steps={
        {{1u<<LEFT,1u<<(A-8),0},{5},{2}},{{0,1u<<(B-8),0},{3},{0}}}};
    struct engine e={0};struct uplink u={0};struct receiver pc={0};
    struct link box;struct box_pad pico={0};
    link_init(&box,nonce);box_start(&pico,nonce());receiver_reset(&pc);
    struct gp_frame staged=link_spi(&box,NULL,0), pending={0};
    uint16_t moves[32]={0};unsigned count=0;
    bool lost=false,lost_ack=false,detected_timeout=false;
    ack=(struct gp_frame){0};
    for(uint32_t us=0;us<250000;us+=200) {
        uint32_t ms=us/1000;
        bool cut=ms>=55 && ms<115;
        uint32_t raw=0;
        if(ms>=20 && ms<30) raw=1u<<8;
        if(ms>=24 && ms<27) raw|=1u<<9;
        if(ms>=40 && ms<75) raw=1u<<2;
        if(ms>=82 && ms<91) raw=1u<<8; /* must never replay after reconnect */
        if(ms>=112 && ms<160) raw=1u<<5;
        if(ms>=170 && ms<225) raw|=1u<<4;
        if(ms>=178 && ms<185) raw|=1u<<17;
        engine_keys(&e,&cfg.profiles[0],raw,ms);
        struct gp_input pad=engine_output(&e,&cfg.profiles[0],effects_tick(&fx,&cfg.fx,e.logical,ms));
        if(console) {link_tick(&box,ms);box_tick(&pico,ms,nonce);}
        else receiver_tick(&pc,ms);
        sent=false;uplink_step(&u,&pad,us,ms,nonce,receive,send);
        if(sent && !cut) {
            if(!lost && packet.type==GP_INPUT) lost=true;
            else {
                if(!lost_ack && packet.type==GP_POLL && ms>20) lost_ack=true;
                else ack=pending;
                pending=console ? link_rf(&box,&packet,ms) : receiver_frame(&pc,&packet,ms,0,nonce);
            }
        }
        if(console && us%400==0) {
            struct gp_frame mosi=box_reply(&pico);
            box_receive(&pico,&staged,ms);staged=link_spi(&box,&mosi,ms);
        }
        if(us%1000==0) {
            struct delivery d;bool got=false;
            if (console && !pico.delivered) {
                d.input=pico.state;box_complete(&pico,pico.tag,pico.state.seq,true);got=true;
            }
            if (!console && receiver_peek(&pc,&d)) {receiver_complete(&pc,&d,ms);got=true;}
            if(got && (!count || d.input.buttons!=moves[count-1])) {
                assert(count<32);moves[count++]=d.input.buttons;
                if(cut && !d.input.buttons) {assert(ms>=100 && ms<=109); detected_timeout=true;}
            }
        }
    }
    const uint16_t expected[]={0,0x1000,0x3000,0x1000,0,8,0,0x8000,0,0x4000,0x5004,0x4000,0x6000,0x4000,0,0x4000,0};
    assert(count==sizeof(expected)/sizeof(*expected) && !memcmp(moves,expected,sizeof(expected)));
    assert(lost && lost_ack && detected_timeout && u.keys.active && u.keys.produced==u.keys.done);
}
int main(void) {pair(false);pair(true);puts("Production single-board engine/uplink + both pinned receivers: ordered input, loss, timeout and reconnect passed");}
