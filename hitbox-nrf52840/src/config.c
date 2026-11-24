/* SPDX-License-Identifier: MIT */
#include "config.h"
#include <stddef.h>
_Static_assert(sizeof(struct profile) == 21, "profile wire format");
_Static_assert(offsetof(struct configuration, fx) == 87, "NVS schema v1");
/* HX, version, command, token LE16, result, slot. Profile payload begins at 10. */
void config_command(struct editor *e, struct configuration *c, const uint8_t *q,
                    uint8_t *r, int (*save)(const struct configuration *))
{
    memset(r,0,HID_BYTES);
    memcpy(r,q,6);
    const unsigned slot=q[7];
    r[7]=slot;
    if (q[0]!='H' || q[1]!='X' || q[2]!=1) { r[6]=1; return; }
    switch (q[3]) {
    case CMD_FX_READ:
    case CMD_FX_STAGE: {
        if (slot>MACRO_COUNT || (q[3]==CMD_FX_STAGE && !e->editing)) { r[6]=2; break; }
        struct effects *fx=q[3]==CMD_FX_READ ? &c->fx : &e->draft.fx;
        void *record=slot ? (void *)&fx->macros[slot-1] : (void *)&fx->settings;
        size_t size=slot ? sizeof(struct macro) : sizeof(struct settings);
        _Static_assert(sizeof(struct macro)<=HID_BYTES-10 && sizeof(struct settings)==18,"HID records");
        if (q[3]==CMD_FX_READ) memcpy(r+10,record,size);
        else memcpy(record,q+10,size);
        break;
    }
    case CMD_READ:
        r[31]=12; /* extended configuration capability */
        if (slot>=PROFILE_COUNT) { r[6]=2; break; }
        memcpy(r+10,&c->profiles[slot],sizeof(struct profile));
        break;
    case CMD_BEGIN:
        e->draft=*c; e->editing=true; break;
    case CMD_STAGE: {
        struct profile p;
        memcpy(&p,q+10,sizeof(p));
        if (!e->editing || slot>=PROFILE_COUNT || !profile_valid(&p)) { r[6]=2; break; }
        e->draft.profiles[slot]=p; break;
    }
    case CMD_COMMIT:
        if (!e->editing) { r[6]=2; break; }
        e->draft.active=q[8]; e->draft.divider_x10=q[9];
        if (!config_valid(&e->draft)) { r[6]=2; break; }
        if (save(&e->draft)) { r[6]=3; break; }
        *c=e->draft; e->editing=false; break;
    case CMD_ABORT: e->editing=false; break;
    default: r[6]=1; break;
    }
    r[8]=c->active; r[9]=c->divider_x10;
}
