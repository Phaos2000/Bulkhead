// Copyright (c) 2026 Phaos - phaos2000@pm.me
// Original Bulkhead portions are licensed under the Bulkhead Freeware and
// Source-Available License. See LICENSE and THIRD_PARTY_NOTICES.md in the project root.
#include "Parameters.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/common/memorystream.h"
#include "base/source/fstreamer.h"
#include <windows.h>
#include <objbase.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <cwchar>
#include <stdexcept>
#include <chrono>
#include <iomanip>
#include <type_traits>

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace bulkhead;
static volatile double checksum=0.;
static void require(bool success,const char* message){if(!success)throw std::runtime_error(message);}
struct Module {
    HMODULE dll=nullptr;IPluginFactory* factory=nullptr;HostApplication host;TUID cid{};
    bool (*finish)()=nullptr;
    explicit Module(const wchar_t* path){
        dll=LoadLibraryW(path);require(dll!=nullptr,"Cannot load plugin binary");
        auto init=reinterpret_cast<bool(*)()>(GetProcAddress(dll,"InitDll"));
        finish=reinterpret_cast<bool(*)()>(GetProcAddress(dll,"ExitDll"));
        auto entry=reinterpret_cast<IPluginFactory*(PLUGIN_API*)()>(GetProcAddress(dll,"GetPluginFactory"));
        require(init&&finish&&entry&&init(),"Cannot initialize plugin module");factory=entry();
        bool found=false;for(int i=0;i<factory->countClasses();++i){PClassInfo info{};factory->getClassInfo(i,&info);if(!std::strcmp(info.category,kVstAudioEffectClass)){std::memcpy(cid,info.cid,16);found=true;}}
        require(found,"Cannot find processor class");
    }
    ~Module(){if(factory)factory->release();if(finish)finish();if(dll)FreeLibrary(dll);}
};
struct Instance {
    IComponent* component=nullptr;IAudioProcessor* processor=nullptr;
    Instance(Module& module,const Values& values,double rate,int format,int channels){
        require(module.factory->createInstance(module.cid,IComponent::iid,reinterpret_cast<void**>(&component))==kResultOk&&component,"Cannot create processor");
        require(component->initialize(&module.host)==kResultOk,"Cannot initialize processor");
        require(component->queryInterface(IAudioProcessor::iid,reinterpret_cast<void**>(&processor))==kResultOk,"Cannot query audio processor");
        MemoryStream state;IBStreamer writer(&state,kLittleEndian);
        writer.writeInt32(0x46524354);writer.writeInt32(7);writer.writeInt32(Count);for(double value:values)writer.writeDouble(value);
        state.seek(0,IBStream::kIBSeekSet,nullptr);require(component->setState(&state)==kResultOk,"Cannot load parameters");
        auto layout=channels==1?SpeakerArr::kMono:SpeakerArr::kStereo;
        require(processor->setBusArrangements(&layout,1,&layout,1)==kResultOk,"Cannot set channel layout");
        component->activateBus(kAudio,kInput,0,true);component->activateBus(kAudio,kOutput,0,true);
        ProcessSetup setup{};setup.processMode=kRealtime;setup.symbolicSampleSize=format;setup.maxSamplesPerBlock=512;setup.sampleRate=rate;
        require(processor->setupProcessing(setup)==kResultOk,"Cannot configure processing");
        require(component->setActive(true)==kResultOk,"Cannot activate processor");processor->setProcessing(true);
    }
    ~Instance(){if(processor){processor->setProcessing(false);component->setActive(false);processor->release();}if(component){component->terminate();component->release();}}
};
template<class Sample> struct Buffers {
    std::vector<Sample> left=std::vector<Sample>(512),right=std::vector<Sample>(512);
    Sample* pointers[2]={left.data(),right.data()};AudioBusBuffers bus{};
    explicit Buffers(int channels){bus.numChannels=channels;if constexpr(std::is_same_v<Sample,double>)bus.channelBuffers64=pointers;else bus.channelBuffers32=pointers;}
};
static uint32_t rng=0x32af1897;
static double noise(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return double(rng)/4294967295.*2.-1.;}
template<class Sample> uint64_t compare(Module& original,Module& updated,double rate,int channels,int scenario){
    constexpr int format=std::is_same_v<Sample,double>?kSample64:kSample32;
    auto v=preset(scenario%presetCount);v[Clock]=scenario%2;v[Bits]=(scenario%3)*.5;v[Truncate]=(scenario/3)%2;
    v[Dither]=(scenario/2)%2;v[AntiAlias]=(scenario/4)%2;v[AdcFilter]=(scenario/6)%2;v[SbFilter]=(scenario/5)%2;v[Body]=(scenario%4)/3.;
    Instance a(original,v,rate,format,channels),b(updated,v,rate,format,channels);
    Buffers<Sample> input(channels),outA(channels),outB(channels);ParameterChanges changes(Count);
    const int blockSizes[]={1,17,64,127,256,511};int position=0,block=0;
    while(position<24000){const int count=std::min(blockSizes[block%6],24000-position);changes.clearQueue();
        if(scenario>=6&&block%11==0)for(int id=0;id<Count;++id){int32 queueIndex=0,pointIndex=0;auto* queue=changes.addParameterData(id,queueIndex);queue->addPoint(0,(block/11+id)%5*.25,pointIndex);queue->addPoint(count-1,(block/11+id+1)%5*.25,pointIndex);}
        for(int i=0;i<count;++i){input.left[i]=Sample(position+i<16000?noise()*.8:0.);input.right[i]=Sample(position+i<16000?noise()*.4:0.);}
        input.bus.silenceFlags=block%17==0?uint64(1)<<((channels==1)?0:1):0;
        ProcessData data{};data.processMode=kRealtime;data.symbolicSampleSize=format;data.numSamples=count;data.numInputs=data.numOutputs=1;data.inputs=&input.bus;data.inputParameterChanges=&changes;
        data.outputs=&outA.bus;require(a.processor->process(data)==kResultOk,"Original process failed");
        data.outputs=&outB.bus;require(b.processor->process(data)==kResultOk,"Updated process failed");
        for(int ch=0;ch<channels;++ch)if(std::memcmp(outA.pointers[ch],outB.pointers[ch],count*sizeof(Sample))){
            for(int i=0;i<count;++i)if(std::memcmp(&outA.pointers[ch][i],&outB.pointers[ch][i],sizeof(Sample))){std::cerr<<"Mismatch: rate="<<rate<<" channels="<<channels<<" format="<<format<<" scenario="<<scenario<<" frame="<<position+i<<" channel="<<ch<<" old="<<std::hexfloat<<outA.pointers[ch][i]<<" new="<<outB.pointers[ch][i]<<"\n";break;}
            throw std::runtime_error("Plugin outputs differ");
        }
        require(outA.bus.silenceFlags==outB.bus.silenceFlags,"Silence flags differ");position+=count;++block;
    }
    return position;
}
static double timed(Module& module,const Values& v,double rate,const std::vector<double>& input){
    Instance instance(module,v,rate,kSample64,2);Buffers<double> in(2),out(2);ProcessData data{};
    data.processMode=kRealtime;data.symbolicSampleSize=kSample64;data.numInputs=data.numOutputs=1;data.inputs=&in.bus;data.outputs=&out.bus;
    const auto start=std::chrono::steady_clock::now();double sum=0.;
    for(size_t position=0;position<input.size();position+=512){data.numSamples=int32(std::min<size_t>(512,input.size()-position));for(int i=0;i<data.numSamples;++i){in.left[i]=input[position+i];in.right[i]=input[position+i]*.7;}
        require(instance.processor->process(data)==kResultOk,"Benchmark process failed");for(int i=0;i<data.numSamples;++i)sum+=out.left[i]+out.right[i];
    }
    checksum=sum;return std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
}
static void benchmark(Module& oldModule,Module& newModule){
    DWORD_PTR processMask=0,systemMask=0;if(GetProcessAffinityMask(GetCurrentProcess(),&processMask,&systemMask)&&processMask){DWORD_PTR selected=processMask&(~processMask+1);for(int skip=0;skip<2;++skip){const auto remaining=processMask&~(selected|(selected-1));if(remaining)selected=remaining&(~remaining+1);}SetThreadAffinityMask(GetCurrentThread(),selected);}
    SetPriorityClass(GetCurrentProcess(),ABOVE_NORMAL_PRIORITY_CLASS);
    for(int scenario=0;scenario<6;++scenario){const double rate=scenario==4?96000.:scenario==5?192000.:48000.;auto v=defaults();if(scenario==1)v[Density]=1.;if(scenario==2){v[Density]=1.;v[Dither]=1.;v[Truncate]=1.;v[Body]=1.;v[AntiAlias]=1.;v[SbFilter]=1.;}if(scenario==3)v[Clock]=0.;
        std::vector<double> input(size_t(rate*5));for(auto& x:input)x=noise()*.4;std::vector<double> oldTimes,newTimes;
        for(int trial=0;trial<7;++trial){double oldTime,newTime;if(trial%2){newTime=timed(newModule,v,rate,input);oldTime=timed(oldModule,v,rate,input);}else{oldTime=timed(oldModule,v,rate,input);newTime=timed(newModule,v,rate,input);}oldTimes.push_back(oldTime);newTimes.push_back(newTime);}
        std::sort(oldTimes.begin(),oldTimes.end());std::sort(newTimes.begin(),newTimes.end());std::cout<<"BENCH "<<scenario<<" rate="<<rate<<" baseline="<<oldTimes[3]<<" updated="<<newTimes[3]<<" CPU reduction="<<(1.-newTimes[3]/oldTimes[3])*100.<<"%\n"<<std::flush;
    }
}
int wmain(int argc,wchar_t** argv){
    if(argc<3){std::cerr<<"Usage: bulkhead_compare old-binary.vst3 new-binary.vst3 [--benchmark]\n";return 2;}
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);int result=0;
    try{Module original(argv[1]),updated(argv[2]);require(std::memcmp(original.cid,updated.cid,16)==0,"Processor ID changed");
        if(argc>3&&std::wcscmp(argv[3],L"--benchmark")==0)benchmark(original,updated);
        else{uint64_t frames=0;int cases=0;for(double rate:{8000.,44100.,48000.,96000.,192000.,384000.})for(int channels:{1,2})for(int scenario=0;scenario<12;++scenario){frames+=compare<float>(original,updated,rate,channels,scenario);frames+=compare<double>(original,updated,rate,channels,scenario);cases+=2;}
            std::cout<<"PASS: "<<cases<<" compiled-plugin cases / "<<frames<<" frames matched bit-for-bit, including automation and silence flags\n";
        }
    }catch(const std::exception& error){std::cerr<<error.what()<<"\n";result=1;}
    CoUninitialize();return result;
}
