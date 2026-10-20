// Copyright (c) 2026 Phaos - phaos2000@pm.me
// Original Bulkhead portions are licensed under the Bulkhead Freeware and
// Source-Available License. See LICENSE and THIRD_PARTY_NOTICES.md in the project root.
#pragma once
#include "Parameters.h"
#include "Engine.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include <atomic>

namespace bulkhead {
inline const Steinberg::FUID processorID(0xC8432DA1,0xB5794AC2,0xA8E3E702,0x6D548F19);
inline const Steinberg::FUID controllerID(0x731B5E94,0xA8624D70,0x926CD1A8,0xF3B807E2);
class Processor final : public Steinberg::Vst::AudioEffect {
public:
    Processor();
    static Steinberg::FUnknown* create(void*) { return static_cast<Steinberg::Vst::IAudioProcessor*>(new Processor); }
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) override;
    Steinberg::tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement*,Steinberg::int32,Steinberg::Vst::SpeakerArrangement*,Steinberg::int32) override;
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup&) override;
    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool) override;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 s) override;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData&) override;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream*) override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream*) override;
    Steinberg::uint32 PLUGIN_API getLatencySamples() override { return 0; }
    Steinberg::uint32 PLUGIN_API getTailSamples() override { return Steinberg::uint32(std::ceil(processSetup.sampleRate*13.)); }
private:
    Engine engine;
    std::array<std::atomic<double>,Count> saved{}, mailbox{};
    std::atomic<unsigned> revision{0}; unsigned applied=0;
    bool prepared=false;
};
class Controller final : public Steinberg::Vst::EditController {
public:
    static Steinberg::FUnknown* create(void*) { return static_cast<Steinberg::Vst::IEditController*>(new Controller); }
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) override;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream*) override;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString) override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream*) override;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream*) override;
    void edit(int id,double value) { value=canonical(id,value);setParamNormalized(id,value);performEdit(id,value); }
    void applyPreset(int index);
    void chooseUiZoom(int zoom);
    void chooseCustomCursor(bool enabled);
    void chooseCursorStyle(int style);
    int cursorStyle=5;
    bool customCursor=true;
    int uiZoom=3;
};
Steinberg::IPlugView* makeEditor(Controller*);
}
