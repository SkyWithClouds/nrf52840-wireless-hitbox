/* SPDX-License-Identifier: MIT */
#include "config.h"
_Static_assert(sizeof(struct profile) == 21, "profile wire format");
_Static_assert(sizeof(struct configuration) == 87, "NVS schema v1");
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
    case CMD_READ:
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
