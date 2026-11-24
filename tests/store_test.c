#include <assert.h>
#include <stdio.h>
#include <zephyr/fs/nvs.h>
#include "config.h"
struct nvs_fs match_fs={true};
static uint8_t legacy[87],current[sizeof(struct configuration)];
static ssize_t current_size=-ENOENT;
ssize_t nvs_read(struct nvs_fs *fs,uint16_t id,void *dst,size_t len)
{
    assert(fs==&match_fs && (id==1 || id==3)); /* RF record 2 is owned by match.c */
    if (id==3 && current_size<0) return current_size;
    size_t size=id==3 ? (size_t)current_size : sizeof(legacy);
    assert(size<=len);
    memcpy(dst,id==3?current:legacy,size);return size;
}
ssize_t nvs_write(struct nvs_fs *fs,uint16_t id,const void *src,size_t size)
{
    assert(fs==&match_fs && id==3 && size==sizeof(current));
    memcpy(current,src,size);current_size=size;return size;
}
int main(void)
{
    struct configuration c,loaded;config_defaults(&c);
    c.active=3;c.divider_x10=20;c.profiles[3].map[0]=NEXT_PROFILE;
    memcpy(legacy,&c,sizeof(legacy));legacy[0]=1;
    assert(store_open(&loaded)==0 && !memcmp(&c,&loaded,sizeof(c)));
    loaded.fx.settings.accessory=BINARY_LED; loaded.fx.settings.led_map[2]=3;
    loaded.profiles[3].map[0]=MACRO_1+5; c=loaded;
    assert(!store_save(&loaded));memset(&loaded,0,sizeof(loaded));
    assert(!store_open(&loaded) && !memcmp(&c,&loaded,sizeof(c)));
    current_size=-EIO; /* an unreadable record leaves safe defaults */
    assert(store_open(&loaded)<0 && !loaded.fx.settings.accessory && loaded.active==0);
    puts("Legacy 87-byte profile migration, separate RF record, v2 reboot and failed read passed");
}
