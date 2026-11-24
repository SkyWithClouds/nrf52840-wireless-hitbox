/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
enum match_role { HITBOX=1, PC_RX, CONSOLE_RX, BRIDGE_TX };
enum { MATCH_READ=32, MATCH_WRITE, MATCH_STATS };
struct match { uint8_t base[4], channel, slots; };
static inline struct match match_factory(enum match_role role)
{
    return (struct match){{0xb7,0x46,0x32,0xda},40,role==PC_RX?2:1};
}
static inline bool match_valid(struct match m, enum match_role role)
{
    return m.channel>=2 && m.channel<=80 && m.slots>=1 && m.slots<=4 &&
           (role!=CONSOLE_RX || m.slots==1);
}
static inline uint16_t match_delay(unsigned pipe) { return 600+(pipe-1)*250; }
/* Pipe indices are local radio registers. Preserve v1's physical address on pipe 1. */
static const uint8_t match_prefix[5]={0x78,0xc5,0x36,0x9a,0x6c};
static inline uint8_t match_mask(struct match m,bool receiver)
{
    return receiver ? (1u << (m.slots+1))-2 : 1u << m.slots;
}
struct rf_stats { uint32_t frames, gaps, duplicates, timeouts; bool online; };
extern struct match match_saved;
extern struct rf_stats match_stats[4];
void match_init(enum match_role role);
bool match_command(const uint8_t q[64], uint8_t r[64]);
