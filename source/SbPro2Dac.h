// Copyright (c) 2026 Phaos - phaos2000@pm.me
// Original Bulkhead portions are licensed under the Bulkhead Freeware and
// Source-Available License. See LICENSE and THIRD_PARTY_NOTICES.md in the project root.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace bulkhead {
// Exact half-away-from-zero rounding for finite PCM codes in [-32768,32768].
// Testing the fractional part avoids both library calls and the double-rounding
// error of adding 0.5 to a value immediately below a half-code boundary.
inline int roundedPcmCode(double code) {
    const int whole=int(code);
    const double fraction=code-whole;
    return whole+int(fraction>=.5)-int(fraction<=-.5);
}
// Original audio-path model, informed by DOSBox Staging's documented card
// behaviour. No emulator source is incorporated. ISA/DMA/IRQ are not modelled.
class SbPro2Dac {
public:
    static constexpr double nativeRate=45454.;
    static double playbackRate(double nominal) {
        // Nearest integer DSP time-constant divisor, two interleaved channels.
        const int divisor=std::clamp(int(std::lround(1000000./(2.*nominal))),1,255);
        return double((1000000/divisor)/2);
    }
    static uint8_t encode(double sample) {
        if(!std::isfinite(sample))return 128;
        return uint8_t(std::clamp(roundedPcmCode(std::clamp(sample,-1.,1.)*128.)+128,0,255));
    }
    static double decode(uint8_t sample){return (int(sample)-128)/128.;}
    void prepare(double hostRate) {
        // Bilinear 2-pole Butterworth approximation of the analogue 3.2 kHz
        // reconstruction filter. It runs AFTER the hold, at the host rate.
        const double k=std::tan(3.14159265358979323846*std::min(3200.,hostRate*.45)/hostRate);
        const double norm=1./(1.+std::sqrt(2.)*k+k*k);
        b0=k*k*norm;b1=2.*b0;b2=b0;
        a1=2.*(k*k-1.)*norm;a2=(1.-std::sqrt(2.)*k+k*k)*norm;
        reset();
    }
    void reset(){nextEdge=0;for(int ch=0;ch<2;++ch)pending[ch]=held[ch]=z1[ch]=z2[ch]=0.;}
    void push(double seconds,double left,double right) {
        advance(seconds,false);
        pending[0]=decode(encode(left));pending[1]=decode(encode(right));
    }
    void render(double seconds,double filterMix,double& left,double& right) {
        advance(seconds,true);
        double result[2]{};
        for(int ch=0;ch<2;++ch){
            const double y=b0*held[ch]+z1[ch];
            z1[ch]=b1*held[ch]-a1*y+z2[ch];z2[ch]=b2*held[ch]-a2*y;
            if(held[ch]==0.&&std::abs(z1[ch])+std::abs(z2[ch])<1e-24)z1[ch]=z2[ch]=0.;
            result[ch]=held[ch]+filterMix*(y-held[ch]);
        }
        left=result[0];right=result[1];
    }
private:
    void advance(double seconds,bool inclusive) {
        // Event timestamps preserve card-clock edges even above/below host rate.
        const double until=seconds*nativeRate;
        while(inclusive?double(nextEdge)<=until+1e-10:double(nextEdge)<until-1e-10){
            held[0]=pending[0];held[1]=pending[1];++nextEdge;
        }
    }
    uint64_t nextEdge=0;
    double pending[2]{},held[2]{},z1[2]{},z2[2]{};
    double b0=0,b1=0,b2=0,a1=0,a2=0;
};
}
