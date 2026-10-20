// Copyright (c) 2026 Phaos - phaos2000@pm.me
// Original Bulkhead portions are licensed under the Bulkhead Freeware and
// Source-Available License. See LICENSE and THIRD_PARTY_NOTICES.md in the project root.
#include "Draw.h"
#include <string>
#include <iostream>
int wmain(int argc,wchar_t** argv) {
    using namespace bulkhead;
    Gdiplus::GdiplusStartupInput in;ULONG_PTR token=0;
    if(Gdiplus::GdiplusStartup(&token,&in,nullptr)!=Gdiplus::Ok)return 1;
    Values values=defaults();ui::Presentation p;
    const std::wstring mode=argc>2?argv[2]:L"main";
    if(mode==L"presets"){
        p.menu=ui::Menu::Presets;p.presetName="STARSHIP HULL";
        p.entries={"F: INIT","F: STARSHIP HULL","F: GLASS SHARDS","F: SMALL CHAMBER",
            "F: VAST WRECK","F: COLD NEEDLES","U: MY HULL"};
        p.selected=1;
    }else if(mode==L"edit"){
        p.editId=Decay;p.editText="1.52s";p.editCaret=5;p.selectAll=false;p.caretOn=true;
        p.status="ENTER: APPLY / ESC: CANCEL / CTRL-A: SELECT";
    }else if(mode==L"bypass"){
        values[Bypass]=1.;p.status="BYPASS / TAIL CONTINUES INTERNALLY";
    }else if(mode==L"options"){p.menu=ui::Menu::Options;}
    else if(mode==L"help"){p.menu=ui::Menu::Help;}
    else if(mode==L"scale"){
        p.menu=ui::Menu::Scale;p.dpi=1.25f;
    }else if(mode==L"trunc"){
        values[Truncate]=1.;p.status="TRUNC / DISCARD LOW BITS IN INTERNAL MATH";
    }
    Gdiplus::Status result;
    {
        const int width=mode==L"compact"?480:ui::W,height=mode==L"compact"?395:ui::H;
        Gdiplus::Bitmap bitmap(width,height,PixelFormat32bppARGB);
        Gdiplus::Graphics g(&bitmap);
        ui::draw(g,width,height,values,-1,p);
        const CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
        result=bitmap.Save(argc>1?argv[1]:L"Bulkhead-preview.png",&png,nullptr);
    }
    Gdiplus::GdiplusShutdown(token);
    return result==Gdiplus::Ok?0:1;
}
