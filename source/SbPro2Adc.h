// Copyright (c) 2026 Phaos - phaos2000@pm.me
// Original Bulkhead portions are licensed under the Bulkhead Freeware and
// Source-Available License. See LICENSE and THIRD_PARTY_NOTICES.md in the project root.
#pragma once
#include "SbPro2Dac.h"
#include <array>

namespace bulkhead {
// Audio-path model, not an ISA device emulator. CT1345 input filter cutoffs
// are documented by Creative; their analogue response is approximated here.
class SbPro2Adc {
    struct LowPass {
        double b0=0,b1=0,b2=0,a1=0,a2=0;
        std::array<double,2> z1{},z2{};
        void prepare(double rate,double cutoff) {
            const double k=std::tan(3.14159265358979323846*std::min(cutoff,rate*.45)/rate);
            const double norm=1./(1.+std::sqrt(2.)*k+k*k);
            b0=k*k*norm;b1=2*b0;b2=b0;
            a1=2*(k*k-1)*norm;a2=(1-std::sqrt(2.)*k+k*k)*norm;reset();
        }
        void reset(){z1.fill(0);z2.fill(0);}
        double tick(int ch,double x){const double y=b0*x+z1[ch];z1[ch]=b1*x-a1*y+z2[ch];z2[ch]=b2*x-a2*y;if(x==0&&std::abs(z1[ch])+std::abs(z2[ch])<1e-24)z1[ch]=z2[ch]=0;return y;}
    };
    LowPass low,high;
public:
    void prepare(double hostRate){low.prepare(hostRate,3200.);high.prepare(hostRate,8800.);}
    void reset(){low.reset();high.reset();}
    void filter(double left,double right,double enabled,double highChoice,double& outL,double& outR) {
        const double input[2]={left,right};double output[2]{};
        for(int ch=0;ch<2;++ch){const double a=low.tick(ch,input[ch]),b=high.tick(ch,input[ch]);output[ch]=input[ch]+enabled*(a+highChoice*(b-a)-input[ch]);}
        outL=output[0];outR=output[1];
    }
    static double capture(double sample){return SbPro2Dac::decode(SbPro2Dac::encode(sample));}
};
}
