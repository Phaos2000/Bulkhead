// Copyright (c) 2026 Phaos - phaos2000@pm.me
// Original Bulkhead portions are licensed under the Bulkhead Freeware and
// Source-Available License. See LICENSE and THIRD_PARTY_NOTICES.md in the project root.
#include "Engine.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <limits>
using namespace bulkhead;
int failures=0,checks=0;
// Observe only the reverb contribution through the public signal path.
// The matched ADC-only instance removes the now-always-present direct signal.
struct WetEngine:Engine {
    Engine direct;
    void prepare(double rate,const Values& v=defaults()){Engine::prepare(rate,v);auto d=v;d[Mix]=0.;direct.prepare(rate,d);}
    void reset(){Engine::reset();direct.reset();}
    void set(int id,double value){Engine::set(id,value);if(id!=Mix)direct.set(id,value);}
    void tick(double l,double r,double& a,double& b){double c,d;Engine::tick(l,r,a,b);direct.tick(l,r,c,d);a-=c;b-=d;}
};
void check(bool ok,const char* what){++checks;std::cout<<(ok?"PASS ":"FAIL ")<<what<<"\n";if(!ok)++failures;}
void wav(const std::filesystem::path& path,const std::vector<float>& samples,int sr=48000){
    std::ofstream f(path,std::ios::binary);auto u16=[&](uint16_t n){f.write(reinterpret_cast<char*>(&n),2);};auto u32=[&](uint32_t n){f.write(reinterpret_cast<char*>(&n),4);};
    f.write("RIFF",4);u32(36+uint32_t(samples.size()*4));f.write("WAVEfmt ",8);u32(16);u16(3);u16(2);u32(sr);u32(sr*8);u16(8);u16(32);f.write("data",4);u32(uint32_t(samples.size()*4));f.write(reinterpret_cast<const char*>(samples.data()),samples.size()*4);
}
double excite(int n,int sr){if(n<0||n>sr*.16)return 0.;uint32_t h=uint32_t(n)*747796405u+2891336453u;h=((h>>((h>>28)+4))^h)*277803737u;const double noise=(double((h>>22)^h)/4294967295.)*2-1;const double t=double(n)/sr;return .24*noise*std::exp(-t*95)+.2*std::sin(2*pi*(120*t+27*t*t))*std::exp(-t*42);}
void benchmark(){
    for(int density:{32,64}){
        auto v=defaults();v[Density]=normalized(Density,density);Engine e;e.prepare(48000,v);constexpr int samples=48000*30;double checksum=0;const auto start=std::chrono::steady_clock::now();for(int n=0;n<samples;++n){double a,b;e.tick((n%24000==0)?.3:0.,0.,a,b);checksum+=a+b;}const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();std::cout<<"BENCHMARK "<<density<<" taps / 48 kHz stereo: "<<elapsed<<" s for 30 s audio; "<<elapsed/30*100<<"% of one core; delay memory "<<e.bytes()/1048576.<<" MiB; checksum "<<checksum<<"\n";
    }
}
int main(int argc,char** argv){
    if(argc>1&&std::string(argv[1])=="--benchmark"){NoDenormals guard;benchmark();return 0;}
    NoDenormals guard;std::filesystem::path output=argc>1?argv[1]:"artifacts";std::filesystem::create_directories(output);
    for(double sr:{8000.,44100.,48000.,96000.,192000.,384000.}){
        auto v=defaults();v[Mix]=0.;v[AntiAlias]=0.;v[Dither]=0.;Engine e;e.prepare(sr,v);bool exact=true;double phase=1.,heldL=0.,heldR=0.;
        for(int n=0;n<10000;++n){if(n%211==0)e.set(Output,(n&1)?1.:0.);const double x=std::sin(n*.017)*.23,y=std::cos(n*.011)*.13;double a,b;e.tick(x,y,a,b);phase+=e.actualClock()/sr;while(phase>=1.){phase-=1.;heldL=SbPro2Adc::capture(x);heldR=SbPro2Adc::capture(y);}exact=exact&&a==heldL&&b==heldR;}check(exact,"Mix zero is the clocked 8-bit ADC signal, unaffected by wet-output gain");
    }
    for(int mode=0;mode<2;++mode){
        auto v=defaults();v[Truncate]=mode;Engine e;e.prepare(48000,v);bool silent=true;for(int n=0;n<48000;++n){double a,b;e.tick(0,0,a,b);silent=silent&&a==0&&b==0;}check(silent,"Silence remains exact zero in each arithmetic mode");
    }
    for(int bits:{8,12,16}){
        bool monotone=true,grid=true;int last=-32768;const int step=1<<(16-bits);
        for(int i=0;i<=200000;++i){const double x=2.*i/200000.-1.;const int q=pcmWord(x,bits);monotone=monotone&&q>=last;grid=grid&&q%step==0;last=q;}
        check(monotone&&grid&&pcmWord(-1.,bits)==-32768&&pcmWord(1.,bits)==32768-step,"PCM uses a monotone signed linear grid and saturates without wrapping");
    }
    check(pcmWord(std::numeric_limits<double>::quiet_NaN(),12)==0,"Invalid converter input is contained");
    {
        Arithmetic rounded(10,0),truncated(10,1),blend(10,.5);const int64_t x=100LL*32768;
        check(rounded(x)==128&&truncated(x)==64&&blend(x)==96&&truncated(-x)==-64,"TRUNC discards low product bits symmetrically; transition blends the arithmetic");
        double d=0;check(parseValue(Clock,"11.025 kHz",d)&&plain(Clock,d)==11025&&parseValue(Decay,"25ms",d)&&std::abs(plain(Decay,d)-.025)<1e-12&&parseValue(Bits,"15.6",d)&&plain(Bits,d)==16&&!parseValue(Tone,"NaN",d)&&!parseValue(Size,"abc",d),"Typed values support units, clock choices, whole bits and invalid-input rejection");
    }
    {
        auto v=defaults();v[Mix]=1.;v[Clock]=normalized(Clock,11025);WetEngine e;e.prepare(48000,v);int changes=0,previousChange=-100,minSpacing=1000;double last=0;
        for(int n=0;n<48000;++n){double a,b;e.tick(excite(n%8000,48000),0,a,b);if(a!=last){if(previousChange>=0)minSpacing=std::min(minSpacing,n-previousChange);previousChange=n;++changes;}last=a;}
        check(changes>100&&minSpacing>=3,"Low-clock wet output retains held steps through the independent DAC clock");
        e.set(Clock,1.);double a,b;for(int n=0;n<2600;++n)e.tick(0,0,a,b);check(e.actualClock()==21739.,"Clock ramps settle at the selected period sample rate");
    }
    {
        ReadHeads<1> heads;ReadHeads<1>::Positions p{{{10,10}}};heads.reset(p,100);bool continuous=true;
        for(int n=0;n<1000;++n){if(n%7==0){const auto from=heads.from,to=heads.to;const double before=heads.blend();const bool moving=heads.moving();p[0][0]=20+n;heads.request(p,100);if(moving)continuous=continuous&&heads.from==from&&heads.to==to&&heads.blend()==before;}heads.advance();}
        for(int n=0;n<201;++n)heads.advance();check(continuous&&!heads.moving()&&heads.to==p,"Rapid retargeting coalesces stationary read-head fades without restarting them");
    }
    {
        auto v=defaults();v[Mix]=0.;Engine e;e.prepare(48000,v);double a=0,b=0;bool bound=true;
        e.tick(.253,.253,a,b);e.set(Bypass,1.);for(int n=0;n<1200;++n){double before=a;e.tick(.253,.253,a,b);bound=bound&&std::abs(a-before)<.001;}
        check(bound&&a==.253&&b==.253,"Bypass smoothly returns the ADC direct signal to exact raw input over 25 ms");
    }
    {
        bool good=true;double prev=0;
        for(int n=0;n<=10000;++n){const double x=double(n)/10000.;const double y=bodyShape(x,.5);good=good&&bodyShape(x,0)==x&&bodyShape(-x,.5)==-y&&y>=prev;prev=y;}
        check(good&&bodyShape(.1,.25)>.1&&bodyShape(.1,.25)<.11&&bodyShape(0.,1)==0.,"Body is odd, monotone, silent at zero and adds controlled low-level weight");
    }
    {
        auto v=defaults();v[Mix]=0.;Engine direct,base,boost;direct.prepare(48000,v);v[Mix]=.4;base.prepare(48000,v);v[Output]=normalized(Output,6.);boost.prepare(48000,v);bool correct=true;
        const double g=std::pow(10.,6./20.);
        for(int n=0;n<48000;++n){const double x=excite(n%5000,48000);double a,b,c,d,f,h;direct.tick(x,x,f,h);base.tick(x,x,a,b);boost.tick(x,x,c,d);correct=correct&&std::abs(c-(f+(a-f)*g))<1e-12;}
        check(correct,"Output gain scales only the wet contribution at partial Mix");
    }
    {
        auto v=defaults();v[Bits]=normalized(Bits,16);v[Density]=0.;Chamber sparse,dense;sparse.prepare(22050,v);v[Density]=1.;dense.prepare(22050,v);int sparseHits=0,denseHits=0;
        for(int n=0;n<22050*8;++n){double a,b;sparse.tick(n==0?.5:0,0,a,b);sparseHits+=a!=0||b!=0;dense.tick(n==0?.5:0,0,a,b);denseHits+=a!=0||b!=0;}
        std::cout<<"Density impulse nonzero ticks: "<<sparseHits<<" / "<<denseHits<<"\n";
        check(denseHits>sparseHits*6&&sparse.lastActiveTaps()<=4&&dense.lastActiveTaps()>50,"Density spans sparse four-reflection response to a 64-tap cloud within the period budget");
    }
    {
        auto v=defaults();v[Mix]=1.;v[Body]=0.;v[Bits]=normalized(Bits,12);WetEngine rounded,truncated,switching;rounded.prepare(48000,v);switching.prepare(48000,v);v[Truncate]=1.;truncated.prepare(48000,v);
        double difference=0,maxFirstDifference=0;bool allFinite=true;
        for(int n=0;n<48000*3;++n){const double x=.16*std::sin(n*.014);double a,b,c,d,e,f;rounded.tick(x,x,a,b);truncated.tick(x,x,c,d);if(n==48000)switching.set(Truncate,1.);switching.tick(x,x,e,f);if(n>=48000&&n<48000+48)maxFirstDifference=std::max(maxFirstDifference,std::abs(e-a));if(n>48000*2)difference+=std::abs(a-c);allFinite=allFinite&&std::isfinite(e)&&std::abs(e)<=1.;}
        std::cout<<"TRUNC first millisecond extra difference "<<maxFirstDifference<<", mode difference sum "<<difference<<"\n";
        check(allFinite&&difference>1.&&maxFirstDifference<.015,"TRUNC alters the internal reflection texture without an abrupt full-mode switch");
    }
    {
        bool finite=true;double peak=0;
        for(double rate:{44100.,48000.,96000.,192000.}){Engine e;e.prepare(rate);uint32_t random=37;
            for(int n=0;n<int(rate*3);++n){if(n%137==0){random=random*1664525u+1013904223u;const int id=int((random>>16)%Count);e.set(id,(random&1)?1.:0.);}double a,b;e.tick(.16*std::sin(n*.019),.13*std::sin(n*.021),a,b);finite=finite&&std::isfinite(a)&&std::isfinite(b);peak=std::max({peak,std::abs(a),std::abs(b)});}
        }
        std::cout<<"Rapid automation peak "<<peak<<"\n";check(finite&&peak<4.,"All parameters tolerate rapid full-range automation at 44.1/48/96/192 kHz");
    }
    for(int mode=0;mode<2;++mode){
        auto v=defaults();v[Mix]=1.;v[Truncate]=mode;Engine e;e.prepare(48000,v);double firstEnergy=0,lateEnergy=0;
        for(int n=0;n<48000*9;++n){double a,b;e.tick(n==0?.5:0,n==0?.5:0,a,b);if(n<48000*2)firstEnergy+=a*a+b*b;if(n>48000*8)lateEnergy+=a*a+b*b;}
        check(firstEnergy>1e-6&&lateEnergy==0.,"Impulse excites the reflection cloud and clears to exact silence in both modes");
    }
    for(int p=0;p<presetCount;++p){auto v=preset(p);v[Mix]=normalized(Mix,65.);Engine e;e.prepare(48000,v);const int length=p==3?48000*18:48000*8;std::vector<float> audio(size_t(length)*2);for(int n=0;n<length;++n){double x=excite(n,48000);if(n>=48000&&n<48000*2)x+=excite(n-48000,48000)*.7;double a,b;e.tick(x,x*.87,a,b);audio[n*2]=float(a);audio[n*2+1]=float(b);}wav(output/(std::string("0")+char('1'+p)+"-"+presetNames[p]+".wav"),audio);}
    {
        auto v=defaults();v[Mix]=.65;v[Bits]=normalized(Bits,8);std::vector<float> audio(48000*12*2);
        for(int mode=0;mode<2;++mode){v[Truncate]=mode;Engine e;e.prepare(48000,v);for(int n=0;n<48000*6;++n){double a,b;e.tick(excite(n%48000,48000)*.6,excite(n%48000,48000)*.6,a,b);const int at=mode*48000*6+n;audio[at*2]=float(a);audio[at*2+1]=float(b);}}
        wav(output/"06-TRUNC-8BIT-OFF-THEN-ON.wav",audio);
    }
    {
        auto v=defaults();v[Mix]=1.;v[Bits]=normalized(Bits,16);
        Engine sb,filtered,damped;sb.prepare(48000,v);v[SbFilter]=1.;filtered.prepare(48000,v);
        v[SbFilter]=0.;v[LowDamp]=1.;damped.prepare(48000,v);
        double cardDifference=0,bassDifference=0;bool cleanIdle=true;
        for(int n=0;n<48000*2;++n){
            const double x=n<24000?.28*std::sin(2*pi*110*n/48000.):0.;
            double a,b,c,d,e,f;sb.tick(x,x,a,b);filtered.tick(x,x,c,d);damped.tick(x,x,e,f);
            cardDifference+=std::abs(a-c);bassDifference+=std::abs(a-e);
            if(n>90000)cleanIdle=cleanIdle&&std::isfinite(a)&&std::isfinite(c)&&std::isfinite(e);
        }
        check(cardDifference>1.&&bassDifference>1.&&cleanIdle,"SB filter changes the held output and low damping independently removes bass");
    }
    {
        auto v=defaults();v[Mix]=1.;v[Dither]=1.;Engine e;e.prepare(48000,v);
        bool silent=true;for(int n=0;n<48000;++n){double a,b;e.tick(0,0,a,b);silent=silent&&a==0.&&b==0.;}
        check(silent,"Dither does not seed a DC tail from digital silence");
    }
    {
        bool smooth=true;double worst=0.;
        for(int id:{SbFilter,ToneFilter,AntiAlias,Dither,LowDamp,Body,Truncate,Bits}){
            auto v=defaults();v[Mix]=1.;v[Bits]=normalized(Bits,16);
            Engine reference,changed;reference.prepare(48000,v);changed.prepare(48000,v);
            for(int n=0;n<48000;++n){
                const double x=.2*std::sin(n*.021)+.12*std::sin(n*.093);
                double a,b,c,d;reference.tick(x,x*.8,a,b);changed.tick(x,x*.8,c,d);
            }
            changed.set(id,id==AntiAlias||id==ToneFilter?0.:id==Bits?0.:id==Clock?0.:1.);
            double first=0.;
            for(int n=0;n<96;++n){
                const double x=.2*std::sin((n+48000)*.021)+.12*std::sin((n+48000)*.093);
                double a,b,c,d;reference.tick(x,x*.8,a,b);changed.tick(x,x*.8,c,d);
                first=std::max(first,std::max(std::abs(c-a),std::abs(d-b)));
            }
            std::cout<<"Transition "<<definitions[id].name<<": "<<first<<"\n";worst=std::max(worst,first);smooth=smooth&&std::isfinite(first)&&first<.04;
        }
        std::cout<<"Worst first-two-millisecond control difference "<<worst<<"\n";
        check(smooth,"Filter toggles and bit-depth changes ramp in without an abrupt full-mode switch");
    }
    {
        bool correct=true;int last=-1;
        for(int n=0;n<=10000;++n){const auto code=SbPro2Dac::encode(-1.+2.*n/10000.);correct=correct&&int(code)>=last&&std::floor(SbPro2Dac::decode(code)*128.)==SbPro2Dac::decode(code)*128.;last=code;}
        check(correct&&SbPro2Dac::encode(-2.)==0&&SbPro2Dac::encode(2.)==255&&SbPro2Dac::encode(0.)==128,"SB Pro 2 converter is monotone unsigned 8-bit PCM with exact silence and saturation");
        check(SbPro2Dac::playbackRate(22050)==21739.&&SbPro2Dac::playbackRate(11025)==11111.,"Stereo DSP timer uses integer divisors for nominal 22 and 11 kHz clocks");
        SbPro2Dac dac;dac.prepare(192000.);double a,b;
        dac.push(0.,.25,-.5);dac.render(0.,0.,a,b);bool held=a==.25&&b==-.5;
        dac.push(.5/SbPro2Dac::nativeRate,.5,.75);dac.render(.75/SbPro2Dac::nativeRate,0.,a,b);held=held&&a==.25&&b==-.5;
        dac.render(1./SbPro2Dac::nativeRate,0.,a,b);held=held&&a==.5&&b==.75;
        check(held,"DAC holds both channels until its next 45.454 kHz clock edge");
        bool response=true;
        for(double frequency:{500.,3200.,8000.}){
            SbPro2Dac raw,filtered;raw.prepare(192000.);filtered.prepare(192000.);double dryEnergy=0,wetEnergy=0;
            for(int n=0;n<192000;++n){const double t=n/192000.,x=.5*std::sin(2*pi*frequency*t);double c,d;raw.push(t,x,x);filtered.push(t,x,x);raw.render(t,0.,a,b);filtered.render(t,1.,c,d);if(n>10000){dryEnergy+=a*a;wetEnergy+=c*c;}}
            const double db=10.*std::log10(wetEnergy/dryEnergy);std::cout<<"SB reconstruction at "<<frequency<<" Hz: "<<db<<" dB\n";
            response=response&&(frequency==500.?db>-.2:frequency==3200.?db<-2.8&&db>-3.3:db<-15.);
        }
        check(response,"Post-hold two-pole output filter passes bass, is -3 dB near 3.2 kHz, and darkens highs");
        dac.reset();bool silent=true;for(int n=0;n<1000;++n){dac.render(n/192000.,1.,a,b);silent=silent&&a==0&&b==0;}
        check(silent,"DAC reset clears pending samples, clock phase and analogue filter state");
    }
    {
        auto v=defaults();v[Mix]=1.;v[ToneFilter]=0.;v[AntiAlias]=0.;v[SbFilter]=0.;v[Body]=0.;v[Bits]=1.;Engine e;e.prepare(48000,v);
        check(e.bytes()==512*1024&&plain(Decay,1.)==4.&&plain(Density,1.)==64.,"Reverb fits 512 KiB with four-second high-clock decay and 64-tap limits");
        bool grid=true;int nonzero=0;for(int n=0;n<48000;++n){double a,b;e.tick(excite(n%8000,48000),0,a,b);grid=grid&&a*128.==std::round(a*128.)&&b*128.==std::round(b*128.);nonzero+=a!=0.||b!=0.;}
        check(grid&&nonzero>100,"With ADC/DAC filters disabled the direct-plus-reverb output remains on the 8-bit step grid");
        v[SbFilter]=1.;v[Dither]=1.;e.prepare(48000,v);std::vector<double> first(16000);bool repeat=true;
        for(int n=0;n<16000;++n){double a,b;e.tick(excite(n,48000),0,a,b);first[n]=a;}e.reset();
        for(int n=0;n<16000;++n){double a,b;e.tick(excite(n,48000),0,a,b);repeat=repeat&&a==first[n];}
        check(repeat,"Engine reset is deterministic including DAC and dither state");
        for(int clock=0;clock<2;++clock){v[Clock]=clock;v[Size]=1.;v[Decay]=1.;e.prepare(48000,v);double late=0;for(int n=0;n<48000*13;++n){double a,b;e.tick(n==0?.75:0.,0.,a,b);if(n>48000*12)late+=std::abs(a)+std::abs(b);}check(late==0.,"Maximum chamber clears to silence within its fixed memory at both clocks");}
    }
    {
        auto v=defaults();v[Mix]=1.;Engine changed,reference;changed.prepare(48000,v);reference.prepare(48000,v);
        double a,b,c,d;for(int n=0;n<48000;++n){const double x=.2*std::sin(n*.021);changed.tick(x,x,a,b);reference.tick(x,x,c,d);}
        changed.set(Clock,0.);const double x=.2*std::sin(48000*.021);changed.tick(x,x,a,b);reference.tick(x,x,c,d);
        const bool boundary=a==c&&b==d;double previous=changed.actualClock();bool smooth=true;
        for(int n=1;n<480;++n){changed.tick(.2*std::sin((48000+n)*.021),0.,a,b);const double rate=changed.actualClock();smooth=smooth&&rate<=previous&&previous-rate<1200.&&std::isfinite(a)&&std::isfinite(b);previous=rate;}
        check(boundary&&smooth&&changed.actualClock()==11111.,"Clock change preserves the first sample and ramps monotonically over 8 ms without a phase reset");
    }
    {
        double parsed=0.;const double high=decaySeconds(1.,1.),low=decaySeconds(1.,0.);
        check(high>4.&&high<4.1&&low>7.9&&low<8.&&parseValue(Decay,"7.94s",parsed,0.,true)&&std::abs(decaySeconds(parsed,0.)-7.94)<.01,"Decay display and typed seconds follow the actual card clock");
        check(parseValue(LowDamp,"300Hz",parsed)&&std::abs(plain(LowDamp,parsed)-300.)<1e-9&&plain(LowDamp,0.)==0.,"Tone L is a Hz cutoff with exact bypass at zero");
    }
    {
        auto v=defaults();v[Mix]=1.;v[Width]=1.;v[Hull]=0.;v[Bits]=normalized(Bits,16);v[Dither]=1.;Engine isolated;isolated.prepare(48000,v);bool independent=true;double leftEnergy=0.;
        for(int n=0;n<96000;++n){double a,b;isolated.tick(excite(n%24000,48000),0.,a,b);independent=independent&&b==0.;leftEnergy+=a*a;}
        check(independent&&leftEnergy>.01,"Stereo PCM path preserves channel isolation through ADC, packets, dither and DAC");
        v[Dither]=0.;v[Width]=0.;Engine narrow,wide;narrow.prepare(48000,v);v[Width]=1.;wide.prepare(48000,v);bool equal=true;double difference=0.;
        for(int n=0;n<96000;++n){double a,b,c,d;const double x=excite(n%24000,48000);narrow.tick(x,x,a,b);wide.tick(x,x,c,d);equal=equal&&a==b;difference+=std::abs(c-d);}
        check(equal&&difference>1.,"Width creates stereo spacing at zero Scatter without channel crossfeed");
    }
    {
        SbPro2Adc adc;adc.prepare(48000.);double a,b;bool grid=true;
        for(int n=0;n<10000;++n){const double x=-1.+2.*n/9999.;const double sample=SbPro2Adc::capture(x);grid=grid&&sample*128.==std::round(sample*128.);adc.filter(0.,0.,1.,1.,a,b);}
        check(grid&&a==0.&&b==0.&&SbPro2Adc::capture(-2.)==-1.&&SbPro2Adc::capture(2.)==127./128.,"ADC uses signed view of unsigned 8-bit PCM, saturation and exact silent capture");
        for(double cutoff:{3200.,8800.}){adc.prepare(192000.);double energy=0.,inputEnergy=0.;for(int n=0;n<192000;++n){const double x=.25*std::sin(2*pi*cutoff*n/192000.);adc.filter(x,0.,1.,cutoff==3200.?0.:1.,a,b);if(n>10000){energy+=a*a;inputEnergy+=x*x;}}const double db=10.*std::log10(energy/inputEnergy);check(db>-3.2&&db<-2.8,"ADC input filter has the selected 3.2/8.8 kHz corner");}
    }
    {
        auto v=defaults();v[Mix]=1.;v[Bits]=normalized(Bits,16);v[Body]=0.;WetEngine clean,dithered;clean.prepare(48000,v);v[Dither]=1.;dithered.prepare(48000,v);double energy=0.,bias=0.;
        for(int n=0;n<96000;++n){const double x=.04*std::sin(2*pi*733*n/48000.);double a,b,c,d;clean.tick(x,x,a,b);dithered.tick(x,x,c,d);if(n>=48000){const double error=c-a;energy+=error*error;bias+=error;}}
        const double rms=std::sqrt(energy/48000.);std::cout<<"Dither output difference RMS "<<rms<<", bias "<<bias/48000.<<"\n";
        check(rms>.001&&rms<.03&&std::abs(bias/48000.)<.003,"Dither is audible at the final 8-bit converter even with 16-bit internal math, without large DC bias");
    }
    {
        auto v=defaults();v[Density]=0.;v[Bits]=normalized(Bits,16);v[ToneFilter]=0.;Chamber chamber;chamber.prepare(22050,v);int fragments=0;
        for(int n=0;n<22050*5;++n){double a,b;chamber.tick(n==0?.3:0.,n==0?.3:0.,a,b);fragments+=a!=0.;}
        check(fragments>=10&&chamber.lastActiveTaps()==4,"Minimum density uses signed multi-fragment packets instead of four isolated delay copies");
    }
    // Regression: the envelope must not expire before a large chamber arrives.
    for(int clock:{0,1})for(int bits:{8,12,16}){
        auto v=defaults();v[Mix]=1.;v[Size]=1.;v[Decay]=0.;v[Clock]=clock;v[Bits]=normalized(Bits,bits);v[Body]=0.;
        WetEngine e;e.prepare(48000,v);double peak=0.,energy=0.;bool finite=true;
        for(int n=0;n<96000;++n){double l,r;e.tick(n==0?.75:0.,0.,l,r);peak=std::max(peak,std::abs(l));energy+=l*l;finite=finite&&std::isfinite(l)&&std::isfinite(r);}
        std::cout<<"Large/short chamber "<<clock<<" / "<<bits<<" peak "<<peak<<", energy "<<energy<<"\n";
        check(finite&&peak>.015&&energy>.001,"32x Size with minimum Decay remains audible at both clocks and all bit depths");
    }
    {
        auto v=defaults();v[Mix]=1.;Engine on,legacyOff;on.prepare(48000,v);v[ToneFilter]=0.;legacyOff.prepare(48000,v);bool same=true;
        for(int n=0;n<48000;++n){double a,b,c,d;const double x=excite(n,48000);on.tick(x,x,a,b);legacyOff.tick(x,x,c,d);same=same&&a==c&&b==d;}
        check(same&&canonical(ToneFilter,0.)==1.,"Legacy Tone Off cannot silently disable the main L/H controls");
    }
    {
        bool good=true;
        for(double amount:{0.,.2,.5,.75,1.}){double previous=-2.;for(int n=-20000;n<=20000;++n){const double x=n/10000.;const double y=bodyShape(x,amount);good=good&&std::isfinite(y)&&y>=previous&&bodyShape(-x,amount)==-y;previous=y;}}
        check(good,"Base Body curve stays monotone and symmetric throughout its range");
        for(double clock:{11111.,21739.}){
            double midDb=0.,highDb=0.,gentleDb=0.;
            for(double frequency:{500.,4000.})for(double amount:{.25,1.}){
                BodyStage stage;stage.configure(clock);double source=0.,wet=0.;
                for(int n=0;n<int(clock);++n){const double x=.08*std::sin(2*pi*frequency*n/clock);const double y=stage.tick(0,x,amount);if(n>clock*.2){source+=x*x;wet+=y*y;}}
                const double db=10.*std::log10(wet/source);
                if(frequency==500.&&amount==1.)midDb=db;if(frequency==4000.&&amount==1.)highDb=db;if(frequency==500.&&amount==.25)gentleDb=db;
            }
            std::cout<<"Body "<<clock<<": mid "<<midDb<<" dB / highs "<<highDb<<" dB / default "<<gentleDb<<" dB\n";
            check(midDb>2.&&midDb<12.&&highDb<1.&&std::abs(gentleDb)<.8,"Body adds useful upper-range low-mid weight without boosting highs or overdriving the default");
        }
        BodyStage stage;stage.configure(21739.);bool exact=true;for(int n=0;n<40000;++n){const double x=.5*std::sin(n*.13);exact=exact&&stage.tick(0,x,0.)==x&&stage.tick(1,0.,1.)==0.;}
        check(exact,"Body Off is exact and the added low-mid path preserves channel isolation");
    }
    {
        auto v=defaults();v[Mix]=1.;v[Body]=0.;v[Width]=0.;v[Geometry]=0.;v[Bits]=1.;v[Hull]=0.;WetEngine orderly,scattered;orderly.prepare(48000,v);v[Hull]=1.;scattered.prepare(48000,v);
        double source=0.,difference=0.;bool isolated=true;
        for(int n=0;n<96000;++n){double a,b,c,d;const double x=excite(n,48000);orderly.tick(x,0.,a,b);scattered.tick(x,0.,c,d);source+=a*a;difference+=(c-a)*(c-a);isolated=isolated&&b==0.&&d==0.;}
        const double relative=std::sqrt(difference/std::max(source,1.e-12));std::cout<<"Scatter relative response difference "<<relative<<"\n";
        check(relative>.5&&isolated,"Scatter independently changes fragment spacing and reflection texture with zero Width and Geometry");
        Engine changed,reference;v[Hull]=0.;changed.prepare(48000,v);reference.prepare(48000,v);
        for(int n=0;n<48000;++n){double a,b,c,d;const double x=.15*std::sin(n*.07);changed.tick(x,x,a,b);reference.tick(x,x,c,d);}
        changed.set(Hull,1.);double worst=0.;for(int n=0;n<96;++n){double a,b,c,d;const double x=.15*std::sin((48000+n)*.07);changed.tick(x,x,a,b);reference.tick(x,x,c,d);worst=std::max(worst,std::abs(a-c));}
        check(worst<.04,"Scatter changes crossfade complete fragment positions without an abrupt read-head jump");
    }
    {
        bool monotone=true;double last=-1.;for(int n=-30000;n<=30000;++n){const double x=n/10000.;const double y=bodySoftLimit(x);monotone=monotone&&std::isfinite(y)&&y>=last&&bodySoftLimit(-x)==-y&&std::abs(y)<=.9800000001;last=y;}
        check(monotone&&bodySoftLimit(.5)==.5,"Body soft limiter is symmetric, monotone, transparent below its knee and bounded");
        BodyStage stage;stage.configure(21739.);double peak=0.;bool silent=true;for(int n=0;n<43500;++n){const double y=stage.tick(0,.9*std::sin(2*pi*350*n/21739.),1.);peak=std::max(peak,std::abs(y));silent=silent&&stage.tick(1,0.,1.)==0.;}
        check(peak>.8&&peak<=.981&&silent,"Maximum Body gently limits high-level low-mid material without channel bleed");
        stage.reset();double input=0.,output=0.;for(int n=0;n<21739;++n){const double x=.05*std::sin(2*pi*110*n/21739.);const double y=stage.tick(0,x,1.);if(n>4000){input+=x*x;output+=y*y;}}
        const double bassDb=10.*std::log10(output/input);std::cout<<"Body 110 Hz gain "<<bassDb<<" dB\n";
        check(bassDb>2.&&bassDb<12.,"Body restores useful bass weight alongside its wider low-mid range");
    }
    {
        auto v=defaults();v[Mix]=0.;v[Dither]=1.;Engine direct,changed,half,full;direct.prepare(48000,v);changed.prepare(48000,v);v[Mix]=.5;half.prepare(48000,v);v[Mix]=1.;full.prepare(48000,v);
        bool independent=true,additive=true;double wetEnergy=0.;
        for(int n=0;n<96000;++n){
            if(n%997==0)for(int id:{Size,Decay,Density,Geometry,Hull,Tone,Width,Output,Bits,Body,Truncate,LowDamp,SbFilter})changed.set(id,(n/997)&1?1.:0.);
            const double x=excite(n%12000,48000),y=x*.63;double a,b,c,d,h,j,f,g;direct.tick(x,y,a,b);changed.tick(x,y,c,d);half.tick(x,y,h,j);full.tick(x,y,f,g);
            independent=independent&&a==c&&b==d;additive=additive&&std::abs(h-(a+.5*(f-a)))<1.e-12&&std::abs(j-(b+.5*(g-b)))<1.e-12;wetEnergy+=(f-a)*(f-a);
        }
        check(independent,"ADC direct signal including dither is independent of every reverb-only control");
        check(additive&&wetEnergy>.01,"Mix adds reverb linearly while retaining the full ADC direct signal at 0, 50 and 100 percent");
    }
    for(int clock:{0,1})for(int filter:{0,1,2}){
        auto v=defaults();v[Mix]=0.;v[Clock]=clock;v[AntiAlias]=filter?1.:0.;v[AdcFilter]=filter==2?1.:0.;Engine e;e.prepare(48000,v);SbPro2Adc oracle;oracle.prepare(48000.);double phase=1.,heldL=0.,heldR=0.;bool same=true;
        for(int n=0;n<24000;++n){const double x=.37*std::sin(n*.79),y=.19*std::cos(n*.17);double a,b,c,d;e.tick(x,y,a,b);oracle.filter(x,y,v[AntiAlias],v[AdcFilter],c,d);phase+=e.actualClock()/48000.;while(phase>=1.){phase-=1.;heldL=SbPro2Adc::capture(c);heldR=SbPro2Adc::capture(d);}same=same&&a==heldL&&b==heldR;}
        check(same,"Direct path includes the selected ADC filter, quantization and sample hold at either clock");
    }
    {
        auto v=defaults();v[Mix]=1.;v[Bypass]=1.;v[AntiAlias]=1.;v[Dither]=1.;Engine e;e.prepare(48000,v);bool exact=true;
        for(int n=0;n<20000;++n){const double x=.137*std::sin(n*.017),y=.219*std::cos(n*.113);double a,b;e.tick(x,y,a,b);exact=exact&&a==x&&b==y;}
        check(exact,"Full bypass remains bit-exact raw input even with the ADC filter and dither enabled");
    }
    // Geometry must change structure on its own, keep energy useful, and
    // preserve the silent channel through the complete ADC/chamber/DAC path.
    for(int clock:{0,1})for(double scatter:{0.,1.}){
        std::array<std::vector<double>,3> responses;
        double energy[3]{};bool isolated=true;
        for(int shape=0;shape<3;++shape){
            auto v=defaults();v[Mix]=1.;v[Body]=0.;v[Bits]=1.;v[Width]=0.;v[Hull]=scatter;v[Clock]=clock;v[Geometry]=shape*.5;
            WetEngine e;e.prepare(48000,v);responses[shape].resize(48000*4);
            for(int n=0;n<int(responses[shape].size());++n){double a,b;e.tick(excite(n,48000),0.,a,b);responses[shape][n]=a;energy[shape]+=a*a;isolated=isolated&&b==0.&&std::isfinite(a)&&std::abs(a)<=1.;}
            if(clock==1&&scatter==0.){
                std::vector<float> audio(responses[shape].size()*2);
                for(size_t n=0;n<responses[shape].size();++n)audio[n*2]=audio[n*2+1]=float(responses[shape][n]);
                wav(output/(std::string("Geometry-")+std::to_string(shape*50)+"-wet.wav"),audio);
            }
        }
        double smallestDifference=100.;
        for(int shape=1;shape<3;++shape){double difference=0.;for(size_t n=0;n<responses[shape].size();++n){const double d=responses[shape][n]-responses[shape-1][n];difference+=d*d;}smallestDifference=std::min(smallestDifference,std::sqrt(difference/std::max(energy[shape-1],1.e-12)));}
        const double levelSpread=10.*std::log10(*std::max_element(energy,energy+3)/std::max(*std::min_element(energy,energy+3),1.e-12));
        std::cout<<"Geometry clock "<<clock<<" scatter "<<scatter<<": smallest response difference "<<smallestDifference<<", level spread "<<levelSpread<<" dB\n";
        check(isolated&&smallestDifference>.65&&levelSpread<6.&&*std::min_element(energy,energy+3)>.01,"Geometry changes both halves of its range at either clock/Scatter extreme without losing level or channel isolation");
    }
    {
        bool smooth=true;double worst=0.;
        for(int direction:{0,1}){
            auto v=defaults();v[Geometry]=direction;v[Hull]=0.;v[Bits]=1.;Chamber reference,changed;reference.prepare(22050,v);changed.prepare(22050,v);
            for(int n=0;n<44100;++n){double a,b,c,d;const double x=.12*std::sin(n*.031)+.05*std::sin(n*.117);reference.tick(x,0.,a,b);changed.tick(x,0.,c,d);}
            changed.set(Geometry,1.-direction);
            for(int n=0;n<44;++n){double a,b,c,d;const double x=.12*std::sin((44100+n)*.031)+.05*std::sin((44100+n)*.117);reference.tick(x,0.,a,b);changed.tick(x,0.,c,d);worst=std::max(worst,std::abs(a-c));smooth=smooth&&d==0.&&std::isfinite(c);if(n==0)smooth=smooth&&a==c;}
        }
        std::cout<<"Geometry first two milliseconds difference "<<worst<<"\n";
        check(smooth&&worst<.03,"Geometry fades positions and fragment weights together in both directions without a boundary jump");
    }
    for(double geometry:{0.,.5,1.}){
        auto v=defaults();v[Geometry]=geometry;v[Hull]=1.;v[Size]=1.;v[Decay]=0.;v[Mix]=1.;v[Body]=0.;v[Clock]=0.;WetEngine e;e.prepare(48000,v);double energy=0.,late=0.;bool finite=true;
        for(int n=0;n<48000*13;++n){double a,b;e.tick(n==0?.75:0.,0.,a,b);energy+=a*a;finite=finite&&std::isfinite(a)&&b==0.;if(n>48000*12)late+=std::abs(a);}
        check(finite&&energy>.001&&late==0.,"Every Geometry region remains audible at maximum Size/minimum Decay and clears within fixed memory");
    }
    benchmark();
    std::cout<<"RESULT "<<checks-failures<<" passed / "<<failures<<" failures\n";return failures?1:0;
}
