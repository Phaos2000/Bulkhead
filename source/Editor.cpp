// Copyright (c) 2026 Phaos - phaos2000@pm.me
// Original Bulkhead portions are licensed under the Bulkhead Freeware and
// Source-Available License. See LICENSE and THIRD_PARTY_NOTICES.md in the project root.
#include "Plugin.h"
#include "Draw.h"
#include "Presets.h"
#include "Cursor.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include <windowsx.h>
#include <cstring>

extern HINSTANCE ghInst;
namespace bulkhead {
using namespace Steinberg;
class Editor final : public CPluginView, public IPlugViewContentScaleSupport {
public:
    explicit Editor(Controller* c):controller(c) {
        ++instances;controller->addRef();presentation.zoom=controller->uiZoom;presentation.customCursor=controller->customCursor;presentation.cursorStyle=controller->cursorStyle;
        rect=ViewRect(0,0,int(std::lround(ui::PW*ui::zoomFactor(controller->uiZoom))),int(std::lround(ui::PH*ui::zoomFactor(controller->uiZoom))));
        Gdiplus::GdiplusStartupInput init;Gdiplus::GdiplusStartup(&gdip,&init,nullptr);
    }
    ~Editor() override {
        close();if(--instances==0)UnregisterClassW(L"Bulkhead.Editor.055",ghInst);
        destroyCursor();controller->release();if(gdip)Gdiplus::GdiplusShutdown(gdip);
    }
    DEFINE_INTERFACES
        DEF_INTERFACE(IPlugViewContentScaleSupport)
    END_DEFINE_INTERFACES(CPluginView)
    REFCOUNT_METHODS(CPluginView)
    tresult PLUGIN_API setContentScaleFactor(ScaleFactor factor) override {
        if(!std::isfinite(factor)||factor<=0)return kInvalidArgument;
        hostScale=true;dpi=std::clamp(factor,.5f,4.f);updateCursor();requestSize();return kResultTrue;
    }
    tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override {
        return type&&std::strcmp(type,kPlatformTypeHWND)==0?kResultTrue:kResultFalse;
    }
    tresult PLUGIN_API attached(void* parent,FIDString type) override {
        if(!parent||isPlatformTypeSupported(type)!=kResultTrue||window)return kResultFalse;
        if(!hostScale)dpi=windowDpi(static_cast<HWND>(parent));
        requestSize();
        WNDCLASSW wc{};wc.hInstance=ghInst;wc.lpfnWndProc=proc;wc.lpszClassName=L"Bulkhead.Editor.055";wc.style=CS_DBLCLKS;
        RegisterClassW(&wc);updateCursor();
        window=CreateWindowExW(0,wc.lpszClassName,L"BULKHEAD - Phaos",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_TABSTOP,
            0,0,rect.getWidth(),rect.getHeight(),static_cast<HWND>(parent),nullptr,ghInst,this);
        if(!window)return kResultFalse;SetTimer(window,1,40,nullptr);return CPluginView::attached(parent,type);
    }
    tresult PLUGIN_API removed() override {close();return CPluginView::removed();}
    tresult PLUGIN_API canResize() override {return kResultTrue;}
    tresult PLUGIN_API checkSizeConstraint(ViewRect* r) override {
        if(!r)return kInvalidArgument;
        const double scale=ui::physicalScale(r->getWidth()/double(ui::PW));
        r->right=r->left+int(std::lround(ui::PW*scale));r->bottom=r->top+int(std::lround(ui::PH*scale));return kResultTrue;
    }
    tresult PLUGIN_API onSize(ViewRect* r) override {
        if(!r||r->getWidth()<=0||r->getHeight()<=0)return kInvalidArgument;
        CPluginView::onSize(r);
        if(!resizing&&window)controller->chooseUiZoom(ui::zoomSlot(r->getWidth()/(ui::PW*dpi)));
        presentation.zoom=controller->uiZoom;presentation.dpi=dpi;
        updateCursor();
        if(window){SetWindowPos(window,nullptr,0,0,r->getWidth(),r->getHeight(),SWP_NOMOVE|SWP_NOZORDER);invalidate();}
        return kResultTrue;
    }
private:
    static float windowDpi(HWND h) {
        using Fn=UINT(WINAPI*)(HWND);const auto get=reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow"));
        const UINT value=get?get(h):96;return std::clamp(float(value?value:96)/96.f,.5f,4.f);
    }
    void requestSize() {
        presentation.zoom=controller->uiZoom;presentation.dpi=dpi;
        const double scale=ui::physicalScale(ui::zoomFactor(controller->uiZoom)*dpi);
        ViewRect wanted(0,0,int(std::lround(ui::PW*scale)),int(std::lround(ui::PH*scale)));
        if(wanted.getWidth()==rect.getWidth()&&wanted.getHeight()==rect.getHeight())return;
        resizing=true;
        if(plugFrame) {
            if(plugFrame->resizeView(this,&wanted)!=kResultTrue)presentation.status="HOST DECLINED WINDOW RESIZE";
        } else onSize(&wanted);
        resizing=false;
    }
    void destroyCursor(){if(GetCursor()==cursor)SetCursor(LoadCursor(nullptr,IDC_ARROW));if(cursor)DestroyCursor(cursor);cursor=nullptr;}
    void updateCursor(){destroyCursor();if(controller->customCursor)cursor=ui::makeCursor(float(rect.getWidth())/ui::PW,controller->cursorStyle);}
    void invalidate(){needsFrame=true;}
    void flushDrag(){if(drag>=0&&dragPending){controller->edit(drag,dragValue);dragPending=false;}}
    void endDrag(){if(drag>=0){flushDrag();controller->endEdit(drag);drag=-1;}}
    void close(){endDrag();if(window){KillTimer(window,1);if(GetCapture()==window)ReleaseCapture();DestroyWindow(window);window=nullptr;}}
    void beginNumber(int id) {
        if(id==Clock||id==Bits){endDrag();focus=ui::slot(id);presentation.status="CLOCK / BITS: USE BUTTONS OR ARROW KEYS";invalidate();return;}
        endDrag();presentation.editId=id;presentation.editText=display(id,controller->getParamNormalized(id),false,controller->getParamNormalized(Clock),true);
        presentation.editCaret=int(presentation.editText.size());presentation.selectAll=true;presentation.caretOn=true;presentation.status="ENTER: APPLY / ESC: CANCEL / CTRL-A: SELECT";
        caretEpoch=GetTickCount64();invalidate();
    }
    bool commitNumber() {
        if(presentation.editId<0)return true;
        double n=0;if(!parseValue(presentation.editId,presentation.editText,n,controller->getParamNormalized(Clock),true)){presentation.status="INVALID VALUE / ENTER AGAIN OR ESC";invalidate();return false;}
        const int id=presentation.editId;presentation.editId=-1;gesture(id,n);return true;
    }
    void eraseSelection() {if(presentation.selectAll){presentation.editText.clear();presentation.editCaret=0;presentation.selectAll=false;}}
    void edited(){if(presentation.presetName.empty())presentation.presetName="CURRENT";if(presentation.presetName.back()!='*')presentation.presetName+="*";presentation.status.clear();}
    void gesture(int id,double value){if(id==Clock||id==Bits)value=normalized(id,plain(id,value));controller->beginEdit(id);controller->edit(id,value);controller->endEdit(id);if(id!=Bypass)edited();invalidate();}
    Values values()const{Values v{};for(int i=0;i<Count;++i)v[i]=controller->getParamNormalized(i);return v;}
    void refreshLibrary(const std::string& select={}) {
        entries=library.scan();presentation.entries.clear();
        for(size_t i=0;i<entries.size();++i){presentation.entries.push_back(std::string(entries[i].factory?"F: ":"U: ")+entries[i].name);if(!select.empty()&&entries[i].name==select&&!entries[i].factory)presentation.selected=int(i);}
        presentation.selected=std::clamp(presentation.selected,0,int(entries.size())-1);
        presentation.scroll=std::clamp(presentation.scroll,0,std::max(0,int(entries.size())-7));
        revealSelected();
    }
    void revealSelected() {
        if(presentation.selected<presentation.scroll)presentation.scroll=presentation.selected;
        if(presentation.selected>=presentation.scroll+7)presentation.scroll=presentation.selected-6;
    }
    void loadSelected() {
        if(presentation.selected<0||presentation.selected>=int(entries.size()))return;
        auto selected=entries[presentation.selected];
        if(!selected.factory&&!PresetLibrary::read(selected.path,selected.values)){presentation.status="CANNOT READ PRESET";invalidate();return;}
        controller->startGroupEdit();
        for(int i=0;i<Count;++i)if(i!=Bypass){controller->beginEdit(i);controller->edit(i,selected.values[i]);controller->endEdit(i);}
        controller->finishGroupEdit();snapshot=values();previous=snapshot;haveSnapshot=true;
        presentation.presetName=selected.name;presentation.bank=selected.factory?0:1;presentation.status="LOADED / "+selected.name;presentation.menu=ui::Menu::None;invalidate();
    }
    int controlAt(float x,float y) const {
        const int id=ui::hitControl(x,y);
        return id==Tone&&presentation.toneLow?LowDamp:id;
    }
    void cyclePreset(int direction) {
        refreshLibrary();
        if(entries.empty())return;
        presentation.selected=(presentation.selected+direction+int(entries.size()))%int(entries.size());
        revealSelected();loadSelected();
    }
    void changeBank() {
        presentation.bank=1-presentation.bank;
        presentation.selected=presentation.bank?std::min(int(entries.size())-1,presetCount+1):0;
        presentation.scroll=presentation.selected;invalidate();
    }
    bool userSelected() const{return presentation.selected>=0&&presentation.selected<int(entries.size())&&!entries[presentation.selected].factory;}
    void saveName() {
        std::string error;const std::string name=presentation.input;
        const bool ok=presentation.menu==ui::Menu::Rename
            ?userSelected()&&library.rename(entries[presentation.selected],name,error)
            :library.save(name,values(),false,error);
        if(!ok){presentation.status=error.empty()?"SELECT A USER PRESET":error;invalidate();return;}
        const bool saving=presentation.menu==ui::Menu::SaveName;
        refreshLibrary(name);presentation.menu=ui::Menu::Presets;presentation.status=saving?"PRESET SAVED":"PRESET RENAMED";
        if(saving){presentation.presetName=name;snapshot=values();haveSnapshot=true;presentation.bank=1;}
        invalidate();
    }
    void action(int index) {
        presentation.status.clear();
        switch(index) {
        case 0:loadSelected();return;
        case 1:presentation.menu=ui::Menu::SaveName;presentation.input.clear();break;
        case 2: {
            if(!userSelected()){presentation.status="SELECT A USER PRESET TO UPDATE";break;}
            std::string error;const auto name=entries[presentation.selected].name;
            if(library.save(name,values(),true,error)){refreshLibrary(name);presentation.presetName=name;presentation.bank=1;snapshot=values();haveSnapshot=true;presentation.status="PRESET UPDATED";}
            else presentation.status=error;
            break;
        }
        case 3:case 4:
            if(!userSelected()){presentation.status="FACTORY PRESETS ARE READ ONLY";break;}
            presentation.input=entries[presentation.selected].name;presentation.menu=index==3?ui::Menu::Rename:ui::Menu::Delete;break;
        case 5:presentation.menu=ui::Menu::None;break;
        }
        invalidate();
    }
    void menuClick(float x,float y) {
        if(presentation.menu==ui::Menu::Help){presentation.menu=ui::Menu::None;invalidate();return;}
        if(presentation.menu==ui::Menu::Options) {
            const int filterIds[]={AntiAlias,SbFilter};
            const float rowY[]={72.f,106.f};
            for(int i=0;i<2;++i)if(ui::RectF(44,rowY[i],232,29).Contains(x,y)){
                if(i==0){
                    if(controller->getParamNormalized(AntiAlias)<.5){gesture(AdcFilter,0.);gesture(AntiAlias,1.);}
                    else if(controller->getParamNormalized(AdcFilter)<.5)gesture(AdcFilter,1.);
                    else gesture(AntiAlias,0.);
                }else{const int id=filterIds[i];gesture(id,controller->getParamNormalized(id)>=.5?0.:1.);}
                return;
            }
            if(ui::RectF(44,155,232,42).Contains(x,y)){
                int next=0;for(int i=0;i<ui::cursorChoiceCount;++i)if(ui::cursorChoices[i]==controller->cursorStyle)next=(i+1)%ui::cursorChoiceCount;controller->chooseCursorStyle(ui::cursorChoices[next]);presentation.customCursor=controller->customCursor;presentation.cursorStyle=controller->cursorStyle;updateCursor();
                SetCursor(cursor?cursor:LoadCursor(nullptr,IDC_ARROW));invalidate();return;
            }
            presentation.menu=ui::Menu::None;invalidate();return;
        }
        if(presentation.menu==ui::Menu::Scale) {
            for(int i=0;i<5;++i)if(ui::RectF(float(123+37*i),70,35,20).Contains(x,y)){
                controller->chooseUiZoom(i+1);presentation.menu=ui::Menu::None;requestSize();invalidate();return;
            }
            presentation.menu=ui::Menu::None;invalidate();return;
        }
        if(x<5||x>=239||y<44||y>=213||ui::RectF(222,47,14,12).Contains(x,y)){presentation.menu=ui::Menu::None;invalidate();return;}
        if(presentation.menu==ui::Menu::Presets) {
            if(ui::RectF(200,47,27,13).Contains(x,y)){changeBank();return;}
            for(int row=0;row<7;++row)if(ui::menuRow(row).Contains(x,y)&&row+presentation.scroll<int(entries.size())){presentation.selected=row+presentation.scroll;invalidate();return;}
            for(int i=0;i<6;++i)if(ui::menuButton(i).Contains(x,y)){action(i);return;}
        } else if(ui::RectF(126,155,96,20).Contains(x,y)) {presentation.menu=ui::Menu::Presets;presentation.status.clear();invalidate();}
        else if(ui::RectF(15,155,96,20).Contains(x,y)) {
            if(presentation.menu==ui::Menu::Delete) {
                std::string error;if(userSelected()&&library.erase(entries[presentation.selected],error)){refreshLibrary();presentation.menu=ui::Menu::Presets;presentation.status="PRESET DELETED";}
                else presentation.status=error;
                invalidate();
            } else saveName();
        }
    }
    void paint() {
        PAINTSTRUCT ps;HDC dc=BeginPaint(window,&ps);RECT bounds;GetClientRect(window,&bounds);
        if(bounds.right>0&&bounds.bottom>0){
            const auto now=GetTickCount64();
            if(framePixels.empty()||now-lastFrame>=40){
                presentation.sbName=controller->getParamNormalized(SbFilter)>=.5?"On":"Off";
                presentation.aaName=controller->getParamNormalized(AntiAlias)<.5?"Off":controller->getParamNormalized(AdcFilter)<.5?"3.2k":"8.8k";
                presentation.toneName=controller->getParamNormalized(ToneFilter)>=.5?"On":"Off";
                framePixels=ui::render(values(),hover,presentation);lastFrame=now;needsFrame=false;
            }
            Gdiplus::Bitmap bitmap(ui::PW,ui::PH,ui::PW*4,PixelFormat32bppARGB,reinterpret_cast<BYTE*>(framePixels.data()));
            Gdiplus::Graphics screen(dc);screen.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);screen.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            screen.DrawImage(&bitmap,Gdiplus::Rect(0,0,bounds.right,bounds.bottom),0,0,ui::PW,ui::PH,Gdiplus::UnitPixel);
        }
        EndPaint(window,&ps);
    }
    static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l) {
        Editor* self=reinterpret_cast<Editor*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        if(m==WM_NCCREATE){self=static_cast<Editor*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);self->window=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        if(!self)return DefWindowProcW(h,m,w,l);
        try{return self->message(m,w,l);}catch(...){self->presentation.status="OPERATION COULD NOT BE COMPLETED";self->invalidate();return 0;}
    }
    LRESULT message(UINT m,WPARAM w,LPARAM l) {
        RECT bounds{};GetClientRect(window,&bounds);const float sx=float(ui::PW)/std::max(1L,bounds.right),sy=float(ui::PH)/std::max(1L,bounds.bottom);
        const float x=(GET_X_LPARAM(l)+.5f)*sx,y=(GET_Y_LPARAM(l)+.5f)*sy-ui::offset;
        switch(m) {
        case WM_SETCURSOR:if(LOWORD(l)==HTCLIENT){SetCursor(cursor?cursor:LoadCursor(nullptr,IDC_ARROW));return TRUE;}break;
        case WM_DPICHANGED:case WM_DPICHANGED_AFTERPARENT:
            if(!hostScale){dpi=windowDpi(window);updateCursor();requestSize();}return 0;
        case WM_PAINT:paint();return 0;
        case WM_ERASEBKGND:return 1;
        case WM_TIMER:{
            flushDrag();
            if(presentation.customCursor!=controller->customCursor||presentation.cursorStyle!=controller->cursorStyle){presentation.customCursor=controller->customCursor;presentation.cursorStyle=controller->cursorStyle;updateCursor();invalidate();}
            if(!digitalHeld&&presentation.pressedDigital>=0&&GetTickCount64()>=digitalRelease){presentation.pressedDigital=-1;invalidate();}
            if(presentation.zoom!=controller->uiZoom)requestSize();
            if(presentation.editId>=0){const bool on=((GetTickCount64()-caretEpoch)/500)%2==0;if(on!=presentation.caretOn){presentation.caretOn=on;invalidate();}}
            const auto v=values();if(v!=previous){previous=v;
                if(haveSnapshot){bool same=true;for(int i=0;i<Count;++i)if(i!=Bypass&&std::abs(v[i]-snapshot[i])>1e-8)same=false;if(!same&&presentation.presetName.back()!='*')presentation.presetName+="*";}
                invalidate();}
            if(needsFrame)InvalidateRect(window,nullptr,FALSE);return 0;
        }
        case WM_LBUTTONDOWN:{
            SetFocus(window);
            if(presentation.menu!=ui::Menu::None){menuClick(x,y);return 0;}
            if(presentation.editId>=0&&!commitNumber())return 0;
            presentation.status.clear();
            if(ui::presetPrevRect().Contains(x,y)){cyclePreset(-1);return 0;}
            if(ui::presetNextRect().Contains(x,y)){cyclePreset(1);return 0;}
            if(ui::presetRect().Contains(x,y)){endDrag();refreshLibrary();presentation.menu=ui::Menu::Presets;invalidate();return 0;}
            if(ui::optionsRect().Contains(x,y)){presentation.menu=ui::Menu::Options;invalidate();return 0;}
            if(ui::scaleRect().Contains(x,y)){presentation.menu=ui::Menu::Scale;invalidate();return 0;}
            if(ui::helpRect().Contains(x,y)){presentation.menu=ui::Menu::Help;invalidate();return 0;}
            if(ui::clockRect().Contains(x,y)){presentation.pressedDigital=Clock;digitalHeld=true;SetCapture(window);focus=ui::slot(Clock);gesture(Clock,controller->getParamNormalized(Clock)>=.5?0.:1.);return 0;}
            if(ui::bitsRect().Contains(x,y)){presentation.pressedDigital=Bits;digitalHeld=true;SetCapture(window);focus=ui::slot(Bits);gesture(Bits,((int(std::lround(controller->getParamNormalized(Bits)*2))+1)%3)/2.);return 0;}
            if(ui::truncRect().Contains(x,y)){gesture(Truncate,controller->getParamNormalized(Truncate)>=.5?0.:1.);return 0;}
            if(ui::ditherRect().Contains(x,y)){gesture(Dither,controller->getParamNormalized(Dither)>=.5?0.:1.);return 0;}
            if(ui::toneHiRect().Contains(x,y)){presentation.toneLow=false;invalidate();return 0;}
            if(ui::toneLoRect().Contains(x,y)){presentation.toneLow=true;invalidate();return 0;}
            if(ui::bypassRect().Contains(x,y)){gesture(Bypass,controller->getParamNormalized(Bypass)>=.5?0.:1.);return 0;}
            const int id=controlAt(x,y);if(id>=0&&ui::readoutRect(id).Contains(x,y)){focus=ui::slot(id);beginNumber(id);return 0;}if(id>=0){drag=id;focus=ui::slot(id);lastY=y;dragValue=controller->getParamNormalized(id);controller->beginEdit(id);SetCapture(window);}return 0;
        }
        case WM_LBUTTONUP:digitalHeld=false;digitalRelease=GetTickCount64()+80;endDrag();if(GetCapture()==window)ReleaseCapture();return 0;
        case WM_CAPTURECHANGED:digitalHeld=false;digitalRelease=GetTickCount64()+80;endDrag();return 0;
        case WM_LBUTTONDBLCLK:{
            if(presentation.menu==ui::Menu::Options){menuClick(x,y);return 0;}
            if(presentation.menu==ui::Menu::None&&ui::digitalParamAt(x,y)>=0)return message(WM_LBUTTONDOWN,w,l);
            if(presentation.menu==ui::Menu::Presets){for(int row=0;row<7;++row)if(ui::menuRow(row).Contains(x,y)){presentation.selected=std::min(int(entries.size())-1,presentation.scroll+row);loadSelected();return 0;}return 0;}
            if(presentation.menu!=ui::Menu::None)return 0;
            const int id=controlAt(x,y);endDrag();if(GetCapture()==window)ReleaseCapture();if(id>=0){if(ui::readoutRect(id).Contains(x,y))beginNumber(id);else gesture(id,normalized(id,definitions[id].initial));}return 0;
        }
        case WM_MOUSEMOVE:{
            const int h=drag>=0?drag:presentation.menu!=ui::Menu::None?-1:ui::digitalParamAt(x,y)>=0?ui::digitalParamAt(x,y):ui::bypassRect().Contains(x,y)?Bypass:controlAt(x,y);
            if(h!=hover){hover=h;invalidate();}
            if(drag>=0){const double step=(drag==Clock?.08:drag==Bits?.04:.0105);const double delta=(lastY-y)*((GetKeyState(VK_SHIFT)&0x8000)?step*.1:step);lastY=y;dragValue=std::clamp(dragValue+delta,0.,1.);dragPending=true;edited();invalidate();}return 0;
        }
        case WM_MOUSEWHEEL:{
            if(presentation.menu==ui::Menu::Presets){presentation.scroll=std::clamp(presentation.scroll-GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA,0,std::max(0,int(entries.size())-7));invalidate();return 0;}
            if(presentation.menu!=ui::Menu::None)return 0;
            POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(window,&p);const float px=(p.x+.5f)*sx,py=(p.y+.5f)*sy-ui::offset;
            if(ui::presetRect().Contains(px,py)){cyclePreset(GET_WHEEL_DELTA_WPARAM(w)>0?-1:1);return 0;}
            const int digital=ui::digitalParamAt(px,py);const int id=digital==Clock||digital==Bits?digital:controlAt(px,py);
            if(id>=0)gesture(id,controller->getParamNormalized(id)+GET_WHEEL_DELTA_WPARAM(w)/120.*(id==Clock?1.:id==Bits?.5:(GetKeyState(VK_SHIFT)&0x8000)?.001:.01));return 0;
        }
        case WM_GETDLGCODE:return DLGC_WANTALLKEYS|DLGC_WANTCHARS;
        case WM_CHAR:
            if(presentation.editId>=0){
                if(w>=32&&w<127&&(presentation.selectAll||presentation.editText.size()<18)){eraseSelection();presentation.editText.insert(size_t(presentation.editCaret),1,char(w));++presentation.editCaret;presentation.caretOn=true;caretEpoch=GetTickCount64();invalidate();}
                return 0;
            }
            if(presentation.menu==ui::Menu::SaveName||presentation.menu==ui::Menu::Rename) {
                if(w==8){if(!presentation.input.empty())presentation.input.pop_back();}
                else {char c=char(w);if(c>='a'&&c<='z')c-=32;
                    if(w<128&&presentation.input.size()<28&&((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c==' '||c=='-'))presentation.input+=c;}
                invalidate();return 0;
            }break;
        case WM_KEYDOWN:
            if(presentation.editId>=0){
                auto& p=presentation;
                if(w==VK_ESCAPE){p.editId=-1;p.status.clear();invalidate();return 0;}
                if(w==VK_RETURN){commitNumber();return 0;}
                if(w==VK_TAB){if(commitNumber()){focus=(focus+((GetKeyState(VK_SHIFT)&0x8000)?11:1))%12;beginNumber(ui::dials[focus]);}return 0;}
                if(w=='A'&&(GetKeyState(VK_CONTROL)&0x8000)){p.selectAll=true;p.editCaret=int(p.editText.size());}
                else if(w==VK_BACK){if(p.selectAll)eraseSelection();else if(p.editCaret>0)p.editText.erase(size_t(--p.editCaret),1);}
                else if(w==VK_DELETE){if(p.selectAll)eraseSelection();else if(p.editCaret<int(p.editText.size()))p.editText.erase(size_t(p.editCaret),1);}
                else if(w==VK_LEFT){p.selectAll=false;p.editCaret=std::max(0,p.editCaret-1);}
                else if(w==VK_RIGHT){p.selectAll=false;p.editCaret=std::min(int(p.editText.size()),p.editCaret+1);}
                else if(w==VK_HOME){p.selectAll=false;p.editCaret=0;}
                else if(w==VK_END){p.selectAll=false;p.editCaret=int(p.editText.size());}
                p.caretOn=true;caretEpoch=GetTickCount64();invalidate();return 0;
            }
            if(w==VK_F1){presentation.menu=presentation.menu==ui::Menu::Help?ui::Menu::None:ui::Menu::Help;invalidate();return 0;}
            if(w==VK_ESCAPE){presentation.menu=ui::Menu::None;presentation.status.clear();endDrag();if(GetCapture()==window)ReleaseCapture();invalidate();return 0;}
            if(presentation.menu==ui::Menu::Presets) {
                if(w==VK_UP||w==VK_DOWN){presentation.selected=std::clamp(presentation.selected+(w==VK_UP?-1:1),0,int(entries.size())-1);revealSelected();invalidate();}
                if(w==VK_RETURN)loadSelected();return 0;
            }
            if(presentation.menu==ui::Menu::SaveName||presentation.menu==ui::Menu::Rename){if(w==VK_RETURN)saveName();return 0;}
            if(presentation.menu!=ui::Menu::None)return 0;
            if(w==VK_TAB){focus=(focus+((GetKeyState(VK_SHIFT)&0x8000)?11:1))%12;hover=ui::dials[focus];invalidate();return 0;}
            if(w==VK_UP||w==VK_RIGHT||w==VK_DOWN||w==VK_LEFT){const int id=ui::dials[focus]==Tone&&presentation.toneLow?LowDamp:ui::dials[focus];gesture(id,controller->getParamNormalized(id)+((w==VK_UP||w==VK_RIGHT)?1:-1)*(id==Clock?1.:id==Bits?.5:(GetKeyState(VK_SHIFT)&0x8000)?.001:.01));return 0;}
            if(w==VK_HOME){const int id=ui::dials[focus]==Tone&&presentation.toneLow?LowDamp:ui::dials[focus];gesture(id,normalized(id,definitions[id].initial));return 0;}break;
        case WM_KILLFOCUS:if(presentation.editId>=0){presentation.editId=-1;presentation.status.clear();invalidate();}endDrag();if(GetCapture()==window)ReleaseCapture();return 0;
        case WM_DESTROY:KillTimer(window,1);endDrag();return 0;
        }
        return DefWindowProcW(window,m,w,l);
    }
    inline static std::atomic<int> instances{0};
    Controller* controller;HWND window=nullptr;HCURSOR cursor=nullptr;ULONG_PTR gdip=0;
    ULONGLONG caretEpoch=0,lastFrame=0,digitalRelease=0;
    bool digitalHeld=false;
    bool needsFrame=true,dragPending=false;std::vector<unsigned> framePixels;
    float dpi=1.f,lastY=0;double dragValue=0.;bool hostScale=false,resizing=false,haveSnapshot=false;
    int hover=-1,drag=-1,focus=0;Values previous{},snapshot{};
    PresetLibrary library;std::vector<PresetEntry> entries;ui::Presentation presentation;
};
IPlugView* makeEditor(Controller* c){return new Editor(c);}
}
