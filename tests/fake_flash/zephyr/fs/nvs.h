/* Minimal storage boundary used to run the production record migration on the host. */
#pragma once
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
struct nvs_fs { bool ready; };
ssize_t nvs_read(struct nvs_fs *,uint16_t,void *,size_t);
ssize_t nvs_write(struct nvs_fs *,uint16_t,const void *,size_t);
