/* SPDX-License-Identifier: MIT */
#include <zephyr/fs/nvs.h>
#include "config.h"
#include <stddef.h>
extern struct nvs_fs match_fs;
#define fs match_fs
static bool writable;
int store_open(struct configuration *c)
{
    config_defaults(c);
    if (!fs.ready) return -EIO;
    writable=true;
    struct configuration stored=*c;
    ssize_t n=nvs_read(&fs,3,&stored,sizeof(stored));
    if (n==sizeof(stored) && config_valid(&stored)) { *c=stored; return 0; }
    if (n==offsetof(struct configuration,eager) && stored.version==2) {
        stored.version=3;
        if (config_valid(&stored)) { *c=stored; return 0; }
    }
    if (n!=-ENOENT) return -EINVAL;
    /* Record 1 is the exact 87-byte v1 record. Read it whole to verify its NVS CRC. */
    size_t legacy=offsetof(struct configuration,fx);
    if (nvs_read(&fs,1,&stored,legacy)==(ssize_t)legacy && stored.version==1) {
        stored.version=3;
        if (config_valid(&stored)) *c=stored;
    }
    return 0;
}
int store_save(const struct configuration *c)
{
    if (!writable) return -EIO;
    ssize_t n=nvs_write(&fs,3,c,sizeof(*c));
    return n==0 || n==sizeof(*c) ? 0 : -EIO;
}
