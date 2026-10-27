/* Copyright (c) 2026 Phaos. See ../LICENSE. Experimental DOS room, not VST state. */
#ifndef BH_ROOM_H
#define BH_ROOM_H
#include "bh_core.h"
typedef struct {bh_i16 data[2048];bh_u32 position,length;bh_i32 low,feedback;} bh_comb;
typedef struct {bh_i32 data[256];bh_u32 position,length;} bh_allpass;
typedef struct {bh_state early;bh_comb comb[4];bh_allpass allpass[4];} bh_room;
#ifdef __cplusplus
extern "C" {
#endif
int bh_room_init(bh_room *s,const bh_patch *p);
void bh_room_frame(bh_room *s,bh_u8 left,bh_u8 right,bh_i16 *out_l,bh_i16 *out_r);
void bh_room_render(bh_room *s,const bh_u8 *in,bh_u8 *out,bh_u32 frames);
#ifdef __cplusplus
}
#endif
#endif
