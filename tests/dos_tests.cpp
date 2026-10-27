// Copyright (c) 2026 Phaos. See LICENSE and THIRD_PARTY_NOTICES.md.
#include "Engine.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
using namespace bulkhead;
int main(int argc,char** argv){
    NoDenormals guard;int failures=0,checks=0;
    auto check=[&](bool ok,const char* text){++checks;std::cout<<(ok?"PASS ":"FAIL ")<<text<<"\n";failures+=!ok;};
    std::filesystem::path folder=argc>1?argv[1]:"artifacts/dos";std::filesystem::create_directories(folder);
    check(sizeof(bh_i32)==4&&sizeof(bh_i16)==2&&sizeof(bh_patch)==1564,"DOS patch uses the expected 32/16-bit little-endian ABI");
    for(int clock:{0,1})for(int bits:{8,12,16})for(int trunc:{0,1})for(int density:{4,16,20,24,64}){
        auto v=defaults();v[Clock]=clock;v[Bits]=normalized(Bits,bits);v[Truncate]=trunc;v[Density]=normalized(Density,density);
        v[Geometry]=(density%3)*.5;v[LowDamp]=density==24?normalized(LowDamp,150.):0.;
        Chamber reference;reference.prepare(22050.,v);reference.clock(SbPro2Dac::playbackRate(plain(Clock,v[Clock])));reference.reset();
        bh_patch patch{};auto core=std::make_unique<bh_state>();
        bool exact=reference.dosPatch(patch)&&bh_init(core.get(),&patch);uint32_t random=1;
        for(int n=0;n<patch.rate*3&&exact;++n){
            random=random*1664525u+1013904223u;const bh_u8 l=n<patch.rate?bh_u8((random>>24)%51+103):128;
            random=random*1664525u+1013904223u;const bh_u8 r=n<patch.rate?bh_u8((random>>24)%51+103):128;
            bh_i16 a,b;double c,d;bh_frame(core.get(),l,r,&a,&b);reference.tick(SbPro2Dac::decode(l),SbPro2Dac::decode(r),c,d);
            exact=exact&&a==nearestInt(c*32768.)&&b==nearestInt(d*32768.);
        }
        check(exact,"Integer DOS kernel matches settled shared chamber exactly across clock/precision/truncation/density cases");
    }
    for(int density:{4,16,20,24,64}){
        auto v=defaults();v[Density]=normalized(Density,density);v[Body]=0.;
        Chamber reference;reference.prepare(22050.,v);reference.clock(21739.);reference.reset();bh_patch patch{};
        const bool exported=reference.dosPatch(patch);auto path=folder/("P"+std::to_string(density)+".BHP");
        std::ofstream file(path,std::ios::binary);file.write("BHP1",4);file.write(reinterpret_cast<const char*>(&patch),sizeof(patch));
        check(exported&&bool(file),"Export a stationary default-geometry DOS feasibility patch");
    }
    auto core=std::make_unique<bh_state>();bh_patch bad{};check(!bh_init(core.get(),&bad),"Reject invalid DOS patch before audio processing");
    std::cout<<"RESULT "<<checks-failures<<" passed / "<<failures<<" failures\n";return failures?1:0;
}
