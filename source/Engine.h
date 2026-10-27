// Copyright (c) 2026 Phaos - phaos2000@pm.me
// Original Bulkhead portions are licensed under the Bulkhead Freeware and
// Source-Available License. See LICENSE and THIRD_PARTY_NOTICES.md in the project root.
#pragma once
#include "Parameters.h"
#include "SbPro2Dac.h"
#include "SbPro2Adc.h"
#include <vector>
#include <cstdint>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#if defined(_M_X64) || defined(__SSE__)
#include <xmmintrin.h>
#endif

namespace bulkhead {
inline constexpr double pi=3.14159265358979323846;
inline int nearestInt(double x) { return int(x<0.?x-.5:x+.5); }
struct NoDenormals {
#if defined(_M_X64) || defined(__SSE__)
    unsigned old=_mm_getcsr();
    NoDenormals(){_mm_setcsr(old|0x8040u);}
    ~NoDenormals(){_mm_setcsr(old);}
#endif
};
struct Ramp {
    double value=0,target=0,step=0;int left=0;
    void reset(double v){value=target=v;step=0;left=0;}
    void aim(double v,int samples){if(v==target)return;target=v;left=std::max(1,samples);step=(v-value)/left;}
    double next(){if(left>0&&--left==0)value=target;else if(left>0)value+=step;return value;}
};
// Track only parameters that are actually moving. Settled processing advances
// no parameter ramps, while every automated ramp retains the original arithmetic.
struct ParameterRamps {
    std::array<Ramp,Count> values{};
    static_assert(Count<=32,"Parameter ramp mask must cover every parameter");
    uint32_t moving=0;
    Ramp& operator[](int id){return values[id];}
    const Ramp& operator[](int id)const{return values[id];}
    void reset(const Values& v){moving=0;for(int i=0;i<Count;++i)values[i].reset(v[i]);}
    void aim(int id,double value,int length){
        values[id].aim(value,length);
        if(values[id].left>0)moving|=uint32_t(1)<<id;
    }
    void advance(){
        uint32_t pending=moving;
        while(pending){unsigned id=0;
#if defined(_MSC_VER)
            unsigned long first=0;_BitScanForward(&first,pending);id=unsigned(first);
#elif defined(__GNUC__)
            id=unsigned(__builtin_ctz(pending));
#else
            while(!(pending&(uint32_t(1)<<id)))++id;
#endif
            values[id].next();
            if(values[id].left==0)moving&=~(uint32_t(1)<<id);
            pending&=pending-1;
        }
    }
};
// Signed linear PCM, matching the storage model of PC sample processing.
// Integer settings produce exactly 8..16-bit samples. During automation only,
// adjacent word lengths are crossfaded before writing the 16-bit container.
inline int16_t pcmWord(double x,int bits,bool truncate=false) {
    if(!std::isfinite(x))return 0;
    bits=std::clamp(bits,8,16);const int step=1<<(16-bits);
    const double code=std::clamp(x,-1.,1.)*(32768./step);
    const int q=(truncate?int(code):roundedPcmCode(code))*step;
    return int16_t(std::clamp(q,-32768,32768-step));
}
inline int16_t pcmWord(double x,double bits,bool truncate=false) {
    const int low=std::clamp(int(std::floor(bits)),8,16);
    const double blend=std::clamp(bits-low,0.,1.);
    const int a=pcmWord(x,low,truncate);
    if(blend==0.)return int16_t(a);
    const int b=pcmWord(x,std::min(16,low+1),truncate);
    return int16_t(std::clamp(int(std::lround(a+(b-a)*blend)),-32768,32767));
}
inline double linearPCM(double x,double bits){return pcmWord(x,bits)/32768.;}

// Reduce Q30 products to the selected fixed-point word length. TRUNC uses
// signed truncation toward zero (no wraparound or negative DC bias). Both modes
// operate inside the filter and reflection summation, before output conversion.
// Q30 products are bounded below INT32_MAX, including three-fragment packets;
// use the 486's 32-bit arithmetic rather than unnecessary 64-bit operations.
// Only transitions blend modes/adjacent bit depths; steady processing is integer.
struct Arithmetic {
    int low=10; double fraction=0,truncate=0;
    struct Scale {int step,shift;uint32_t half;};
    static Scale scale(int bits){return {1<<(16-bits),31-bits,uint32_t(1)<<(30-bits)};}
    Scale lower,upper;
    Arithmetic(double bits,double mode):low(std::clamp(int(bits),8,16)),fraction(bits-low),truncate(clean(mode)),lower(scale(low)),upper(scale(std::min(16,low+1))){}
    int word(int32_t q30,const Scale& s)const {
        const int step=s.step,shift=s.shift,sign=q30<0?-1:1;
        const uint32_t magnitude=q30<0?0u-uint32_t(q30):uint32_t(q30);
        const int t=sign*int(magnitude>>shift)*step;
        if(truncate==1.)return t;
        const int r=sign*int((magnitude+s.half)>>shift)*step;
        return truncate==0.?r:int(std::lround(r+(t-r)*truncate));
    }
    int operator()(int32_t q30)const {
        const int a=word(q30,lower);
        return fraction==0.?a:int(std::lround(a+(word(q30,upper)-a)*fraction));
    }
};
// Steady reflection kernels use the same integer formula without testing the
// precision/mode for every product. Transitions retain the general arithmetic.
template<bool Truncate> struct FixedArithmetic {
    Arithmetic::Scale scale;
    int operator()(int32_t q30)const {
        const int sign=q30<0?-1:1;
        const uint32_t magnitude=q30<0?0u-uint32_t(q30):uint32_t(q30);
        return sign*int((magnitude+(Truncate?0u:scale.half))>>scale.shift)*scale.step;
    }
};

// All geometry changes crossfade complete, stationary sets of read positions.
// Requests made during a fade are coalesced and taken up when that fade ends.
template<size_t N,size_t Fields=2> class ReadHeads {
public:
    using Positions=std::array<std::array<int,Fields>,N>;
    void reset(const Positions& p,int length){from=to=pending=p;fadeLength=requestedFade=std::max(8,length);fade=fadeLength;}
    void request(const Positions& p,int length){pending=p;requestedFade=std::max(8,length);if(fade==fadeLength&&to!=pending)start();}
    double blend()const{if(fade==fadeLength)return 1.;const double t=double(fade)/fadeLength;return t*t*(3.-2.*t);}
    void advance(){if(fade<fadeLength&&++fade==fadeLength){from=to;if(pending!=to)start();}}
    bool moving()const{return fade<fadeLength;}
    Positions from{},to{};
private:
    void start(){from=to;to=pending;fade=0;fadeLength=requestedFade;}
    Positions pending{};int fade=1,fadeLength=1,requestedFade=1;
};

// A fixed-budget cloud of three-fragment signed reflection packets: no modal resonators, recirculating
// combs, allpass wash, convolution FFT or modulation. Era-plausible operations:
// int16 sample memory, Q15 tap coefficients and wide integer sums.
// Modern conveniences are tail-preserving geometry fades and parameter ramps.
class Chamber {
public:
    static constexpr int maxTaps=64;
    static constexpr int capacity=131072; // 256 KiB: one signed 16-bit wet history.
    void prepare(double rate,const Values& v=defaults()) {
        maxRate=sr=rate;targets=v;for(auto& n:targets)n=clean(n);
        data.assign(capacity,0);
        reset();
    }
    void reset() {
        std::fill(data.begin(),data.end(),0);
        write=validHistory=0;low[0]=lowBand[0]=0;
        lowEnable.reset(targets[LowDamp]>0.?1.:0.);controlClock=0;cached.fill(-1.);cachedRate=-1.;
        controls.reset(targets);
        configure(true);
    }
    void set(int id,double value) {
        if(id<0||id>=Count||!std::isfinite(value))return;
        targets[id]=canonical(id,value);controls.aim(id,targets[id],std::max(1,int(sr*.05)));
        if(id==LowDamp)lowEnable.aim(targets[id]>0.?1.:0.,std::max(1,int(sr*.05)));
    }
    void set(const Values& v){for(int i=0;i<Count;++i)set(i,v[i]);}
    void clock(double rate){sr=std::clamp(rate,1.,maxRate);}
    const Values& parameters()const{return targets;}
    size_t bytes()const{return data.size()*sizeof(int16_t);}
    size_t activeBytes()const{return bytes();}
    size_t lastActiveTaps()const{return activeTaps;}
    void tick(double inL,double inR,double& outL,double& outR) {
        controls.advance();
        if(controlClock--==0){configure(false);controlClock=31;}
        const double bits=cachedBits;
        const Arithmetic& arithmetic=cachedArithmetic;
        if(filter.left>0)filterWord=std::clamp(nearestInt(filter.next()*32768),1,32767);
        if(bassFilter.left>0)bassWord=std::clamp(nearestInt(bassFilter.next()*32768),1,32767);
        const int a=filterWord,bassA=bassWord;
        const double lowMix=lowEnable.next();
        const double input[1]={(inL+inR)*.5};
        {const int ch=0;
            const int x=pcmWord(input[ch],bits);
            int bass=arithmetic(int32_t(lowBand[ch])*(32768-bassA)+int32_t(x)*bassA);
            if(x==0&&std::abs(bass)>=std::abs(lowBand[ch]))bass=0;
            lowBand[ch]=std::clamp(bass,-32768,32767);
            const int damp=std::clamp(nearestInt(lowMix*32768),0,32768);
            const int source=std::clamp(x-int(int32_t(lowBand[ch])*damp/32768),-32768,32767);
            int filtered=arithmetic(int32_t(low[ch])*(32768-a)+int32_t(source)*a);
            // A rounded recursive filter must not stick at one silent LSB.
            if(x==0&&std::abs(filtered)>=std::abs(low[ch]))filtered=0;
            low[ch]=std::clamp(filtered,-32768,32767);
            data[write]=pcmWord(low[ch]/32768.,bits);
        }
        int32_t sum[2]{};
        const bool moving=heads.moving();
        const int fadeQ=moving?nearestInt(heads.blend()*32768):32768;
        if(arithmetic.fraction==0.&&arithmetic.truncate==0.)reflectShared(sum,moving,fadeQ,FixedArithmetic<false>{arithmetic.lower});
        else if(arithmetic.fraction==0.&&arithmetic.truncate==1.)reflectShared(sum,moving,fadeQ,FixedArithmetic<true>{arithmetic.lower});
        else reflectShared(sum,moving,fadeQ,arithmetic);
        write=(write+1)&(capacity-1);
        validHistory=std::min(capacity,validHistory+1);
        heads.advance();
        if(tapReductionTicks>0&&--tapReductionTicks==0)tapLimit=targetTapLimit;
        // Stereo comes from the balanced packet pan coefficients.
        outL=linearPCM(sum[0]/32768.*1.4,bits);
        outR=linearPCM(sum[1]/32768.*1.4,bits);
    }
private:
    // Shared history is read once per fragment. Stereo comes from fixed packet
    // pan coefficients, precombined with packet gain at control rate. No extra
    // packet-gain multiply or intermediate packet clipping in this new mode.
    template<class Quantize> void reflectShared(int32_t (&sum)[2],bool moving,int fadeQ,const Quantize& arithmetic){
        if(moving)reflectShared<true>(sum,fadeQ,arithmetic);else reflectShared<false>(sum,fadeQ,arithmetic);
    }
    template<bool Moving,class Quantize> void reflectShared(int32_t (&sum)[2],int fadeQ,const Quantize& arithmetic){
        activeTaps=0;
        for(int i=0;i<tapLimit;++i){
            const auto& first=heads.to[i*3];const auto& oldFirst=heads.from[i*3];
            const bool currentActive=first[1]||first[2],previousActive=Moving&&(oldFirst[1]||oldFirst[2]);
            if(!currentActive&&!previousActive)continue;++activeTaps;
            int32_t current[2]{},previous[2]{};
            for(int f=0;f<3;++f){const int k=i*3+f;const auto& p=heads.to[k];
                if(currentActive){const int x=read(p[0]);for(int ch=0;ch<2;++ch)current[ch]+=int32_t(x)*p[1+ch];}
                if constexpr(Moving){if(previousActive){const auto& q=heads.from[k];const int x=read(q[0]);for(int ch=0;ch<2;++ch)previous[ch]+=int32_t(x)*q[1+ch];}}
            }
            for(int ch=0;ch<2;++ch){int sample=arithmetic(current[ch]);
                if constexpr(Moving)sample=int((int32_t(arithmetic(previous[ch]))*(32768-fadeQ)+int32_t(sample)*fadeQ)/32768);
                sum[ch]+=sample;
            }
        }
    }
    int read(int delay)const{return delay>validHistory?0:data[(write-delay)&(capacity-1)];}
    static double coefficient(double value,double truncate) {
        const double coarse=std::trunc(value*1024.)/1024.;
        return value+truncate*(coarse-value);
    }
    static unsigned reversed(unsigned n){unsigned r=0;for(int b=0;b<6;++b){r=(r<<1)|(n&1);n>>=1;}return r;}
    static double random(unsigned n){n=n*747796405u+2891336453u;n=((n>>((n>>28)+4))^n)*277803737u;return double((n>>22)^n)/4294967295.;}
    void configure(bool snap) {
        Values current{};for(int i=0;i<Count;++i)current[i]=controls[i].value;
        if(!snap&&current==cached&&sr==cachedRate)return;
        cached=current;cachedRate=sr;
        cachedBits=8.+8.*current[Bits];cachedArithmetic=Arithmetic(cachedBits,current[Truncate]);
        const double size=plain(Size,targets[Size]),decay=plain(Decay,targets[Decay]);
        const double density=plain(Density,controls[Density].value);
        const double scatter=controls[Hull].value,geometry=targets[Geometry],width=controls[Width].value;
        const double exponent=std::clamp(1.7-std::log2(size)/8.,.85,2.3);
        const double span=decay*1.15+size*.025,early=.003*size;
        typename ReadHeads<maxTaps*3,3>::Positions positions{};
        // Geometry folds the cloud into eight unevenly placed groups. The
        // bit-reversed tap order keeps those groups populated at low density.
        // All mapping/normalization happens at control rate, never per read.
        const double folding=.88*geometry*geometry;
        const double fragmentShape[]={1.,.45+.45*geometry,-(.15+.60*geometry)};
        const double fragmentNorm=1./std::sqrt(1.+fragmentShape[1]*fragmentShape[1]+fragmentShape[2]*fragmentShape[2]);
        int fragmentWords[3]{};
        for(int f=0;f<3;++f)fragmentWords[f]=nearestInt(fragmentShape[f]*fragmentNorm*32767.);
        std::array<double,maxTaps> weights{},times{},enabledWeights{};double energy=0,firstReflection=1.e30;
        for(int i=0;i<maxTaps;++i) {
            const double u=(reversed(unsigned(i))+.5)/maxTaps;
            const unsigned group=unsigned(u*8.);
            const double centre=(group+.5+(random(group+701)-.5)*.60)/8.;
            const double folded=u+folding*(centre-u);
            const double jitter=(random(i+17)-.5)*scatter*.40;
            const double time=early+span*std::pow(std::clamp(folded+jitter*u*(1-u),.0001,.9999),exponent);
            const double fragmentScale=(.2+2.8*scatter*scatter)*(.18+1.62*geometry);
            const int first=std::max(1,nearestInt((23+(i*79)%239)*fragmentScale));
            const int second=first+std::max(1,nearestInt((47+(i*113)%397)*fragmentScale));
            const int offsets[]={0,first,second};
            const int base=std::clamp(int(maxRate*std::max(1./maxRate,time)),1,capacity-1-second);
            for(int fragment=0;fragment<3;++fragment)positions[i*3+fragment][0]=base+offsets[fragment];
            // A continuous count admits one reflection at a time without switches.
            const double enabled=std::clamp(density-i,0.,1.);
            const double polarity=(i&2)?1.-1.8*scatter:1.;
            times[i]=time;enabledWeights[i]=enabled*polarity;
            if(enabled>0.)firstReflection=std::min(firstReflection,time);
        }
        // Decay starts at the first active packet, not before sound arrives.
        // Rescaling by a common onset keeps relative gains while avoiding
        // underflow at large Size / short Decay before Q15 normalization.
        for(int i=0;i<maxTaps;++i){
            weights[i]=enabledWeights[i]*std::exp(-6.90775527898*std::max(0.,times[i]-firstReflection)/decay);
            energy+=weights[i]*weights[i];
        }
        const double normalization=1./std::sqrt(std::max(energy,1.e-12));
        std::array<std::array<double,2>,maxTaps> sharedGains{};
        double channelEnergy[2]{};
        {
            for(int i=0;i<maxTaps;++i){
                const double g=coefficient(weights[i]*normalization,controls[Truncate].value);
                const double pan=(random(i+217)-.5)*width*pi*.5;
                sharedGains[i]={g*std::cos(pi*.25+pan),g*std::sin(pi*.25+pan)};
                for(int ch=0;ch<2;++ch)channelEnergy[ch]+=sharedGains[i][ch]*sharedGains[i][ch];
            }
            // Balance the actual decay-weighted packet set, not the unweighted
            // pan distribution. Preserve total nominal energy, half per side.
            // Each normalized packet gain remains bounded by sqrt(0.5).
            for(int ch=0;ch<2;++ch){const double scale=channelEnergy[ch]>0.?std::sqrt(.5/channelEnergy[ch]):0.;
                for(auto& gain:sharedGains)gain[ch]*=scale;
            }
        }
        const int wantedTaps=std::clamp(int(std::ceil(density)),4,maxTaps);
        if(snap){tapLimit=targetTapLimit=wantedTaps;tapReductionTicks=0;}
        else {tapLimit=std::max(tapLimit,wantedTaps);targetTapLimit=wantedTaps;tapReductionTicks=std::max(32,int(sr*.025))+32;}
        for(int i=0;i<maxTaps;++i)for(int f=0;f<3;++f)for(int ch=0;ch<2;++ch)
            positions[i*3+f][1+ch]=std::clamp(nearestInt(fragmentWords[f]*sharedGains[i][ch]),-32768,32767);
        if(snap)heads.reset(positions,int(sr*.018));else heads.request(positions,int(sr*.018));
        const double cutoff=std::min(sr*.45,plain(Tone,controls[Tone].value));
        const double a=std::max(1./32768.,coefficient(1.-std::exp(-2*pi*cutoff/sr),controls[Truncate].value));
        const double bassCutoff=std::min(sr*.4,plain(LowDamp,controls[LowDamp].value));
        const double bassA=std::max(1./32768.,coefficient(1.-std::exp(-2*pi*bassCutoff/sr),controls[Truncate].value));
        if(snap){filter.reset(a);bassFilter.reset(bassA);filterWord=std::clamp(nearestInt(a*32768),1,32767);bassWord=std::clamp(nearestInt(bassA*32768),1,32767);}
        else{filter.aim(a,32);bassFilter.aim(bassA,32);}
    }
    double maxRate=22050,sr=22050,cachedRate=-1,cachedBits=12.;
    Arithmetic cachedArithmetic{12.,0.};
    int write=0,validHistory=0,low[2]{},lowBand[2]{},controlClock=0,tapLimit=maxTaps,targetTapLimit=maxTaps,tapReductionTicks=0;
    size_t activeTaps=0;
    std::vector<int16_t> data;
    ReadHeads<maxTaps*3,3> heads;
    int filterWord=1,bassWord=1;
    Ramp filter,bassFilter,lowEnable;ParameterRamps controls;Values targets=defaults(),cached{};
};
inline double bodyShape(double x,double amount) {
    if(!std::isfinite(x))return 0.;amount=clean(amount);
    const double drive=1.+.35*amount;
    const double t=std::clamp(x*drive,-1.25,1.25);
    const double shaped=t-.2*t*t*t;
    return x+amount*(std::clamp(shaped,-.98,.98)-x);
}
inline double bodySoftLimit(double x){
    if(!std::isfinite(x))return 0.;const double magnitude=std::abs(x);
    if(magnitude<=.82)return x;
    const double knee=std::clamp((magnitude-.82)/.24,0.,1.);
    return std::copysign(.82+.24*(knee-knee*knee*knee/3.),x);
}
// Body preserves the gentle base curve and adds low-mid weight only near
// the upper end. Four one-pole states per channel, no envelope follower,
// FFT, lookahead, oversampling or per-sample transcendental functions.
class BodyStage {
public:
    void reset(){for(auto& channel:state)channel.fill(0.);}
    void configure(double rate){
        if(rate==sampleRate)return;sampleRate=rate;
        bassA=1.-std::exp(-2*pi*120./rate);
        midA=1.-std::exp(-2*pi*1400./rate);
    }
    double tick(int ch,double x,double amount){
        if(!std::isfinite(x))x=0.;amount=clean(amount);auto& f=state[ch];
        f[0]+=bassA*(x-f[0]);f[1]+=midA*(x-f[1]);
        const double band=f[1]-f[0],upper=amount*amount*amount;
        const double t=std::clamp(band*(1.+2.*upper),-1.,1.);
        const double difference=t-t*t*t/3.-band;
        f[2]+=midA*(difference-f[2]);f[3]+=midA*(f[2]-f[3]);
        // Prevent filters from lingering at inaudible denormal levels.
        for(auto& v:f)if(std::abs(v)<1.e-20)v=0.;
        if(amount==0.)return x;
        const double base=bodyShape(x,std::min(amount,.5));
        const double shaped=(base+1.5*upper*f[3])*(1.-.12*upper);
        const double limited=bodySoftLimit(shaped);
        return shaped+std::min(1.,amount*4.)*(limited-shaped);
    }
private:
    std::array<std::array<double,4>,2> state{};
    double sampleRate=-1.,bassA=0.,midA=0.;
};
class Engine {
public:
    void prepare(double rate,const Values& v=defaults()) {
        hostRate=rate;targets=v;for(auto& n:targets)n=clean(n);
        chamber.prepare(22050.,targets);dac.prepare(hostRate);adc.prepare(hostRate);reset();
    }
    void reset() {
        controls.reset(targets);
        clockControl.reset(SbPro2Dac::playbackRate(plain(Clock,targets[Clock])));
        chamber.clock(clockControl.value);chamber.reset();
        phase=1.;seconds=0.;rng=0x8372a4cdu;adcRng=0x591fc327u;
        dac.reset();adc.reset();body.reset();controlClock=0;configure(true);
    }
    void set(int id,double value) {
        if(id<0||id>=Count||!std::isfinite(value))return;
        targets[id]=canonical(id,value);
        controls.aim(id,targets[id],std::max(1,int(hostRate*(id==Bypass?.025:.05))));
        if(id==Clock)clockControl.aim(SbPro2Dac::playbackRate(plain(Clock,targets[Clock])),std::max(1,int(hostRate*.008)));
        if(id<=Width||id==Bits||id==Truncate||id==LowDamp)chamber.set(id,targets[id]);
    }
    void set(const Values& v){for(int id=0;id<Count;++id)set(id,v[id]);}
    const Values& parameters()const{return targets;}
    size_t bytes()const{return chamber.bytes();}
    size_t activeBytes()const{return chamber.bytes();}
    double actualClock()const{return clockRate;}
    void tick(double l,double r,double& outL,double& outR) {
        controls.advance();clockControl.next();
        if(controlClock--==0){configure(false);controlClock=31;}
        const double bypass=controls[Bypass].value,gain=outputGain.next();
        l=std::isfinite(l)?l:0.;r=std::isfinite(r)?r:0.;
        double fedL=0,fedR=0;
        adc.filter(l,r,controls[AntiAlias].value,controls[AdcFilter].value,fedL,fedR);
        seconds+=1./hostRate;phase+=clockRate/hostRate;
        while(phase>=1.) {
            phase-=1.;const double bits=8.+8.*controls[Bits].value;
            const double dither=controls[Dither].value*double(1<<(16-int(std::lround(bits))))/32768.;
            const double adcDither=controls[Dither].value/128.;
            const double sourceL=SbPro2Adc::capture(fedL+(fedL==0.?0.:tpdf(adcRng)*adcDither));
            const double sourceR=SbPro2Adc::capture(fedR+(fedR==0.?0.:tpdf(adcRng)*adcDither));
            const double noiseL=sourceL==0.?0.:tpdf(rng)*dither,noiseR=sourceR==0.?0.:tpdf(rng)*dither;
            double wetL,wetR;
            chamber.tick(linearPCM((sourceL+noiseL)*(1.-bypass),bits),linearPCM((sourceR+noiseR)*(1.-bypass),bits),wetL,wetR);
            const double shapedL=linearPCM(body.tick(0,wetL,controls[Body].value),bits);
            const double shapedR=linearPCM(body.tick(1,wetR,controls[Body].value),bits);
            // Captured dry and processed wet share the one physical output path.
            const double mix=controls[Mix].value;
            const double outputL=sourceL+mix*shapedL*gain,outputR=sourceR+mix*shapedR*gain;
            const double outputDither=controls[Dither].value/128.;
            const double dacNoiseL=outputL==0.?0.:tpdf(rng)*outputDither;
            const double dacNoiseR=outputR==0.?0.:tpdf(rng)*outputDither;
            dac.push(seconds-phase/clockRate,outputL+dacNoiseL,outputR+dacNoiseR);
        }
        double heldL,heldR;dac.render(seconds,controls[SbFilter].value,heldL,heldR);
        outL=bypass==1.?l:(1.-bypass)*heldL+bypass*l;
        outR=bypass==1.?r:(1.-bypass)*heldR+bypass*r;
    }
private:
    static double randomDither(uint32_t& state){state^=state<<13;state^=state>>17;state^=state<<5;return double(state&0xffffu)/65535.;}
    static double tpdf(uint32_t& state){const double first=randomDither(state);const double second=randomDither(state);return first-second;}
    void configure(bool snap) {
        clockRate=clockControl.value;chamber.clock(clockRate);body.configure(clockRate);
        const double db=plain(Output,controls[Output].value);
        const double g=std::abs(db)<1e-12?1.:std::pow(10.,db/20.);
        if(snap)outputGain.reset(g);else outputGain.aim(g,32);
    }
    double hostRate=48000,clockRate=21739,phase=1.,seconds=0.;
    SbPro2Dac dac;SbPro2Adc adc;BodyStage body;Chamber chamber;
    uint32_t rng=0x8372a4cdu,adcRng=0x591fc327u;
    int controlClock=0;Values targets=defaults();ParameterRamps controls;Ramp outputGain,clockControl;
};
}
