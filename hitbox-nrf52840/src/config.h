/* SPDX-License-Identifier: MIT */
#pragma once
#include "engine.h"
enum { HID_BYTES=64, CMD_READ=1, CMD_BEGIN, CMD_STAGE, CMD_COMMIT, CMD_ABORT };
struct editor { struct configuration draft; bool editing; };
int store_open(struct configuration *cfg);
int store_save(const struct configuration *cfg);
void config_command(struct editor *edit, struct configuration *live, const uint8_t *req,
                    uint8_t *res, int (*save)(const struct configuration *));
