/* SPDX-License-Identifier: MIT */
#include "display.h"
#include <string.h>
unsigned player_number(bool online,int lamp,unsigned slot)
{
    if (!online || lamp==0) return 0;
    if (lamp>=2 && lamp<=5) return lamp-1;
    if (lamp>=6 && lamp<=9) return lamp-5;
    return slot;
}
static void wheel(unsigned phase,uint8_t color[3])
{
    unsigned sector=phase/85;
    unsigned f=(phase%85)*3;
    color[sector]=255-f;
    color[(sector+1)%3]=f;
    color[(sector+2)%3]=0;
}
void lights_plan(const struct settings *s,const struct indicators *v,uint32_t now,
                 uint32_t idle_ms,bool sleep,struct lighting *out)
{
    memset(out,0,sizeof(*out));
    if (sleep) return;
    out->oled=(s->accessory==SSD1306 || s->accessory==SH1106) && idle_ms<s->idle_sec*1000u;
    if (s->accessory==BINARY_LED) out->binary=s->led_map[v->player];
    unsigned sum=0;
    for (unsigned i=0;i<s->count && s->rgb_mode!=RGB_OFF;i++) {
        if (s->rgb_mode==RGB_KEYS && !(v->keys & (1u<<i))) continue;
        uint8_t color[3]; memcpy(color,s->color,3);
        unsigned level=s->brightness;
        if (s->rgb_mode==RGB_RAINBOW) wheel(((now/12)%255+i*255/s->count)%255,color);
        if (s->rgb_mode==RGB_BREATHE) {
            unsigned phase=now%2000;
            level=level*(phase<1000 ? phase : 2000-phase)/1000;
        }
        for (unsigned c=0;c<3;c++) {
            out->rgb[i][c]=(unsigned)color[c]*level/255;
            sum+=out->rgb[i][c];
        }
    }
    /* 1 mA idle per installed pixel + 20 mA per full-scale colour channel. */
    unsigned budget=(RGB_BUDGET_MA-s->count)*255/20;
    for (unsigned i=0;i<s->count;i++) for (unsigned c=0;c<3;c++) {
        if (sum>budget) out->rgb[i][c]=(unsigned)out->rgb[i][c]*budget/sum;
        if (out->rgb[i][c]) out->rgb_lit=true;
    }
    out->powered=out->oled || out->binary || out->rgb_lit;
}
static void pixel(uint8_t *f,unsigned x,unsigned y) { f[(y/8)*128+x] |= 1u<<(y%8); }
/* Uppercase 3x5 glyphs, row major, MSB first. */
static const uint16_t digits[]={0x7b6f,0x2c97,0x73e7,0x73cf,0x5bc9,0x79cf,0x79ef,0x7249,0x7bef,0x7bcf};
static const uint16_t letters[]={
    0x2bed,0x6bae,0x3923,0x6b6e,0x79a7,0x79a4,0x396b,0x5bed,0x7497,0x124e,
    0x5bad,0x4927,0x5fed,0x5f6d,0x2b6a,0x6ba4,0x2b7b,0x6bad,0x39ce,0x7492,
    0x5b6f,0x5b6a,0x5bfd,0x5aad,0x5a92,0x72a7};
static void label(uint8_t *f,unsigned x,unsigned y,const char *str)
{
    while (*str) {
        char c=*str++;
        uint16_t g=c>='A' && c<='Z' ? letters[c-'A'] :
                   c>='0' && c<='9' ? digits[c-'0'] : c=='-' ? 0x1c0 : 0;
        for (unsigned n=0;n<15;n++) if (g & (1u<<(14-n))) pixel(f,x+n%3,y+n/3);
        x+=4;
    }
}
void oled_frame(uint8_t f[1024],const struct indicators *v)
{
    static const uint8_t xy[18][2]={{3,26},{15,23},{27,26},{35,39},{55,21},{71,17},{87,18},{103,21},
        {55,35},{71,31},{87,32},{103,35},{17,6},{32,6},{47,6},{62,6},{77,6},{92,6}};
    static const char *const socd[]={"NEUTRAL","UP","LAST","FIRST"};
    static const char *const dir[]={"DPAD","LS","RS"};
    memset(f,0,1024);
    for (unsigned i=0;i<18;i++) for (unsigned y=0;y<7;y++) for (unsigned x=0;x<9;x++) {
        if (x==0 || x==8 || y==0 || y==6 || (v->keys & (1u<<i))) pixel(f,xy[i][0]+x,xy[i][1]+y);
    }
    label(f,0,49,socd[v->socd]); label(f,36,49,dir[v->direction]);
    char player[]={'P',v->player ? (char)('0'+v->player) : '-',0};
    label(f,60,49,player); label(f,76,49,v->wired?"USB":"RF");
    if (v->turbo) label(f,92,49,"TURBO");
    label(f,0,58,"BAT");
    if (v->mv) {
        unsigned pct=v->mv>=4200 ? 100 : v->mv<=3300 ? 0 : (v->mv-3300)/9;
        char val[]={pct==100?'1':' ',pct>=10?(char)('0'+pct/10%10):' ',(char)('0'+pct%10),0};
        label(f,16,58,val); label(f,32,58,"PCT");
    } else label(f,16,58,"--");
    if (v->macros) {
        label(f,60,58,"MACRO");
        for (unsigned i=0;i<MACRO_COUNT;i++) if (v->macros & (1u<<i)) {
            char n[]={(char)('1'+i),0}; label(f,84+5*i,58,n);
        }
    }
}
