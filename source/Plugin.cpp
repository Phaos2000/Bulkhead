// Copyright (c) 2026 Phaos - phaos2000@pm.me
// Original Bulkhead portions are licensed under the Bulkhead Freeware and
// Source-Available License. See LICENSE and THIRD_PARTY_NOTICES.md in the project root.
#include "Plugin.h"
#include "Version.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <cstring>
#include "Preferences.h"

using namespace Steinberg;
using namespace Steinberg::Vst;
namespace bulkhead {
namespace {
constexpr int32 stateMagic=0x46524354, stateVersion=10;
bool readState(IBStream* stream,Values& v) {
    if(!stream)return false;
    IBStreamer s(stream,kLittleEndian);int32 magic=0,version=0,count=0;
    if(!s.readInt32(magic)||magic!=stateMagic||!s.readInt32(version)||version!=stateVersion||!s.readInt32(count)||count!=Count)return false;
    for(int id=0;id<Count;++id){if(!s.readDouble(v[id])||!std::isfinite(v[id])||v[id]<0.||v[id]>1.)return false;v[id]=canonical(id,v[id]);}
    return true;
}
class Parameter final : public Vst::Parameter {
public:
    explicit Parameter(int i):Vst::Parameter(),id(i) {
        info.id=i; info.flags=ParameterInfo::kCanAutomate|(i==Bypass?ParameterInfo::kIsBypass:0);
        info.stepCount=(i==Bypass||i==Truncate||i==Dither||i==AntiAlias||i==SbFilter||i==AdcFilter||i==Clock)?1:i==Bits?2:0;info.defaultNormalizedValue=normalized(i,definitions[i].initial);
        UString(info.title,128).fromAscii(definitions[i].name);UString(info.shortTitle,128).fromAscii(definitions[i].name);
        UString(info.units,128).fromAscii(definitions[i].unit);valueNormalized=info.defaultNormalizedValue;
    }
    ParamValue toPlain(ParamValue v) const override {return plain(id,v);}
    ParamValue toNormalized(ParamValue v) const override {return normalized(id,v);}
    void toString(ParamValue v,String128 s) const override {UString(s,128).fromAscii(display(id,v,false).c_str());}
    bool fromString(const TChar* s,ParamValue& v) const override {
        char ascii[128]{};UString(const_cast<TChar*>(s),128).toAscii(ascii,128);
        if(id==Bypass){if(!std::strcmp(ascii,"Active")){v=0;return true;}if(!std::strcmp(ascii,"Bypassed")){v=1;return true;}}
        if(id==Truncate||id==Dither||id==AntiAlias||id==SbFilter){if(!std::strcmp(ascii,"On")){v=1;return true;}if(!std::strcmp(ascii,"Off")){v=0;return true;}}
        return parseValue(id,ascii,v);
    }
private:int id;
};
}
Processor::Processor() {
    setControllerClass(controllerID);const auto d=defaults();
    for(int i=0;i<Count;++i){saved[i].store(d[i]);mailbox[i].store(d[i]);}
}
tresult PLUGIN_API Processor::initialize(FUnknown* c) {
    auto result=AudioEffect::initialize(c);if(result!=kResultOk)return result;
    addAudioInput(STR16("Input"),SpeakerArr::kStereo);addAudioOutput(STR16("Output"),SpeakerArr::kStereo);return kResultOk;
}
tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement* in,int32 ni,SpeakerArrangement* out,int32 no) {
    if(ni!=1||no!=1||!in||!out||in[0]!=out[0]||(in[0]!=SpeakerArr::kStereo&&in[0]!=SpeakerArr::kMono))return kResultFalse;
    return AudioEffect::setBusArrangements(in,ni,out,no);
}
tresult PLUGIN_API Processor::canProcessSampleSize(int32 s){return s==kSample32||s==kSample64?kResultTrue:kResultFalse;}
tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& s) {
    if(!std::isfinite(s.sampleRate)||s.sampleRate<8000||s.sampleRate>384000)return kInvalidArgument;
    Values v{};for(int i=0;i<Count;++i)v[i]=saved[i].load();
    engine.prepare(s.sampleRate,v);prepared=true;applied=revision.load();return AudioEffect::setupProcessing(s);
}
tresult PLUGIN_API Processor::setActive(TBool state){if(state&&prepared)engine.reset();return AudioEffect::setActive(state);}
tresult PLUGIN_API Processor::setState(IBStream* s) {
    Values v{};if(!readState(s,v))return kResultFalse;
    revision.fetch_add(1,std::memory_order_acq_rel);
    for(int i=0;i<Count;++i){mailbox[i].store(v[i],std::memory_order_relaxed);saved[i].store(v[i],std::memory_order_relaxed);}
    revision.fetch_add(1,std::memory_order_release);return kResultOk;
}
tresult PLUGIN_API Processor::getState(IBStream* stream) {
    if(!stream)return kInvalidArgument;IBStreamer s(stream,kLittleEndian);
    if(!s.writeInt32(stateMagic)||!s.writeInt32(stateVersion)||!s.writeInt32(Count))return kResultFalse;
    for(auto& a:saved)if(!s.writeDouble(a.load(std::memory_order_relaxed)))return kResultFalse;
    return kResultOk;
}
tresult PLUGIN_API Processor::process(ProcessData& data) {
    NoDenormals guard;
    const unsigned r=revision.load(std::memory_order_acquire);
    if(r!=applied && !(r&1)) {
        Values v{};for(int i=0;i<Count;++i)v[i]=mailbox[i].load(std::memory_order_relaxed);
        if(revision.load(std::memory_order_acquire)==r){engine.set(v);applied=r;}
    }
    struct Event { IParamValueQueue* q=nullptr;int32 index=0,offset=0;double value=0;bool ready=false; };
    std::array<Event,Count> events{};
    auto advance=[&](Event& e) {
        e.ready=false;
        while(e.q && e.index<e.q->getPointCount()) {
            if(e.q->getPoint(e.index++,e.offset,e.value)==kResultTrue&&std::isfinite(e.value)) {e.offset=std::max<int32>(0,e.offset);e.ready=true;break;}
        }
    };
    if(data.inputParameterChanges)for(int32 q=0;q<data.inputParameterChanges->getParameterCount();++q) {
        auto* queue=data.inputParameterChanges->getParameterData(q);if(!queue||queue->getParameterId()>=Count)continue;
        auto& e=events[queue->getParameterId()];e.q=queue;advance(e);
    }
    auto apply=[&](int32 offset) {
        for(int id=0;id<Count;++id)while(events[id].ready&&events[id].offset<=offset) {
            const double v=canonical(id,events[id].value);
            engine.set(id,v);saved[id].store(v,std::memory_order_relaxed);advance(events[id]);
        }
    };
    if(data.numSamples<=0 || data.numOutputs<1 || !data.outputs || !prepared) {
        apply(INT32_MAX);return kResultOk;
    }
    if(canProcessSampleSize(data.symbolicSampleSize)!=kResultTrue)return kInvalidArgument;
    auto& out=data.outputs[0];if(out.numChannels<1)return kResultOk;
    const bool is64=data.symbolicSampleSize==kSample64;
    auto* in=(data.numInputs>0&&data.inputs)?&data.inputs[0]:nullptr;
    int32 cursor=0;
    bool silent=true;
    while(cursor<data.numSamples) {
        apply(cursor);int32 end=data.numSamples;
        for(const auto& e:events)if(e.ready)end=std::min(end,e.offset);
        for(;cursor<end;++cursor) {
            double l=0,rIn=0;
            if(in&&in->numChannels>0) {
                if(is64) {
                    if(in->channelBuffers64&&in->channelBuffers64[0]&&!(in->silenceFlags&1))l=in->channelBuffers64[0][cursor];
                    rIn=l;if(in->numChannels>1)rIn=(in->channelBuffers64&&in->channelBuffers64[1]&&!(in->silenceFlags&2))?in->channelBuffers64[1][cursor]:0.;
                } else {
                    if(in->channelBuffers32&&in->channelBuffers32[0]&&!(in->silenceFlags&1))l=in->channelBuffers32[0][cursor];
                    rIn=l;if(in->numChannels>1)rIn=(in->channelBuffers32&&in->channelBuffers32[1]&&!(in->silenceFlags&2))?in->channelBuffers32[1][cursor]:0.;
                }
            }
            double a,b;engine.tick(l,rIn,a,b);if(out.numChannels==1)a=(a+b)*.5;
            silent=silent&&std::abs(a)<1.e-12&&std::abs(b)<1.e-12;
            if(is64) {if(out.channelBuffers64){if(out.channelBuffers64[0])out.channelBuffers64[0][cursor]=a;if(out.numChannels>1&&out.channelBuffers64[1])out.channelBuffers64[1][cursor]=b;}}
            else {if(out.channelBuffers32){if(out.channelBuffers32[0])out.channelBuffers32[0][cursor]=float(a);if(out.numChannels>1&&out.channelBuffers32[1])out.channelBuffers32[1][cursor]=float(b);}}
        }
    }
    apply(data.numSamples);out.silenceFlags=silent?((uint64(1)<<out.numChannels)-1):0;
    return kResultOk;
}
tresult PLUGIN_API Controller::initialize(FUnknown* c) {
    uiZoom=loadPreferredZoom();cursorStyle=loadPreferredCursorStyle();customCursor=cursorStyle!=0;
    const auto r=EditController::initialize(c);if(r!=kResultOk)return r;
    for(int i=0;i<Count;++i)parameters.addParameter(new bulkhead::Parameter(i));return kResultOk;
}
tresult PLUGIN_API Controller::setComponentState(IBStream* s) {
    Values v{};if(!readState(s,v))return kResultFalse;
    for(int i=0;i<Count;++i)setParamNormalized(i,v[i]);return kResultOk;
}
tresult PLUGIN_API Controller::getState(IBStream* stream) {
    if(!stream)return kInvalidArgument;IBStreamer s(stream,kLittleEndian);
    return s.writeInt32(0x42485549)&&s.writeInt32(4)&&s.writeInt32(uiZoom)&&s.writeInt32(customCursor?1:0)&&s.writeInt32(cursorStyle)?kResultOk:kResultFalse;
}
tresult PLUGIN_API Controller::setState(IBStream* stream) {
    if(!stream)return kInvalidArgument;IBStreamer s(stream,kLittleEndian);int32 magic=0,version=0,zoom=0;
    if(!s.readInt32(magic)||magic!=0x42485549||!s.readInt32(version)||(version<1||version>4)||!s.readInt32(zoom)||zoom<1||zoom>5)return kResultFalse;
    int32 cursorValue=1;if(version>=2&&(!s.readInt32(cursorValue)||(cursorValue!=0&&cursorValue!=1)))return kResultFalse;
    int32 style=5;if(version>=3&&(!s.readInt32(style)||style<0||style>5))return kResultFalse;
    if(version==3){style=cursorValue?(style==2?2:5):0;}
    else if(version<3)style=cursorValue?5:0;
    if(style==1)style=5;
    uiZoom=zoom;cursorStyle=style;customCursor=style!=0;return kResultOk;
}
void Controller::chooseCursorStyle(int style){cursorStyle=style==1?5:std::clamp(style,0,5);customCursor=cursorStyle!=0;savePreferredCursorStyle(cursorStyle);savePreferredCursor(customCursor);}
void Controller::chooseCustomCursor(bool enabled){customCursor=enabled;cursorStyle=enabled?5:0;savePreferredCursor(enabled);savePreferredCursorStyle(cursorStyle);}
void Controller::chooseUiZoom(int zoom){uiZoom=std::clamp(zoom,1,5);savePreferredZoom(uiZoom);}
IPlugView* PLUGIN_API Controller::createView(FIDString name) {return name&&std::strcmp(name,ViewType::kEditor)==0?makeEditor(this):nullptr;}
void Controller::applyPreset(int index) {
    const auto v=preset(index);startGroupEdit();
    // Preserve mix, output level and bypass when auditioning spaces.
    for(int i=0;i<=Width;++i){beginEdit(i);edit(i,v[i]);endEdit(i);}finishGroupEdit();
}
}
BEGIN_FACTORY_DEF("Phaos", "", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(bulkhead::processorID),PClassInfo::kManyInstances,kVstAudioEffectClass,"Bulkhead",Vst::kDistributable,"Fx|Reverb",bulkhead::versionString,kVstVersionString,bulkhead::Processor::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(bulkhead::controllerID),PClassInfo::kManyInstances,kVstComponentControllerClass,"Bulkhead Controller",0,"",bulkhead::versionString,kVstVersionString,bulkhead::Controller::create)
END_FACTORY
