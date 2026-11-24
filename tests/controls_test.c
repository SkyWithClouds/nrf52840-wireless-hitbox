#include "controls.h"
#include "rail.h"
#include <assert.h>
#define K(i) (1u<<(i))
int main(void)
{
    struct configuration c;config_defaults(&c);struct engine e={.eager=true};struct playback fx={0};
    struct profile *p=&c.profiles[0];p->debounce=5;
    engine_keys(&e,p,K(8),0);assert(e.stable==K(8));
    engine_keys(&e,p,0,1);engine_keys(&e,p,K(8),2);engine_keys(&e,p,0,3);
    engine_keys(&e,p,0,7);assert(e.stable);engine_keys(&e,p,0,8);assert(!e.stable);
    struct chord g={0};uint32_t keys=K(AUX)|K(START)|K(BACK);
    chord_tick(&g,keys,0,true);assert(!chord_tick(&g,0,1999,true));
    chord_tick(&g,keys,3000,true);chord_tick(&g,keys,5000,true);
    assert(!chord_tick(&g,K(AUX),5001,true));assert(chord_tick(&g,0,5002,true)==TOGGLE_LOCK);
    c.locked=c.neutral=1;p->socd=SOCD_UP;c.fx.settings.allow[1]=1u<<(A-8);
    fx.playing=63;e.logical=MENU_BITS|K(UP)|K(DOWN)|K(A)|K(MACRO_1);
    assert(gameplay(&e,p,&fx,&c,false,10).buttons==K(A) && !fx.playing && !fx.pulsing);
    assert(gameplay(&e,p,&fx,&c,false,55).buttons==K(A));
    struct rail rail={0};uint32_t t=UINT32_MAX-4;
    assert(rail_step(&rail,true,t)==RAIL_ENABLE && rail_step(&rail,true,t+12)==RAIL_WAIT);
    assert(rail_step(&rail,true,t+13)==RAIL_DRIVE && rail_step(&rail,false,t+13)==RAIL_ISOLATE_OFF);
    assert(rail_step(&rail,true,t+14)==RAIL_ENABLE && rail_step(&rail,false,t+15)==RAIL_ISOLATE_OFF);
}
