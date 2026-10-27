/* Copyright (c) 2026 Phaos. See ../LICENSE.
 * Original experimental arrangement of pre-1994 comb/allpass primitives.
 * Four signed early packets, four damped combs, two common allpasses and
 * two stereo allpasses. Fixed 12-bit rounded early storage; no modulation.
 */
#include "bh_room.h"
#include <string.h>
static bh_i32 sat(bh_i32 x){return x<-32768L?-32768L:x>32767L?32767L:x;}
static bh_i32 abs32(bh_i32 x){return x<0?-x:x;}
static bh_i32 q12(bh_i32 product){
    /* Watcom's signed right shift is arithmetic on this explicit x86 target.
       Bias is half a Q12 step, with ties away from zero. Inputs are bounded. */
    bh_i32 sign=product>>31;
    bh_i32 q=(product+262144L+sign)>>19;
    return q*16L;
}
static bh_i32 early_packet(bh_state *s,const bh_packet *p){
    bh_i32 a=s->history[(s->write-p->delay[0])&(BH_CAPACITY-1UL)];
    bh_i32 b=s->history[(s->write-p->delay[1])&(BH_CAPACITY-1UL)];
    bh_i32 c=s->history[(s->write-p->delay[2])&(BH_CAPACITY-1UL)];
    return q12(a*(bh_i32)p->gain[0][0]+b*(bh_i32)p->gain[0][1]+c*(bh_i32)p->gain[0][2]);
}
static bh_i32 comb_tick(bh_comb *c,bh_i32 input){
    bh_i32 delayed=c->data[c->position];
    bh_i32 filtered=c->low+((delayed-c->low)*11469L)/32768L;
    if(delayed==0&&abs32(filtered)>=abs32(c->low))filtered=0;
    c->low=filtered;
    c->data[c->position]=(bh_i16)sat(input+(filtered*c->feedback)/32768L);
    if(++c->position==c->length)c->position=0;
    return delayed;
}
static bh_i32 allpass_tick(bh_allpass *a,bh_i32 input){
    bh_i32 delayed=a->data[a->position],output=delayed-input/2L;
    a->data[a->position]=input+output/2L;
    if(++a->position==a->length)a->position=0;
    return output;
}
int bh_room_init(bh_room *s,const bh_patch *p){
    static const bh_u32 delays[4]={659,809,947,1103};
    /* 10^(-3 * delay_seconds / 1.5), Q15; frozen outside the sample loop. */
    static const bh_i32 feedback[4]={28498,27607,26812,25940};
    static const bh_u32 spread[4]={113,37,67,97};
    int i,f;
    if(!s||!p||p->bits!=12||p->truncation||p->count!=4||p->low_enabled)return 0;
    memset(s,0,sizeof(*s));if(!bh_init(&s->early,p))return 0;
    for(i=0;i<4;++i){
        bh_i32 bound=0;
        for(f=0;f<3;++f)s->early.patch.packet[i].gain[0][f]=(bh_i16)(((bh_i32)p->packet[i].gain[0][f]+p->packet[i].gain[1][f])/2L);
        for(f=0;f<3;++f)bound+=abs32(s->early.patch.packet[i].gain[0][f]);
        if(bound>60000L)return 0; /* Leave safe headroom for rounded Q12 bias. */
        s->comb[i].length=(delays[i]*(bh_u32)p->rate+10869UL)/21739UL;
        s->comb[i].feedback=feedback[i];
        s->allpass[i].length=(spread[i]*(bh_u32)p->rate+10869UL)/21739UL;
    }
    return 1;
}
void bh_room_frame(bh_room *s,bh_u8 left,bh_u8 right,bh_i16 *out_l,bh_i16 *out_r){
    bh_state *e=&s->early;
    bh_i32 x=((bh_i32)left+(bh_i32)right-256L)*128L;
    bh_i32 filtered=q12(e->low*(32768L-e->patch.tone)+x*e->patch.tone);
    bh_i32 early,late,drive;
    if(x==0&&abs32(filtered)>=abs32(e->low))filtered=0;
    e->low=sat(filtered);e->history[e->write]=(bh_i16)(e->low>32752L?32752L:e->low);
    early=early_packet(e,&e->patch.packet[0])+early_packet(e,&e->patch.packet[1])+
          early_packet(e,&e->patch.packet[2])+early_packet(e,&e->patch.packet[3]);
    e->write=(e->write+1UL)&(BH_CAPACITY-1UL);
    drive=e->low/8L;
    late=(comb_tick(&s->comb[0],drive)+comb_tick(&s->comb[1],drive)+
          comb_tick(&s->comb[2],drive)+comb_tick(&s->comb[3],drive))/2L;
    late=allpass_tick(&s->allpass[0],late);
    late=allpass_tick(&s->allpass[1],late);
    *out_l=(bh_i16)sat(early/2L+allpass_tick(&s->allpass[2],late));
    *out_r=(bh_i16)sat(early/2L+allpass_tick(&s->allpass[3],late));
}
static bh_u8 byte_code(bh_i16 x){
    bh_i32 q=x<0?-((-((bh_i32)x)+128L)/256L):((bh_i32)x+128L)/256L;
    q+=128L;return (bh_u8)(q<0?0:q>255?255:q);
}
void bh_room_render(bh_room *s,const bh_u8 *in,bh_u8 *out,bh_u32 frames){
    bh_u32 i;bh_i16 a,b;
    for(i=0;i<frames;++i){bh_room_frame(s,in[i*2],in[i*2+1],&a,&b);out[i*2]=byte_code(a);out[i*2+1]=byte_code(b);}
}
