/* Copyright (c) 2026 Phaos. See ../LICENSE and ../THIRD_PARTY_NOTICES.md.
 * Stationary shared-packet kernel for 32-bit DOS. C89, no FPU or 64-bit math.
 * Patch construction and the analogue converter simulation are separate.
 */
#ifndef BH_CORE_H
#define BH_CORE_H
#include <limits.h>
#if LONG_MAX != 2147483647L || SHRT_MAX != 32767
#error This core requires 32-bit long and 16-bit short
#endif
#define BH_CAPACITY 131072UL
#define BH_MAX_PACKETS 64
typedef signed long bh_i32;
typedef unsigned long bh_u32;
typedef signed short bh_i16;
typedef unsigned char bh_u8;
typedef struct {
    bh_u32 delay[3];
    bh_i16 gain[2][3];
} bh_packet;
typedef struct {
    bh_i32 count,bits,truncation,rate,tone,bass,low_enabled;
    bh_packet packet[BH_MAX_PACKETS];
} bh_patch;
typedef struct {
    bh_patch patch;
    bh_u32 write;
    bh_i32 low,low_band;
    bh_u32 rounding;
    int product_shift,pcm_shift;
    bh_i16 history[BH_CAPACITY];
} bh_state;
#ifdef __cplusplus
extern "C" {
#endif
int bh_init(bh_state *state,const bh_patch *patch);
void bh_frame(bh_state *state,bh_u8 left,bh_u8 right,bh_i16 *wet_left,bh_i16 *wet_right);
void bh_render(bh_state *state,const bh_u8 *input,bh_u8 *output,bh_u32 frames);
#ifdef __cplusplus
}
#endif
#endif
