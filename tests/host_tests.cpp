// Copyright (c) 2026 Phaos - phaos2000@pm.me
// Original Bulkhead portions are licensed under the Bulkhead Freeware and
// Source-Available License. See LICENSE and THIRD_PARTY_NOTICES.md in the project root.
#include "Parameters.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/common/memorystream.h"
#include "base/source/fstreamer.h"
#include <windows.h>
#include <objbase.h>
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include <iostream>
#include <cstring>
#include "Preferences.h"
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace bulkhead;
int failures=0,checks=0;
void check(bool ok,const char* what){++checks;std::cout<<(ok?"PASS ":"FAIL ")<<what<<std::endl;if(!ok)++failures;}
struct Frame : IPlugFrame {
    int resizes=0;
    tresult PLUGIN_API queryInterface(const TUID iid,void** obj) override {
        if(FUnknownPrivate::iidEqual(iid,IPlugFrame::iid)||FUnknownPrivate::iidEqual(iid,FUnknown::iid)){*obj=static_cast<IPlugFrame*>(this);return kResultOk;}*obj=nullptr;return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override{return 1;}
    uint32 PLUGIN_API release() override{return 1;}
    tresult PLUGIN_API resizeView(IPlugView* view,ViewRect* rect)override{++resizes;return view->onSize(rect);}
};
void rewind(MemoryStream& s){s.seek(0,IBStream::kIBSeekSet,nullptr);}
void click(HWND w,int x,int y,double scale){const auto pos=MAKELPARAM(int(std::lround(x*scale)),int(std::lround(y*scale)));SendMessageW(w,WM_LBUTTONDOWN,MK_LBUTTON,pos);SendMessageW(w,WM_LBUTTONUP,0,pos);}
void type(HWND w,const char* s){for(;*s;++s)SendMessageW(w,WM_CHAR,*s,0);SendMessageW(w,WM_KEYDOWN,VK_RETURN,0);}
void componentState(MemoryStream& stream,int version){
    IBStreamer s(&stream,kLittleEndian);s.writeInt32(0x46524354);s.writeInt32(version);s.writeInt32(Count);
    auto v=defaults();v[Truncate]=1.;for(double value:v)s.writeDouble(value);rewind(stream);
}
int wmain(int argc,wchar_t** argv){
    if(argc!=2)return 2;
    const auto settingsFolder=std::filesystem::temp_directory_path()/(L"Bulkhead-host-test-"+std::to_wstring(GetCurrentProcessId()));
    SetEnvironmentVariableW(L"BULKHEAD2_SETTINGS_DIR",settingsFolder.c_str());
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    auto dll=LoadLibraryW(argv[1]);check(dll!=nullptr,"Load built VST3 binary");if(!dll)return 2;
    auto init=reinterpret_cast<bool(*)()>(GetProcAddress(dll,"InitDll"));auto finish=reinterpret_cast<bool(*)()>(GetProcAddress(dll,"ExitDll"));
    auto getFactory=reinterpret_cast<IPluginFactory*(PLUGIN_API*)()>(GetProcAddress(dll,"GetPluginFactory"));
    check(init&&finish&&getFactory&&init(),"Initialize VST3 module");if(!init||!finish||!getFactory)return 2;
    auto* factory=getFactory();PFactoryInfo info{};factory->getFactoryInfo(&info);check(!std::strcmp(info.vendor,"Phaos"),"Factory company is Phaos");
    TUID controllerID{},processorID{};
    for(int i=0;i<factory->countClasses();++i){PClassInfo c{};factory->getClassInfo(i,&c);if(!std::strcmp(c.category,kVstComponentControllerClass))std::memcpy(controllerID,c.cid,16);if(!std::strcmp(c.category,kVstAudioEffectClass))std::memcpy(processorID,c.cid,16);}
    HostApplication host;IEditController* controller=nullptr;
    check(factory->createInstance(controllerID,IEditController::iid,reinterpret_cast<void**>(&controller))==kResultOk&&controller,"Create controller from factory");if(!controller)return 2;
    check(controller->initialize(&host)==kResultOk&&controller->getParameterCount()==Count,"Initialize all parameters including appended topology IDs");
    ParameterInfo pi{};controller->getParameterInfo(Truncate,pi);check(pi.id==Truncate&&pi.stepCount==1,"TRUNC is an automatable two-state parameter with a stable appended ID");
    check(controller->getParameterCount()==19&&plain(Density,controller->getParamNormalized(Density))==20.,"Bulkhead 2 exposes no dry or chamber topology parameters");
    {MemoryStream state;componentState(state,10);check(controller->setComponentState(&state)==kResultOk&&controller->getParamNormalized(Truncate)==1.,"Bulkhead 2 state restores current sound parameters");}
    {MemoryStream state;componentState(state,7);check(controller->setComponentState(&state)!=kResultOk,"Bulkhead 1 states are not silently interpreted as Bulkhead 2");}
    IComponent* processor=nullptr;factory->createInstance(processorID,IComponent::iid,reinterpret_cast<void**>(&processor));
    if(processor){processor->initialize(&host);MemoryStream old;componentState(old,10);MemoryStream current;check(processor->setState(&old)==kResultOk&&processor->getState(&current)==kResultOk,"Processor state round-trips without legacy migration");rewind(current);check(controller->setComponentState(&current)==kResultOk&&controller->getParamNormalized(Truncate)==1.,"Processor and controller agree on saved parameters");processor->terminate();processor->release();}
    HWND parent=CreateWindowExW(0,L"STATIC",L"Bulkhead test parent",WS_OVERLAPPEDWINDOW,0,0,1000,900,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Frame frame;
    for(int cycle=0;cycle<3;++cycle){
        auto* view=controller->createView(ViewType::kEditor);check(view!=nullptr,"Create/reopen editor");if(!view)continue;
        view->setFrame(&frame);IPlugViewContentScaleSupport* scale=nullptr;
        check(view->queryInterface(IPlugViewContentScaleSupport::iid,reinterpret_cast<void**>(&scale))==kResultOk&&scale,"Editor exposes host DPI interface");
        if(scale){check(scale->setContentScaleFactor(1.25f)==kResultTrue,"Accept host DPI before attachment");ViewRect size;view->getSize(&size);check(size.getWidth()==1280&&size.getHeight()==1052,"125% DPI keeps pixel edges at a whole 4x physical scale");}
        check(view->attached(parent,kPlatformTypeHWND)==kResultTrue,"Attach real native plugin editor");
        HWND window=FindWindowExW(parent,nullptr,L"Bulkhead2.Editor.200",nullptr);check(window!=nullptr,"Editor window created");
        if(window&&scale){
            scale->setContentScaleFactor(1.f);ViewRect size;view->getSize(&size);double factor=size.getWidth()/320.;
            click(window,238,58,factor);click(window,214,24+80,factor); // UI 3X
            view->getSize(&size);factor=size.getWidth()/320.;
            click(window,291,53,factor);click(window,25,24+57,factor);
            click(window,45,24+175,factor);
            check(plain(Clock,controller->getParamNormalized(Clock))==11025.,"Clock buttons select the exact 11.025 kHz rate");
            controller->setParamNormalized(Bits,.5);click(window,119,24+175,factor);
            check(plain(Bits,controller->getParamNormalized(Bits))==16.,"Bit-depth buttons select 16-bit internal arithmetic");
            const auto clockBefore=controller->getParamNormalized(Clock);
            click(window,45,24+175,factor);
            check(controller->getParamNormalized(Clock)!=clockBefore,"First clock press advances immediately");
            SendMessageW(window,WM_LBUTTONDBLCLK,MK_LBUTTON,MAKELPARAM(int(45*factor),int((24+175)*factor)));
            SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(int(45*factor),int((24+175)*factor)));
            check(controller->getParamNormalized(Clock)==clockBefore,"Second rapid clock press advances through double-click event");
            const auto bitsBefore=controller->getParamNormalized(Bits);
            SendMessageW(window,WM_LBUTTONDBLCLK,MK_LBUTTON,MAKELPARAM(int(119*factor),int((24+175)*factor)));
            SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(int(119*factor),int((24+175)*factor)));
            check(plain(Bits,controller->getParamNormalized(Bits))==8.,"Rapid bits press wraps 16 to 8 without a dead click");
            controller->setParamNormalized(Bits,bitsBefore);
            const double before=controller->getParamNormalized(Mix);click(window,286,24+190,factor);SendMessageW(window,WM_CHAR,'8',0);SendMessageW(window,WM_KEYDOWN,VK_ESCAPE,0);
            check(controller->getParamNormalized(Mix)==before,"Escape cancels typed value without changing sound");
            const double highBefore=controller->getParamNormalized(Tone);
            click(window,204,24+107+6,factor);
            click(window,208,24+103+30,factor);type(window,"75");
            check(std::abs(plain(LowDamp,controller->getParamNormalized(LowDamp))-75.)<.01,"Tone L accepts a typed cutoff in Hz");
            check(controller->getParamNormalized(Tone)==highBefore,"Editing L preserves the H cutoff");
            click(window,224,24+107+6,factor);click(window,208,24+103+30,factor);type(window,"8kHz");
            check(std::abs(plain(Tone,controller->getParamNormalized(Tone))-8000.)<.01&&std::abs(plain(LowDamp,controller->getParamNormalized(LowDamp))-75.)<.01,"L/H select editing while retaining both independent cutoffs");
            const double sb=controller->getParamNormalized(SbFilter),aa=controller->getParamNormalized(AntiAlias);
            click(window,165,53,factor);click(window,205,24+72+14,factor);click(window,205,24+106+14,factor);
            check(controller->getParamNormalized(SbFilter)!=sb&&controller->getParamNormalized(AntiAlias)!=aa,"Options independently switch ADC and DAC filters");
            click(window,205,24+72+14,factor);
            check(controller->getParamNormalized(AntiAlias)==1.&&plain(AdcFilter,controller->getParamNormalized(AdcFilter))==8800.,"ADC row selects its 8.8 kHz input filter");
            click(window,205,24+72+14,factor);
            check(controller->getParamNormalized(AntiAlias)==0.,"ADC row bypasses filtering while keeping capture active");
            const auto filterBefore=controller->getParamNormalized(SbFilter);
            click(window,205,24+106+14,factor);
            check(controller->getParamNormalized(SbFilter)!=filterBefore,"First Options press toggles immediately");
            SendMessageW(window,WM_LBUTTONDBLCLK,MK_LBUTTON,MAKELPARAM(int(205*factor),int((24+106+14)*factor)));
            SendMessageW(window,WM_LBUTTONUP,0,0);
            check(controller->getParamNormalized(SbFilter)==filterBefore,"Second rapid Options press toggles without a dead click");
            // The current single row cycles Silver, Red/amber, Orange/gold,
            // Green, Off. Advance through it to Off instead of treating the
            // first click as the separate On/Off toggle used by older editors.
            for(int step=0;step<5;++step){click(window,205,24+175,factor);if(loadPreferredCursorStyle()==0)break;}
            SendMessageW(window,WM_SETCURSOR,reinterpret_cast<WPARAM>(window),MAKELPARAM(HTCLIENT,WM_MOUSEMOVE));
            check(GetCursor()==LoadCursor(nullptr,IDC_ARROW),"GUI option disables custom cursor and uses system arrow");
            check(!loadPreferredCursor(),"Custom cursor preference persists as Off");
            MemoryStream cursorState;controller->getState(&cursorState);
            click(window,205,24+175,factor);
            SendMessageW(window,WM_SETCURSOR,reinterpret_cast<WPARAM>(window),MAKELPARAM(HTCLIENT,WM_MOUSEMOVE));
            ICONINFO cursorInfo{};BITMAP cursorBitmap{};bool cursorSized=false;
            if(GetIconInfo(GetCursor(),&cursorInfo)){if(cursorInfo.hbmColor&&GetObject(cursorInfo.hbmColor,sizeof(cursorBitmap),&cursorBitmap))cursorSized=std::max(cursorBitmap.bmWidth,cursorBitmap.bmHeight)==48; if(cursorInfo.hbmColor)DeleteObject(cursorInfo.hbmColor);if(cursorInfo.hbmMask)DeleteObject(cursorInfo.hbmMask);}
            check(cursorSized,"Custom cursor is 48 pixels along its longest edge at the 3x plugin size");
            rewind(cursorState);controller->setState(&cursorState);SendMessageW(window,WM_TIMER,1,0);
            SendMessageW(window,WM_SETCURSOR,reinterpret_cast<WPARAM>(window),MAKELPARAM(HTCLIENT,WM_MOUSEMOVE));
            check(GetCursor()==LoadCursor(nullptr,IDC_ARROW),"Controller state restores custom cursor Off");
            click(window,205,24+175,factor);
            MemoryStream originalCursor;controller->getState(&originalCursor);
            bool stylesWork=true;
            const int expectedStyles[]={2,3,4,0,5};
            for(int expected:expectedStyles){click(window,205,24+175,factor);stylesWork=stylesWork&&loadPreferredCursorStyle()==expected;SendMessageW(window,WM_SETCURSOR,reinterpret_cast<WPARAM>(window),MAKELPARAM(HTCLIENT,WM_MOUSEMOVE));stylesWork=stylesWork&&((GetCursor()!=LoadCursor(nullptr,IDC_ARROW))==(expected!=0));}
            check(stylesWork,"Cursor style row cycles four arrow colours and Off, and persists each choice");
            click(window,205,24+175,factor);MemoryStream savedStyle;controller->getState(&savedStyle);click(window,205,24+175,factor);rewind(savedStyle);
            check(controller->setState(&savedStyle)==kResultOk,"Cursor style selection round-trips in controller state");
            rewind(originalCursor);controller->setState(&originalCursor);SendMessageW(window,WM_TIMER,1,0);
            click(window,110,24+145,factor); // Empty section margin closes Options.
            click(window,165,53,factor);
            click(window,90,24+209,factor);
            check(controller->getParameterCount()==19,"Removed mode selector area does not expose hidden topology choices");
            click(window,110,24+145,factor);
            const auto ditherBefore=controller->getParamNormalized(Dither);click(window,119,24+201,factor);
            check(controller->getParamNormalized(Dither)!=ditherBefore,"Clicking unused Options space closes it without an X");
            click(window,165,53,factor);
            click(window,10,55,factor);
            click(window,45,24+175,factor);
            check(plain(Clock,controller->getParamNormalized(Clock))==22050.,"Clock buttons select the high hardware rate");
            click(window,119,24+175,factor);check(plain(Bits,controller->getParamNormalized(Bits))==8.,"Bits button wraps from 16 to 8 bits");click(window,119,24+175,factor);
            check(plain(Bits,controller->getParamNormalized(Bits))==12.,"Bit-depth buttons select 12-bit internal arithmetic");
            const double bypass=controller->getParamNormalized(Bypass);click(window,160,20,factor);check(controller->getParamNormalized(Bypass)!=bypass,"BULKHEAD logo toggles host bypass parameter");
            const double mode=controller->getParamNormalized(Truncate);click(window,45,24+201,factor);check(controller->getParamNormalized(Truncate)!=mode,"Truncate button in the shared digital panel toggles arithmetic mode");
            const double dither=controller->getParamNormalized(Dither);click(window,119,24+201,factor);check(controller->getParamNormalized(Dither)!=dither,"Dither button in the shared digital panel toggles converter dither");
            click(window,60,55,factor);click(window,40,24+67+2*11,factor);click(window,40,24+160,factor);
            check(std::abs(plain(Density,controller->getParamNormalized(Density))-24)<1e-6,"Styled preset menu loads revised factory sound");
            scale->setContentScaleFactor(2.f);view->getSize(&size);check(size.getWidth()==1920&&size.getHeight()==1578,"DPI changes resize the attached view through its host frame");
            scale->setContentScaleFactor(1.f);view->getSize(&size);factor=size.getWidth()/320.;
            click(window,238,58,factor);click(window,177,24+80,factor);view->getSize(&size);check(size.getWidth()==640&&size.getHeight()==526,"UI menu selects compact 2x size");
            factor=size.getWidth()/320.;click(window,238,58,factor);click(window,140,24+80,factor);view->getSize(&size);
            check(size.getWidth()==480&&size.getHeight()==395,"Smallest Size option provides a legible 1.5x interface");
            ViewRect constrained(0,0,480,395);view->checkSizeConstraint(&constrained);
            check(constrained.getWidth()==480&&constrained.getHeight()==395,"Host size constraints preserve the 1.5x choice");
            scale->setContentScaleFactor(2.f);view->getSize(&size);
            check(size.getWidth()==960&&size.getHeight()==789,"1.5x choice follows 200 percent host DPI");
            scale->setContentScaleFactor(1.f);view->getSize(&size);
            MemoryStream compact;check(controller->getState(&compact)==kResultOk,"Save the 1.5x size choice");
            rewind(compact);controller->setState(&compact);auto* restored=controller->createView(ViewType::kEditor);ViewRect reopened;
            if(restored){restored->getSize(&reopened);restored->release();}
            check(restored&&reopened.getWidth()==480&&reopened.getHeight()==395,"Reopened editor restores the saved 1.5x choice");
            IEditController* fresh=nullptr;factory->createInstance(controllerID,IEditController::iid,reinterpret_cast<void**>(&fresh));
            if(fresh){fresh->initialize(&host);auto* freshView=fresh->createView(ViewType::kEditor);if(freshView){freshView->getSize(&reopened);freshView->release();}fresh->terminate();fresh->release();}
            check(fresh&&reopened.getWidth()==480&&reopened.getHeight()==395,"New instance retains preferred size without host controller state");
            factor=size.getWidth()/320.;click(window,238,58,factor);click(window,177,24+80,factor);view->getSize(&size);
            check(size.getWidth()==640&&size.getHeight()==526,"Editor returns to compact 2x after 1.5x");
            MemoryStream ui;check(controller->getState(&ui)==kResultOk,"Save UI size in controller state");rewind(ui);check(controller->setState(&ui)==kResultOk,"Restore UI size state");
            // Return to 3x so repeated lifecycle cycles have the same starting conditions.
            factor=size.getWidth()/320.;click(window,238,58,factor);click(window,214,24+80,factor);
        }
        if(scale)scale->release();check(view->removed()==kResultTrue,"Detach editor and release native resources");view->setFrame(nullptr);view->release();
    }
    DestroyWindow(parent);controller->terminate();controller->release();factory->release();
    check(finish(),"Exit module cleanly after repeated editor lifetimes");FreeLibrary(dll);CoUninitialize();
    std::error_code settingsError;std::filesystem::remove(settingsFolder/L"ui-size.txt",settingsError);std::filesystem::remove(settingsFolder/L"custom-cursor.txt",settingsError);std::filesystem::remove(settingsFolder/L"cursor-style.txt",settingsError);std::filesystem::remove(settingsFolder/L"cursor-palette.txt",settingsError);std::filesystem::remove(settingsFolder,settingsError);SetEnvironmentVariableW(L"BULKHEAD2_SETTINGS_DIR",nullptr);
    std::cout<<"RESULT "<<checks-failures<<" passed / "<<failures<<" failures\n";return failures?1:0;
}
