/* Exercise the same state generators and display buffers compiled into the UF2. */
#include "config.h"
#include "display.h"
#include <assert.h>
#include <stdio.h>
#define K(i) (1u<<(i))
static struct step step(uint32_t bits,unsigned hold,unsigned gap)
{
    return (struct step){{bits,bits>>8,bits>>16},{hold,hold>>8},{gap,gap>>8}};
}
static void turbo(struct effects fx)
{
    for (unsigned hz=5;hz<=30;hz++) {
        struct playback s={0};
        fx.settings.hz=hz; fx.settings.allow[1]=K(A-8);
        uint32_t base=UINT32_MAX-22, previous=0, rising=0;
        for (unsigned ms=0;ms<1000;ms++) {
            uint32_t out=effects_tick(&s,&fx,K(A)|K(B),base+ms);
            assert((out & K(B)) && s.pulsing);
            rising+=!!(out & ~previous & K(A)); previous=out;
        }
        assert(rising==hz);
        uint32_t t=base+1000;
        assert(effects_tick(&s,&fx,K(TURBO_KEY)|K(A),t)==0);
        assert(effects_tick(&s,&fx,K(A),++t)==0); /* swallow the entire chord */
        effects_tick(&s,&fx,0,++t);
        for (unsigned j=0;j<220;j++) assert(effects_tick(&s,&fx,K(A),++t)==K(A) && !s.pulsing);
        effects_tick(&s,&fx,K(TURBO_KEY),++t);
        effects_tick(&s,&fx,K(TURBO_KEY)|K(A),++t);
        effects_tick(&s,&fx,0,++t);
        assert(effects_tick(&s,&fx,K(A),++t)==K(A) && s.pulsing);
    }
}
static void macros(struct effects fx)
{
    struct playback s={0};
    for (unsigned i=0;i<6;i++) {
        struct macro *m=&fx.macros[i]; m->mode=i%3;m->count=2;
        m->steps[0]=step(K(i)|K(LT),3,2);
        m->steps[1]=step(K(A),1,1);
    }
    assert(effects_valid(&fx));
    uint32_t t=UINT32_MAX-2, triggers=((1u<<6)-1)<<MACRO_1;
    assert(effects_tick(&s,&fx,triggers,t)==(63|K(LT)) && s.playing==63);
    assert(effects_tick(&s,&fx,triggers,t+3)==0);
    assert(effects_tick(&s,&fx,triggers,t+5)==K(A));
    assert(!effects_tick(&s,&fx,triggers,t+6));
    assert(effects_tick(&s,&fx,triggers,t+7)==(K(1)|K(4)|K(LT)) && s.playing==18);
    assert(effects_tick(&s,&fx,K(B),t+8)==K(B) && !s.playing);
    uint32_t cancel=K(MACRO_1+5);
    effects_tick(&s,&fx,cancel,100); effects_tick(&s,&fx,0,101);
    assert(!effects_tick(&s,&fx,cancel,102) && !s.playing);
    effects_tick(&s,&fx,0,106); effects_tick(&s,&fx,cancel,107); effects_tick(&s,&fx,0,108);
    assert(effects_tick(&s,&fx,cancel,114)==(K(5)|K(LT))); /* fresh edge at period boundary */
    struct profile p={.socd=SOCD_NEUTRAL}; struct engine engine={0};
    fx.macros[0].steps[0]=step(K(LEFT)|K(LT),2,0);
    uint32_t logical=effects_tick(&(struct playback){0},&fx,K(MACRO_1)|K(RIGHT),1);
    struct gp_input pad=engine_output(&engine,&p,logical);assert(!pad.buttons && pad.lt==255);
}
static void power_and_frame(struct effects fx)
{
    struct settings *s=&fx.settings;
    struct lighting l; struct indicators v={0};
    lights_plan(s,&v,0,0,false,&l);assert(!l.powered);
    for (unsigned i=1;i<=4;i++) {
        assert(player_number(true,i+1,1)==i && player_number(true,i+5,1)==i);
        assert(player_number(true,-1,i)==i && player_number(true,0,i)==0 && !player_number(false,8,i));
    }
    s->accessory=BINARY_LED;
    for (unsigned player=0;player<5;player++) for (unsigned bits=0;bits<4;bits++) {
        v.player=player;s->led_map[player]=bits;
        lights_plan(s,&v,0,UINT32_MAX,false,&l);
        assert(l.binary==bits && l.powered==!!bits && !l.oled && !l.rgb_lit);
    }
    for (unsigned controller=SSD1306;controller<=SH1106;controller++) {
        s->accessory=controller;
        lights_plan(s,&v,0,29999,false,&l);assert(l.powered && l.oled);
        lights_plan(s,&v,0,30000,false,&l);assert(!l.powered);
    }
    s->count=18;s->brightness=64;memset(s->color,255,3);s->rgb_mode=RGB_STATIC;
    lights_plan(s,&v,0,30000,false,&l);
    unsigned sum=0;
    for (unsigned i=0;i<18;i++) for (unsigned c=0;c<3;c++) sum+=l.rgb[i][c];
    assert(l.powered && !l.oled && sum>0 && sum*20 <= (RGB_BUDGET_MA-18)*255);
    lights_plan(s,&v,0,0,true,&l);assert(!l.powered && !l.oled && !l.rgb_lit);
    s->rgb_mode=RGB_KEYS;lights_plan(s,&v,0,30000,false,&l);assert(!l.powered);
    v.keys=K(17);lights_plan(s,&v,0,30000,false,&l);assert(l.rgb[17][0] && !l.rgb[0][0]);
    s->rgb_mode=RGB_BREATHE;lights_plan(s,&v,0,30000,false,&l);assert(!l.powered);
    lights_plan(s,&v,1000,30000,false,&l);assert(l.rgb_lit);
    s->rgb_mode=RGB_RAINBOW;lights_plan(s,&v,0,30000,false,&l);
    assert(l.rgb[0][0]>0 && !l.rgb[0][1] && l.rgb[9][1]);
    uint8_t idle[1024],frame[1024];v=(struct indicators){0};oled_frame(idle,&v);
    for (unsigned i=0;i<18;i++) {
        v.keys=K(i);oled_frame(frame,&v);unsigned set=0;
        for (unsigned j=0;j<1024;j++) {
            assert((idle[j] & frame[j])==idle[j]);
            set+=__builtin_popcount((unsigned)(idle[j]^frame[j]));
        }
        assert(set==35); /* each held key fills its own 7x5 interior */
    }
    v.keys=0;v.socd=3;v.direction=1;v.player=4;v.wired=true;
    v.turbo=true;v.macros=63;v.mv=4200;oled_frame(frame,&v);
    assert(!memcmp(frame,idle,6*128));
    assert(memcmp(frame+6*128,idle+6*128,128) && memcmp(frame+7*128,idle+7*128,128));
}
static int reject_save(const struct configuration *cfg) { (void)cfg;return -1; }
static void transaction(void)
{
    struct configuration live;config_defaults(&live);
    struct configuration old=live;
    struct editor e={0};uint8_t q[64]={'H','X',1,CMD_BEGIN,2},r[64];
    config_command(&e,&live,q,r,reject_save);
    q[3]=CMD_FX_STAGE;q[7]=6; q[10]=ONCE;q[11]=1;
    config_command(&e,&live,q,r,reject_save);
    q[3]=CMD_COMMIT;config_command(&e,&live,q,r,reject_save);
    assert(r[6]==2 && !memcmp(&live,&old,sizeof(live))); /* zero hold invalid */
    q[3]=CMD_FX_STAGE;q[15]=1;config_command(&e,&live,q,r,reject_save);
    q[3]=CMD_COMMIT;config_command(&e,&live,q,r,reject_save);
    assert(r[6]==3 && !memcmp(&live,&old,sizeof(live)));
}
int main(void)
{
    struct effects fx;effects_defaults(&fx);assert(effects_valid(&fx));
    turbo(fx);macros(fx);power_and_frame(fx);transaction();
    puts("Turbo 5-30 Hz, six macros, wrap, SOCD, player LEDs, OLED, RGB budget and power gating passed");
}
