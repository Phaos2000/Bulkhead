// Copyright (c) 2026 Phaos. See LICENSE and THIRD_PARTY_NOTICES.md.
#include "Engine.h"
#include <iostream>
#include <vector>
using namespace bulkhead;
int main(){
    NoDenormals guard;int checks=0,failures=0;auto check=[&](bool ok,const char* s){++checks;std::cout<<(ok?"PASS ":"FAIL ")<<s<<"\n";failures+=!ok;};
    auto v=defaults();check(Count==19&&v[Clock]==1.&&v[Dither]==0.&&plain(Density,v[Density])==20.,"Defaults have one fixed ADC/shared-chamber/DAC route with 22k, 20 packets and no dither");
    // An independent ADC/DAC-only cascade is the oracle at zero wet amount.
    for(double sr:{8000.,44100.,48000.,96000.,192000.,384000.})for(int clock:{0,1}){bool exact=true;double maxError=0.;
        for(int inputFilter:{0,1,2})for(int outputFilter:{0,1}){auto p=defaults();p[Clock]=clock;p[Mix]=0.;p[AntiAlias]=inputFilter?1.:0.;p[AdcFilter]=inputFilter==2?1.:0.;p[SbFilter]=outputFilter;Engine e;e.prepare(sr,p);SbPro2Adc adc;SbPro2Dac dac;adc.prepare(sr);dac.prepare(sr);double phase=1.,seconds=0.,rate=SbPro2Dac::playbackRate(plain(Clock,p[Clock]));
            for(int n=0;n<10000;++n){double x=.29*std::sin(n*.173),y=.19*std::cos(n*.119),a,b,c,d,l,r;if(n%311==0)e.set(Output,(n/311)%2);e.tick(x,y,a,b);adc.filter(x,y,p[AntiAlias],p[AdcFilter],l,r);seconds+=1./sr;phase+=rate/sr;while(phase>=1.){phase-=1.;dac.push(seconds-phase/rate,SbPro2Adc::capture(l),SbPro2Adc::capture(r));}dac.render(seconds,p[SbFilter],c,d);const double error=std::max(std::abs(a-c),std::abs(b-d));maxError=std::max(maxError,error);
                // Filtered floating-point recurrences can differ by a few ULPs
                // when compiled in the standalone oracle versus the full engine.
                // The unfiltered converter codes must still match exactly.
                exact=exact&&(outputFilter?error<=1.e-12:(a==c&&b==d));}}
        std::cout<<"CASCADE max_error="<<maxError<<" rate="<<sr<<" clock="<<clock<<"\n";
        check(exact,"Zero-wet output matches ADC then DAC at every host rate, card clock and filter setting");}
    {auto p=defaults();p[Mix]=1.;p[Output]=p[Body]=1.;Engine e;e.prepare(48000,p);bool ok=true;int clipped=0;for(int n=0;n<48000*3;++n){double a,b;e.tick(.8*std::sin(n*.031),.8*std::cos(n*.037),a,b);ok=ok&&a>=-1&&a<=127./128.&&b>=-1&&b<=127./128.&&a*128==std::round(a*128)&&b*128==std::round(b*128);clipped+=a==-1||a==127./128.||b==-1||b==127./128.;}check(ok&&clipped>0,"Dry plus wet saturates at one final 8-bit DAC rather than exceeding its rails");}
    {auto p=defaults();p[Mix]=0.;Engine raw,filtered;raw.prepare(192000,p);p[SbFilter]=1.;filtered.prepare(192000,p);double ea=0,eb=0;for(int n=0;n<192000;++n){double x=.3*std::sin(2*pi*8000*n/192000.),a,b,c,d;raw.tick(x,x,a,b);filtered.tick(x,x,c,d);if(n>10000){ea+=a*a;eb+=c*c;}}check(10*std::log10(eb/ea)<-15.,"SB output filtering acts on captured dry as well as wet audio");}
    for(int clock:{0,1})for(int presetIndex=-1;presetIndex<presetCount;++presetIndex){auto p=presetIndex<0?defaults():preset(presetIndex);p[Clock]=clock;p[Mix]=1.;Engine e;e.prepare(48000,p);uint32_t random=1;double l=0,r=0;for(int n=0;n<48000*10;++n){random=random*1664525u+1013904223u;double x=n<48000?.18*(double(random)/4294967295.*2-1):0.,a,b;e.tick(x,x,a,b);l+=a*a;r+=b*b;}double db=10*std::log10(r/l);std::cout<<"BALANCE clock="<<clock<<" preset="<<presetIndex<<" right_db="<<db<<"\n";check(l>.001&&r>.001&&std::abs(db)<1.,"Combined-DAC output stays centred across defaults and factory sounds");}
    for(int dither:{0,1}){auto p=defaults();p[Dither]=dither;Engine e;e.prepare(48000,p);bool silent=true;for(int n=0;n<48000;++n){double a,b;e.tick(0,0,a,b);silent=silent&&a==0&&b==0;}check(silent,"Idle stays exactly silent with dither off or on");std::vector<double> first;for(int n=0;n<24000;++n){double a,b;e.tick(.2*std::sin(n*.043),0,a,b);first.push_back(a);first.push_back(b);}e.reset();for(int n=0;n<48000;++n){double a,b;e.tick(0,0,a,b);}bool repeat=true;for(int n=0;n<24000;++n){double a,b;e.tick(.2*std::sin(n*.043),0,a,b);repeat=repeat&&a==first[n*2]&&b==first[n*2+1];}check(repeat,"Reset reproduces the complete converter and chamber output deterministically");}
    {auto p=defaults();p[Bypass]=1.;p[Dither]=p[AntiAlias]=p[SbFilter]=1.;Engine e;e.prepare(48000,p);bool exact=true;for(int n=0;n<20000;++n){double x=1.2*std::sin(n*.017),y=.7*std::cos(n*.113),a,b;e.tick(x,y,a,b);exact=exact&&a==x&&b==y;}check(exact,"Host bypass remains exact raw input");}
    {Engine e;e.prepare(48000);uint32_t random=37;bool finite=true;for(int n=0;n<48000*4;++n){if(n%137==0){random=random*1664525u+1013904223u;e.set(int((random>>16)%Count),(random&1)?1.:0.);}double a,b;e.tick(.3*std::sin(n*.019),.2*std::cos(n*.021),a,b);finite=finite&&std::isfinite(a)&&std::isfinite(b)&&std::abs(a)<2&&std::abs(b)<2;}check(finite,"All remaining parameters tolerate rapid full-range automation");}
    std::cout<<"RESULT "<<checks-failures<<" passed / "<<failures<<" failures\n";return failures?1:0;
}
