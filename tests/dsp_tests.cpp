// Copyright (c) 2026 Phaos. See LICENSE and THIRD_PARTY_NOTICES.md.
#include "Engine.h"
#include <iostream>
#include <limits>
using namespace bulkhead;
int main(){
    NoDenormals guard;int checks=0,failures=0;
    auto check=[&](bool ok,const char* text){++checks;std::cout<<(ok?"PASS ":"FAIL ")<<text<<"\n";failures+=!ok;};
    for(int bits:{8,12,16}){bool good=true;int previous=-32768,step=1<<(16-bits);
        for(int i=0;i<=200000;++i){int q=pcmWord(i/100000.-1.,bits);good=good&&q%step==0&&q>=previous;previous=q;}
        check(good&&pcmWord(-2.,bits)==-32768&&pcmWord(2.,bits)==32768-step,"PCM grid is monotone and saturates without wrapping");}
    bool exact=true;
    for(int bits=8;bits<=16;++bits){int levels=1<<(bits-1),step=1<<(16-bits);
        for(int code=-levels;code<=levels;++code){double centre=(code+.5)/levels;
            for(double x:{centre,std::nextafter(centre,-INFINITY),std::nextafter(centre,INFINITY)})for(bool trunc:{false,true}){double scaled=std::clamp(x,-1.,1.)*levels;int ref=int(trunc?std::trunc(scaled):std::round(scaled))*step;exact=exact&&pcmWord(x,bits,trunc)==std::clamp(ref,-32768,32768-step);}}}
    check(exact,"Internal rounding matches every half-code boundary and adjacent double across 8-16 bits");
    check(pcmWord(std::numeric_limits<double>::quiet_NaN(),12)==0,"Invalid converter input is contained");
    {Arithmetic a(10,0),b(10,1);check(a(100*32768)==128&&b(100*32768)==64&&b(-100*32768)==-64,"Signed internal arithmetic keeps rounding and truncation rules");}
    exact=true;for(int code=-129;code<=129;++code){double centre=(code+.5)/128.;for(double x:{centre,std::nextafter(centre,-INFINITY),std::nextafter(centre,INFINITY)})exact=exact&&SbPro2Dac::encode(x)==std::clamp(int(std::lround(std::clamp(x,-1.,1.)*128.))+128,0,255);}
    check(exact&&SbPro2Dac::encode(0)==128&&SbPro2Dac::decode(0)==-1.&&SbPro2Dac::decode(255)==127./128.,"Converters retain unsigned 8-bit codes and exact centred silence");
    check(SbPro2Dac::playbackRate(22050)==21739.&&SbPro2Dac::playbackRate(11025)==11111.,"Stereo clock modes retain integer timer divisors");
    {SbPro2Dac d;d.prepare(192000);double a,b;d.push(0,.25,-.5);d.render(0,0,a,b);bool ok=a==.25&&b==-.5;d.push(.5/d.nativeRate,.5,.75);d.render(.75/d.nativeRate,0,a,b);ok=ok&&a==.25&&b==-.5;d.render(1./d.nativeRate,0,a,b);check(ok&&a==.5&&b==.75,"DAC holds until its next native update edge");}
    for(double cutoff:{3200.,8800.}){SbPro2Adc adc;adc.prepare(192000);double ei=0,eo=0;for(int n=0;n<192000;++n){double x=.25*std::sin(2*pi*cutoff*n/192000.),a,b;adc.filter(x,0,1,cutoff==3200?0:1,a,b);if(n>10000){ei+=x*x;eo+=a*a;}}check(std::abs(10*std::log10(eo/ei)+3.0103)<.2,"ADC filter retains its selected cutoff corner");}
    for(int bits:{8,12,16})for(int trunc:{0,1})for(int density:{4,20,64}){auto v=defaults();v[Bits]=normalized(Bits,bits);v[Truncate]=trunc;v[Density]=normalized(Density,density);Chamber e;e.prepare(22050,v);double energy=0,late=0;bool finite=true;
        for(int n=0;n<22050*7;++n){double a,b;e.tick(n<22050?.2*std::sin(n*.031):0.,0.,a,b);energy+=a*a+b*b;if(n>22050*6)late+=std::abs(a)+std::abs(b);finite=finite&&std::isfinite(a)&&std::isfinite(b)&&std::abs(a)<=1&&std::abs(b)<=1;}
        check(finite&&energy>.001&&late==0&&e.bytes()==256*1024&&e.lastActiveTaps()<=size_t(density),"Shared chamber is bounded and finite across density, precision and truncation");}
    for(double geometry:{0.,.5,1.}){auto v=defaults();v[Geometry]=geometry;v[Size]=1.;v[Decay]=0.;v[Bits]=1.;Chamber e;e.prepare(22050,v);double energy=0;for(int n=0;n<22050*4;++n){double a,b;e.tick(n==0?.75:0.,0.,a,b);energy+=a*a+b*b;}check(energy>.001,"Maximum size with minimum decay remains audible across Geometry");}
    {ReadHeads<1> h;ReadHeads<1>::Positions p{{{10,10}}};h.reset(p,100);bool ok=true;for(int n=0;n<1000;++n){if(n%7==0){auto from=h.from,to=h.to;double blend=h.blend();bool moving=h.moving();p[0][0]=20+n;h.request(p,100);if(moving)ok=ok&&h.from==from&&h.to==to&&h.blend()==blend;}h.advance();}for(int n=0;n<201;++n)h.advance();check(ok&&!h.moving()&&h.to==p,"Retargeting coalesces stationary geometry fades");}
    for(double amount:{0.,.25,.5,1.}){bool good=true;double prev=-2.;for(int n=-20000;n<=20000;++n){double x=n/10000.,y=bodyShape(x,amount);good=good&&y>=prev&&bodyShape(-x,amount)==-y;prev=y;}check(good,"Body remains monotone and odd");}
    std::cout<<"RESULT "<<checks-failures<<" passed / "<<failures<<" failures\n";return failures?1:0;
}
