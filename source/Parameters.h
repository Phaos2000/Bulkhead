// Copyright (c) 2026 Phaos - phaos2000@pm.me
// Original Bulkhead portions are licensed under the Bulkhead Freeware and
// Source-Available License. See LICENSE and THIRD_PARTY_NOTICES.md in the project root.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cctype>
#include "SbPro2Dac.h"

namespace bulkhead {
enum Param { Size, Decay, Density, Geometry, Hull, Tone, Width, Mix, Output, Clock, Bits, Bypass, Body, Truncate, LowDamp, Dither, AntiAlias, SbFilter, AdcFilter, Count };
struct Definition { const char* name; const char* unit; double min, max, initial; bool log; };
inline constexpr std::array<Definition, Count> definitions {{
    {"Size", "x", .05, 32., 2.4, true},
    {"Decay", "s", .025, 4., 1.5, true},
    {"Density", "taps", 4., 64., 20., false},
    {"Geometry", "%", 0., 100., 71., false},
    {"Scatter", "%", 0., 100., 70., false},
    {"Tone", "Hz", 350., 20000., 6500., true},
    {"Width", "%", 0., 100., 75., false},
    {"Mix", "%", 0., 100., 35., false},
    {"Output", "dB", -36., 12., 0., false},
    {"Clock", "Hz", 11025., 22050., 22050., false},
    {"Resolution", "bit", 8., 16., 12., false},
    {"Bypass", "", 0., 1., 0., false},
    {"Body", "%", 0., 100., 25., false},
    {"Truncation", "", 0., 1., 0., false},
    {"Low cutoff", "Hz", 0., 2000., 0., false},
    {"Dither", "", 0., 1., 0., false},
    {"ADC filter enable", "", 0., 1., 0., false},
    {"SB filter", "", 0., 1., 0., false},
    {"ADC filter cutoff", "Hz", 3200., 8800., 8800., false}
}};
using Values = std::array<double, Count>;
inline double clean(double n) { return std::isfinite(n) ? std::clamp(n, 0., 1.) : 0.; }
inline constexpr double clockRates[2]={11025.,22050.};
inline constexpr double bitDepths[3]={8.,12.,16.};
inline double plain(int id, double n) {
    if(id==AdcFilter)return clean(n)<.5?3200.:8800.;
    if(id==Clock)return clockRates[std::clamp(int(std::lround(clean(n))),0,1)];
    if(id==Bits)return bitDepths[std::clamp(int(std::lround(clean(n)*2)),0,2)];
    if(id==LowDamp)return 20.*(std::pow(101.,clean(n))-1.);
    const auto& d = definitions[id]; n = clean(n);
    return d.log ? d.min * std::pow(d.max / d.min, n) : d.min + n * (d.max - d.min);
}
inline double normalized(int id, double v) {
    if(id==AdcFilter)return std::isfinite(v)&&std::abs(v-3200.)<std::abs(v-8800.)?0.:1.;
    if(id==Clock){if(!std::isfinite(v))return 1.;return std::abs(v-clockRates[0])<std::abs(v-clockRates[1])?0.:1.;}
    if(id==Bits){if(!std::isfinite(v))return .5;int best=0;for(int i=1;i<3;++i)if(std::abs(v-bitDepths[i])<std::abs(v-bitDepths[best]))best=i;return best/2.;}
    if(id==LowDamp)return std::log1p(std::clamp(std::isfinite(v)?v:0.,0.,2000.)/20.)/std::log(101.);
    const auto& d = definitions[id];
    v = std::isfinite(v) ? std::clamp(v, d.min, d.max) : d.initial;
    return d.log ? std::log(v / d.min) / std::log(d.max / d.min) : (v - d.min) / (d.max - d.min);
}
inline double canonical(int id,double n) {
    n=clean(n);
    if(id==Bypass||id==Truncate||id==Dither||id==AntiAlias||id==SbFilter||id==AdcFilter)return n>=.5?1.:0.;
    if(id==Bits)return normalized(id,plain(id,n));
    if(id==Clock)return normalized(id,plain(id,n));
    return n;
}
inline double decaySecondsScale(double clockN){return 22050./SbPro2Dac::playbackRate(plain(Clock,clockN));}
inline double decaySeconds(double decayN,double clockN){return plain(Decay,decayN)*decaySecondsScale(clockN);}
inline bool parseValue(int id,std::string text,double& result,double clockN=1.,bool clocked=false) {
    if(id<0||id>=Count)return false;
    char* end=nullptr;const double parsed=std::strtod(text.c_str(),&end);
    if(end==text.c_str()||!std::isfinite(parsed))return false;
    std::string suffix;for(;*end;++end)if(!std::isspace(static_cast<unsigned char>(*end)))suffix+=char(std::toupper(static_cast<unsigned char>(*end)));
    double v=parsed;
    if(id==Clock||id==Tone||id==LowDamp||id==AdcFilter){if(suffix=="K"||suffix=="KHZ")v*=1000.;else if(!suffix.empty()&&suffix!="HZ")return false;}
    else if(id==Decay){if(suffix=="MS")v*=.001;else if(!suffix.empty()&&suffix!="S"&&suffix!="SEC")return false;}
    else if(!suffix.empty()&&suffix!="%"&&suffix!="DB"&&suffix!="X"&&suffix!="BITS"&&suffix!="BIT"&&suffix!="TAPS")return false;
    if(!std::isfinite(v))return false;if(id==Bits)v=std::round(v);
    if(id==Decay&&clocked)v/=decaySecondsScale(clockN);
    result=normalized(id,v);return true;
}
inline Values defaults() {
    Values v{}; for (int i=0; i<Count; ++i) v[i] = normalized(i, definitions[i].initial); return v;
}
inline std::string display(int id, double n, bool units = true,double clockN=1.,bool clocked=false) {
    char b[64]{}; const double p = id==Decay&&clocked?decaySeconds(n,clockN):plain(id,n);
    if (id == Truncate || id == Dither || id == AntiAlias || id == SbFilter) return n >= .5 ? "On" : "Off";
    if (id == Bypass) return n >= .5 ? "Bypassed" : "Active";
    const int digits = (id == Size || id == Decay) ? (p < 10. ? 2 : 1) : (id == Output ? 1 : 0);
    std::snprintf(b,sizeof(b),"%.*f%s%s",digits,p,units ? " " : "",units ? definitions[id].unit : "");
    return b;
}
inline constexpr int presetCount = 5;
inline constexpr const char* presetNames[presetCount] = {"STARSHIP HULL", "GLASS SHARDS", "SMALL CHAMBER", "VAST WRECK", "COLD NEEDLES"};
inline Values preset(int p) {
    constexpr double settings[presetCount][7] = {
        {2.4,1.5,20,71,70,6500,75}, {.62,.85,24,93,85,14000,90},
        {.14,.25,16,56,48,8100,60}, {16,4,24,86,74,4100,95},
        {.32,2.2,8,100,92,17500,100}
    };
    Values v = defaults(); p = std::clamp(p,0,presetCount-1);
    for (int i=0;i<7;++i) v[i]=normalized(i,settings[p][i]);
    return v;
}
}
