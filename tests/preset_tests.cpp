// Copyright (c) 2026 Phaos - phaos2000@pm.me
// Original Bulkhead portions are licensed under the Bulkhead Freeware and
// Source-Available License. See LICENSE and THIRD_PARTY_NOTICES.md in the project root.
#include "Presets.h"
#include <iostream>
using namespace bulkhead;
int main(int argc,char** argv){
    int failures=0;auto check=[&](bool ok,const char* label){std::cout<<(ok?"PASS ":"FAIL ")<<label<<"\n";if(!ok)++failures;};
    fs::path folder=fs::path(argc>1?argv[1]:"artifacts")/("preset-test-"+std::to_string(GetCurrentProcessId()));
    PresetLibrary library(folder);auto original=defaults();original[Truncate]=1.;std::string error;
    check(library.scan().size()==6,"Six factory choices without user files");
    check(library.save("TEST PATCH",original,false,error),"Create a user preset atomically");
    Values loaded{};check(PresetLibrary::read(folder/"TEST PATCH.bulkhead",loaded)&&loaded==original,"Every sound parameter including TRUNC round-trips");
    auto legacyTone=original;legacyTone[ToneFilter]=0.;
    check(library.save("TEST PATCH",legacyTone,true,error)&&PresetLibrary::read(folder/"TEST PATCH.bulkhead",loaded)&&loaded[ToneFilter]==1.,"Legacy Tone Off preset restores main Tone controls as active");
    check(!library.save("TEST PATCH",defaults(),false,error),"Save As never silently replaces an existing preset");
    check(library.save("TEST PATCH",defaults(),true,error)&&PresetLibrary::read(folder/"TEST PATCH.bulkhead",loaded)&&loaded==defaults(),"Explicit Update replaces preset contents");
    auto entries=library.scan();check(entries.size()==7&&!entries.back().factory,"Saved preset appears in library");
    check(library.rename(entries.back(),"RENAMED",error)&&fs::exists(folder/"RENAMED.bulkhead"),"Rename user preset");
    check(!library.erase(entries.front(),error)&&!library.rename(entries.front(),"INIT2",error),"Factory choices are protected");
    bool names=true;for(const char* name:{"../ESCAPE"," A","A ","CON","COM1","NUL","A/B"})names=names&&!validPresetName(name);
    check(names,"Reject path traversal, device names and malformed names");
    {std::ofstream out(folder/"BAD.bulkhead");out<<"BULKHEAD 2 14\n0 NaN\n";}
    check(!PresetLibrary::read(folder/"BAD.bulkhead",loaded)&&library.scan().size()==7,"Malformed files are rejected and skipped in the menu");
    {std::ofstream out(folder/"LEGACY.bulkhead");out<<"BULKHEAD 1 13\n";for(int i=0;i<13;++i)out<<i<<" "<<original[i]<<"\n";}
    check(PresetLibrary::read(folder/"LEGACY.bulkhead",loaded)&&loaded[Truncate]==0.,"Earlier preset format gains TRUNC off by default");
    {std::ofstream out(folder/"VERSION3.bulkhead");out<<"BULKHEAD 3 18\n"<<std::setprecision(17);auto old=defaults();old[Decay]=std::log(2./.025)/std::log(120./.025);old[Density]=(48.-4.)/252.;for(int i=0;i<18;++i)out<<i<<" "<<old[i]<<"\n";}
    check(PresetLibrary::read(folder/"VERSION3.bulkhead",loaded)&&std::abs(plain(Decay,loaded[Decay])-2.)<1e-10&&std::abs(plain(Density,loaded[Density])-48.)<1e-10&&loaded[SbFilter]==0.&&loaded[ToneFilter]==1.,"Previous preset ranges migrate in seconds and tap counts with new filter defaults");
    entries=library.scan();for(const auto& e:entries)if(!e.factory)check(library.erase(e,error),"Delete test user preset");
    fs::remove(folder/"BAD.bulkhead");fs::remove(folder);
    std::cout<<"RESULT "<<failures<<" failures\n";return failures?1:0;
}
