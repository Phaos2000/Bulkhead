/* Copyright (c) 2026 Phaos. See ../LICENSE and ../THIRD_PARTY_NOTICES.md. */
#include "bh_core.h"
#include <string.h>

static bh_i32 limit_word(bh_i32 x) {
    return x < -32768L ? -32768L : x > 32767L ? 32767L : x;
}
static bh_u32 magnitude(bh_i32 x) {
    return x < 0 ? 0UL-(bh_u32)x : (bh_u32)x;
}
static bh_i32 absolute(bh_i32 x) { return x < 0 ? -x : x; }
static bh_i32 product_word(bh_i32 product,int bits,int truncation) {
    unsigned int shift=31-bits;
    bh_u32 n=magnitude(product);
    if(!truncation)n+=1UL<<(shift-1);
    n=(n>>shift)<<(16-bits);
    return product < 0 ? -(bh_i32)n : (bh_i32)n;
}
static bh_i32 configured_product(bh_i32 product,const bh_state *s) {
    bh_u32 n=(magnitude(product)+s->rounding)>>s->product_shift;
    n<<=s->pcm_shift;
    return product<0?-(bh_i32)n:(bh_i32)n;
}
static bh_i16 output_word(bh_i32 sum,int bits) {
    /* Exact rational 1.4 gain followed by rounded PCM, without 64-bit math.
       The packet sum is bounded; numerator is far below 2^32. */
    bh_u32 step=1UL<<(16-bits),denominator=5UL*step;
    bh_u32 q=(magnitude(sum)*7UL+denominator/2UL)/denominator;
    bh_i32 x=(bh_i32)(q*step);
    if(sum<0)x=-x;
    if(x>32768L-(bh_i32)step)x=32768L-(bh_i32)step;
    if(x<-32768L)x=-32768L;
    return (bh_i16)x;
}
static bh_u8 dac_byte(bh_i16 x) {
    bh_i32 q=(bh_i32)((magnitude(x)+128UL)>>8);
    if(x<0)q=-q;
    q+=128;
    return (bh_u8)(q<0?0:q>255?255:q);
}
int bh_init(bh_state *state,const bh_patch *patch) {
    int i,f,ch;
    if(!state||!patch||patch->count<4||patch->count>64||
       (patch->bits!=8&&patch->bits!=12&&patch->bits!=16)||
       (patch->truncation!=0&&patch->truncation!=1)||
       patch->tone<1||patch->tone>32767||patch->bass<1||patch->bass>32767||
       (patch->low_enabled!=0&&patch->low_enabled!=1)||
       (patch->rate!=11111&&patch->rate!=21739))return 0;
    for(i=0;i<patch->count;++i){
        bh_u32 absolute_sum[2]={0,0};
        for(f=0;f<3;++f){
            if(patch->packet[i].delay[f]<1||patch->packet[i].delay[f]>=BH_CAPACITY)return 0;
            for(ch=0;ch<2;++ch)absolute_sum[ch]+=magnitude(patch->packet[i].gain[ch][f]);
        }
        /* Protect every signed 32-bit shard product and sum. */
        if(absolute_sum[0]>65535UL||absolute_sum[1]>65535UL)return 0;
    }
    memset(state,0,sizeof(*state));state->patch=*patch;
    state->product_shift=(int)(31-patch->bits);state->pcm_shift=(int)(16-patch->bits);
    state->rounding=patch->truncation?0UL:1UL<<(state->product_shift-1);
    return 1;
}
void bh_frame(bh_state *s,bh_u8 left,bh_u8 right,bh_i16 *out_l,bh_i16 *out_r) {
    const bh_patch *p=&s->patch;
    bh_i32 x,bass,source,filtered,shard_l,shard_r,sum_l=0,sum_r=0;
    const bh_packet *packet=p->packet,*end=packet+p->count;
    x=((bh_i32)left+(bh_i32)right-256L)*128L;
    x=limit_word(product_word(x*32768L,(int)p->bits,0));
    bass=configured_product(s->low_band*(32768L-p->bass)+x*p->bass,s);
    if(x==0&&absolute(bass)>=absolute(s->low_band))bass=0;
    s->low_band=limit_word(bass);
    source=limit_word(x-(p->low_enabled?s->low_band:0));
    filtered=configured_product(s->low*(32768L-p->tone)+source*p->tone,s);
    if(x==0&&absolute(filtered)>=absolute(s->low))filtered=0;
    s->low=limit_word(filtered);
    s->history[s->write]=(bh_i16)limit_word(product_word(s->low*32768L,(int)p->bits,0));
    for(;packet!=end;++packet){
        bh_i32 a=s->history[(s->write-packet->delay[0])&(BH_CAPACITY-1UL)];
        bh_i32 b=s->history[(s->write-packet->delay[1])&(BH_CAPACITY-1UL)];
        bh_i32 c=s->history[(s->write-packet->delay[2])&(BH_CAPACITY-1UL)];
        shard_l=a*(bh_i32)packet->gain[0][0]+b*(bh_i32)packet->gain[0][1]+c*(bh_i32)packet->gain[0][2];
        shard_r=a*(bh_i32)packet->gain[1][0]+b*(bh_i32)packet->gain[1][1]+c*(bh_i32)packet->gain[1][2];
        sum_l+=configured_product(shard_l,s);
        sum_r+=configured_product(shard_r,s);
    }
    s->write=(s->write+1UL)&(BH_CAPACITY-1UL);
    *out_l=output_word(sum_l,(int)p->bits);*out_r=output_word(sum_r,(int)p->bits);
}
void bh_render(bh_state *s,const bh_u8 *input,bh_u8 *output,bh_u32 frames) {
    bh_u32 n;bh_i16 left,right;
    for(n=0;n<frames;++n){
        bh_frame(s,input[n*2],input[n*2+1],&left,&right);
        output[n*2]=dac_byte(left);output[n*2+1]=dac_byte(right);
    }
}
