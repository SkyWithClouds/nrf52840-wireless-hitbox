#include "config.h"
#include <assert.h>
#include <stdio.h>
#define K(i) (1u<<(i))
static int write_result;
static int save(const struct configuration *c) { assert(config_valid(c)); return write_result; }
static void config_test(void)
{
    struct configuration c; config_defaults(&c);
    struct editor e={0}; uint8_t q[64]={'H','X',1,CMD_BEGIN,12},r[64];
    config_command(&e,&c,q,r,save); assert(!r[6] && e.editing);
    struct profile p=c.profiles[2]; p.socd=SOCD_FIRST; p.debounce=10; p.map[13]=UNBOUND;
    q[3]=CMD_STAGE; q[7]=2; memcpy(q+10,&p,sizeof(p)); config_command(&e,&c,q,r,save);
    assert(!r[6] && !c.profiles[2].socd);
    q[3]=CMD_COMMIT; q[8]=2; q[9]=20; write_result=-1; config_command(&e,&c,q,r,save);
    assert(r[6]==3 && c.active==0 && !c.profiles[2].socd);
    write_result=0;config_command(&e,&c,q,r,save);assert(!r[6] && c.active==2 && c.divider_x10==20);
    q[3]=CMD_READ;config_command(&e,&c,q,r,save);assert(!memcmp(r+10,&p,sizeof(p)));
    q[7]=4;config_command(&e,&c,q,r,save);assert(r[6]);
    p.map[0]=20;assert(!profile_valid(&p));
}
int main(void)
{
    struct configuration c;config_defaults(&c);assert(config_valid(&c));
    struct profile p=c.profiles[0];p.debounce=1;
    for (int mode=0;mode<4;mode++) {
        struct engine e={0}; p.socd=mode;
        engine_scan(&e,&p,K(0),0);
        assert(engine_scan(&e,&p,K(0),1).buttons==4);
        engine_scan(&e,&p,K(0)|K(2),2);
        assert(engine_scan(&e,&p,K(0)|K(2),3).buttons==(mode==2?8:mode==3?4:0));
        engine_scan(&e,&p,K(2),4);
        assert(engine_scan(&e,&p,K(2),5).buttons==8);
        e=(struct engine){0};
        engine_scan(&e,&p,K(1)|K(3),0);
        assert(engine_scan(&e,&p,K(1)|K(3),1).buttons==(mode==1?1:0));
    }
    struct engine e={0};
    uint32_t mask=K(0)|K(1)|K(8)|K(9)|K(10)|K(11);
    p.output=RIGHT_STICK;
    engine_scan(&e,&p,mask,UINT32_MAX);
    struct gp_input s=engine_scan(&e,&p,mask,0);
    assert(s.buttons==0x3000 && s.rx==-32768 && s.ry==-32768 && s.lt==255 && s.rt==255);
    p.output=LEFT_STICK;s=engine_scan(&e,&p,mask,1);assert(s.lx==-32768 && !s.rx);
    p.debounce=10;e=(struct engine){0};engine_scan(&e,&p,K(8),0);
    engine_scan(&e,&p,0,2);engine_scan(&e,&p,K(8),3);
    assert(!engine_scan(&e,&p,K(8),12).buttons);
    assert(engine_scan(&e,&p,K(8),13).buttons==0x1000);
    config_test();puts("SOCD, 18-key engine, debounce, profiles and NVS transaction contract passed");
}
