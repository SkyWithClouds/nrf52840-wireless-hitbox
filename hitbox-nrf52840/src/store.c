/* SPDX-License-Identifier: MIT */
#include <zephyr/fs/nvs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>
#include "config.h"
static struct nvs_fs fs;
static bool writable;
int store_open(struct configuration *c)
{
    config_defaults(c);
    fs.flash_device=FIXED_PARTITION_DEVICE(hitbox_nvs);
    fs.offset=FIXED_PARTITION_OFFSET(hitbox_nvs);
    fs.sector_size=4096;
    fs.sector_count=FIXED_PARTITION_SIZE(hitbox_nvs)/4096;
    if (!device_is_ready(fs.flash_device)) return -ENODEV;
    int rc=nvs_mount(&fs);
    if (rc) return rc;
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
