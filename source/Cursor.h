// Copyright (c) 2026 Phaos - phaos2000@pm.me
// Original Bulkhead portions are licensed under the Bulkhead Freeware and
// Source-Available License. See LICENSE and THIRD_PARTY_NOTICES.md in the project root.
#pragma once
#include "CursorSprites.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
namespace bulkhead::ui {
inline HCURSOR makeCursor(float uiScale,int style=defaultCursorStyle){
    if(style==0)return nullptr;
    const auto& s=arrowCursorSprite;
    const int maximum=std::max(s.width,s.height),size=int(std::lround(48.*std::clamp(double(uiScale)/3.,.5,8.)));
    const int w=std::max(1,s.width*size/maximum),h=std::max(1,s.height*size/maximum);
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr;HBITMAP color=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    HBITMAP mask=CreateBitmap(w,h,1,1,nullptr);HCURSOR result=nullptr;
    if(color&&mask){
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){const auto px=s.pixels[(y*s.height/h)*s.width+x*s.width/w];static_cast<uint32_t*>(pixels)[y*w+x]=cursorPalettePixel(style,px);}
        ICONINFO icon{};icon.fIcon=FALSE;icon.xHotspot=0;icon.yHotspot=0;icon.hbmColor=color;icon.hbmMask=mask;
        result=static_cast<HCURSOR>(CreateIconIndirect(&icon));
    }
    if(color)DeleteObject(color);if(mask)DeleteObject(mask);return result;
}
}
