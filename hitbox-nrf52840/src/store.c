/* SPDX-License-Identifier: MIT */
#include <zephyr/fs/nvs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>
#include "config.h"
extern struct nvs_fs match_fs;
#define fs match_fs
static bool writable;
int store_open(struct configuration *c)
{
    config_defaults(c);
    if (!fs.ready) return -EIO;
    writable=true;
    struct configuration stored;
    if (nvs_read(&fs,1,&stored,sizeof(stored))==sizeof(stored) && config_valid(&stored)) *c=stored;
    return 0;
}
int store_save(const struct configuration *c)
{
    if (!writable) return -EIO;
    ssize_t n=nvs_write(&fs,1,c,sizeof(*c));
    return n==0 || n==sizeof(*c) ? 0 : -EIO;
}
