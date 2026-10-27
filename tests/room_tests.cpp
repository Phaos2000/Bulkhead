// Copyright (c) 2026 Phaos. See LICENSE.
#include "Engine.h"
#include "../dos/bh_room.h"
#include <iostream>
#include <memory>
#include <fstream>
#include <filesystem>
using namespace bulkhead;
int main(int argc,char** argv){
    int failures=0,checks=0;auto check=[&](bool ok,const char* text){++checks;std::cout<<(ok?"PASS ":"FAIL ")<<text<<"\n";failures+=!ok;};
    auto room=std::make_unique<bh_room>();
    for(int clock:{0,1}){
        auto v=defaults();v[Density]=normalized(Density,4.);Chamber e;e.prepare(22050.,v);const int rate=int(SbPro2Dac::playbackRate(plain(Clock,clock)));e.clock(rate);e.reset();bh_patch p{};
        check(e.dosPatch(p)&&bh_room_init(room.get(),&p),"Configure period-plausible fixed-point room at either card clock");
        bool silent=true;for(int n=0;n<rate;++n){bh_i16 a,b;bh_room_frame(room.get(),128,128,&a,&b);silent=silent&&a==0&&b==0;}check(silent,"Experimental room preserves exact digital silence");
        bh_room_init(room.get(),&p);double l=0,r=0,difference=0;int clips=0;uint32_t rng=1;long late=0;
        for(int n=0;n<rate*12;++n){rng=rng*1664525u+1013904223u;const bh_u8 code=n<rate?bh_u8((rng>>24)%51+103):128;bh_i16 a,b;bh_room_frame(room.get(),code,code,&a,&b);l+=double(a)*a;r+=double(b)*b;difference+=std::abs(int(a)-int(b));clips+=a==32767||a==-32768||b==32767||b==-32768;if(n>rate*11)late+=std::abs(int(a))+std::abs(int(b));}
        const double db=10*std::log10(r/l);std::cout<<"ROOM rate="<<rate<<" right_db="<<db<<" clips="<<clips<<" final_second_energy="<<late<<"\n";
        check(l>1e6&&r>1e6&&difference>1000&&std::abs(db)<.5,"Experimental room creates stereo with centred integrated wet energy");
        check(clips==0&&late==0,"Normal excitation avoids clipping and clears to exact silence");
        bh_room_init(room.get(),&p);bool odd=true;auto inverse=std::make_unique<bh_room>();bh_room_init(inverse.get(),&p);
        for(int n=0;n<rate*2;++n){bh_i16 a,b,c,d;const int x=(n%51)-25;bh_room_frame(room.get(),bh_u8(128+x),bh_u8(128+x),&a,&b);bh_room_frame(inverse.get(),bh_u8(128-x),bh_u8(128-x),&c,&d);odd=odd&&a==-c&&b==-d;}
        check(odd,"Fixed-point room preserves odd symmetry without a negative DC rounding bias");
    }
    bh_patch bad{};check(!bh_room_init(room.get(),&bad),"Reject unsupported experimental-room patch instead of silently changing settings");
    std::cout<<"RESULT "<<checks-failures<<" passed / "<<failures<<" failures\n";return failures?1:0;
}
