/* SPDX-License-Identifier: MIT */
#include <zephyr/fs/nvs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/byteorder.h>
#include "match.h"
struct match match_saved;
struct rf_stats match_stats[4];
struct nvs_fs match_fs;
static bool mounted;
static enum match_role device_role;
void match_init(enum match_role role)
{
    device_role=role;
    match_saved=match_factory(role);
    match_fs=(struct nvs_fs){.flash_device=FIXED_PARTITION_DEVICE(hitbox_nvs),
        .offset=FIXED_PARTITION_OFFSET(hitbox_nvs), .sector_size=4096,
        .sector_count=FIXED_PARTITION_SIZE(hitbox_nvs)/4096};
    mounted=device_is_ready(match_fs.flash_device) && !nvs_mount(&match_fs);
    uint8_t record[7];
    if (mounted && nvs_read(&match_fs,2,record,sizeof(record))==sizeof(record) && record[0]==1) {
        struct match candidate;
        memcpy(&candidate,record+1,sizeof(candidate));
        if (match_valid(candidate,role)) match_saved=candidate;
    }
}
bool match_command(const uint8_t q[64],uint8_t r[64])
{
    if (memcmp(q,"HX\1",3) || q[3]<MATCH_READ || q[3]>MATCH_STATS) return false;
    memset(r,0,64);
    memcpy(r,q,6);
    r[7]=device_role;
    if (q[3]==MATCH_WRITE) {
        struct match m;
        memcpy(&m,q+8,sizeof(m));
        uint8_t record[7]={1};
        memcpy(record+1,&m,sizeof(m));
        if (!match_valid(m,device_role)) r[6]=2;
        else if (!mounted || nvs_write(&match_fs,2,record,sizeof(record))<0) r[6]=3;
        else match_saved=m;
    }
    memcpy(r+8,&match_saved,sizeof(match_saved));
    if (q[3]==MATCH_STATS && q[7]<4) {
        const struct rf_stats *s=&match_stats[q[7]];
        r[14]=s->online;
        sys_put_le32(s->frames,r+16); sys_put_le32(s->gaps,r+20);
        sys_put_le32(s->duplicates,r+24); sys_put_le32(s->timeouts,r+28);
    }
    return true;
}
