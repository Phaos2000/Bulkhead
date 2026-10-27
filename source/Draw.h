// Copyright (c) 2026 Phaos - phaos2000@pm.me
// Original Bulkhead portions are licensed under the Bulkhead Freeware and
// Source-Available License. See LICENSE and THIRD_PARTY_NOTICES.md in the project root.
#pragma once
#include "Parameters.h"
#include "Version.h"
#include "TitleFont.h"
#include "CursorSprites.h"
#include "PresetFloppy.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <vector>
#include <cstring>

namespace bulkhead::ui {
inline constexpr int PW=320,PH=263,W=960,H=789,offset=24;
using Gdiplus::RectF;
// Saved size slots stay compatible; the former 1x slot now means 1.5x.
inline double zoomFactor(int slot){return slot==1?1.5:double(slot);}
inline double physicalScale(double requested){return requested<1.75?1.5:std::clamp(double(std::lround(requested)),2.,20.);}
inline int zoomSlot(double requested){return requested<1.75?1:std::clamp(int(std::lround(requested)),2,5);}
struct RGB{float r,g,b;};
// The complete fixed 32-colour palette. Text and frame pixels are protected
// from error diffusion; texture pixels use Floyd-Steinberg 7/3/5/1 weights.
inline constexpr RGB palette[32]={
 {5,12,16},{12,23,30},{19,33,42},{27,44,55},{35,54,67},{44,66,80},{55,79,94},{68,92,107},
 {82,106,120},{99,123,136},{118,142,154},{139,162,173},{162,184,194},{186,207,216},{210,229,234},{235,243,238},
 {12,24,19},{219,219,219},{28,46,32},{182,182,182},{48,72,51},{146,146,146},{86,107,80},{109,109,146},
 {38,24,13},{57,35,17},{73,73,109},{73,73,73},{36,36,36},{164,115,65},{197,150,97},{227,190,137}
};
struct Pixel{RGB c;bool texture=false;};
class Raster {
public:
    int originY=0;
    std::vector<Pixel> pixels=std::vector<Pixel>(PW*PH);
    void dot(int x,int y,int color){y+=originY;if(x>=0&&x<PW&&y>=0&&y<PH)pixels[y*PW+x]={palette[color],false};}
    void rect(int x,int y,int w,int h,int color){for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i)dot(i,j,color);}
    void line(int x0,int y0,int x1,int y1,int color){int dx=std::abs(x1-x0),sx=x0<x1?1:-1,dy=-std::abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;for(;;){dot(x0,y0,color);if(x0==x1&&y0==y1)break;const int e=2*err;if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}}}
    void texture(int x,int y,int w,int h,int top,int bottom){for(int j=0;j<h;++j)for(int i=0;i<w;++i){const int px=x+i,py=y+j+originY;if(px<0||px>=PW||py<0||py>=PH)continue;const float t=float(j)/std::max(1,h-1),grain=float(((px*37+py*73)^(px*py*3))%7-3)*.32f;const auto a=palette[top],b=palette[bottom];pixels[py*PW+px]={{a.r+(b.r-a.r)*t+grain,a.g+(b.g-a.g)*t+grain,a.b+(b.b-a.b)*t+grain},true};}}
    void frame(int x,int y,int w,int h,bool pressed=false,int top=12,int dark=1){line(x,y,x+w-1,y,pressed?dark:top);line(x,y,x,y+h-1,pressed?dark:top);line(x+w-1,y,x+w-1,y+h-1,pressed?top:dark);line(x,y+h-1,x+w-1,y+h-1,pressed?top:dark);line(x+1,y+1,x+w-2,y+1,pressed?3:7);line(x+1,y+1,x+1,y+h-2,pressed?3:7);}
    static const unsigned char* glyph(char ch){
        static const unsigned char letters[][7]={
        {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{15,16,16,16,16,16,15},{30,17,17,17,17,17,30},
        {31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{15,16,16,19,17,17,15},{17,17,17,31,17,17,17},
        {14,4,4,4,4,4,14},{7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17},{17,25,25,21,19,19,17},{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},
        {14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
        {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,27,17},{17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}};
        static const unsigned char digits[][7]={{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14}};
        static const unsigned char blank[7]={},bar[7]={4,4,4,4,4,4,4},dot[7]={0,0,0,0,0,6,6},minus[7]={0,0,0,31,0,0,0},plus[7]={0,4,4,31,4,4,0},slash[7]={1,2,2,4,8,8,16},percent[7]={17,2,4,4,8,16,17},colon[7]={0,6,6,0,6,6,0},left[7]={1,2,4,8,4,2,1},right[7]={16,8,4,2,4,8,16},star[7]={0,4,21,14,21,4,0};
        static const unsigned char lower[][7]={
        {0,0,14,1,15,17,15},{16,16,30,17,17,17,30},{0,0,14,17,16,17,14},{1,1,15,17,17,17,15},
        {0,0,14,17,31,16,14},{6,9,8,28,8,8,8},{0,0,15,17,15,1,14},{16,16,30,17,17,17,17},
        {4,0,12,4,4,4,14},{2,0,6,2,2,18,12},{16,16,18,20,24,20,18},{12,4,4,4,4,4,14},
        {0,0,26,21,21,21,21},{0,0,30,17,17,17,17},{0,0,14,17,17,17,14},{0,0,30,17,30,16,16},
        {0,0,15,17,15,1,1},{0,0,22,25,16,16,16},{0,0,15,16,14,1,30},{8,8,28,8,8,9,6},
        {0,0,17,17,17,19,13},{0,0,17,17,17,10,4},{0,0,17,17,21,21,10},{0,0,17,10,4,10,17},
        {0,0,17,17,15,1,14},{0,0,31,2,4,8,31}};
        if(ch>='a'&&ch<='z')return lower[ch-'a'];if(ch>='A'&&ch<='Z')return letters[ch-'A'];if(ch>='0'&&ch<='9')return digits[ch-'0'];
        switch(ch){case '|':return bar;case '.':return dot;case '-':return minus;case '+':return plus;case '/':return slash;case '%':return percent;case ':':return colon;case '<':return left;case '>':return right;case '*':return star;default:return blank;}
    }
    void text(int x,int y,const std::string& s,int color=12,int scale=1,int maxChars=100){int count=0;for(char ch:s){if(++count>maxChars)break;const auto* rows=glyph(ch);for(int j=0;j<7;++j)for(int i=0;i<5;++i)if(rows[j]&(1<<(4-i)))rect(x+i*scale,y+j*scale,scale,scale,color);x+=6*scale;}}
    static const unsigned char* tinyGlyph(char ch) {
        static const unsigned char A[5]={2,5,7,5,5},B[5]={6,5,6,5,6},
            C[5]={3,4,4,4,3},E[5]={7,4,6,4,7},I[5]={7,2,2,2,7},
            H[5]={5,5,7,5,5},N[5]={5,7,7,7,5},O[5]={2,5,5,5,2},R[5]={6,5,6,5,5},colon[5]={0,2,0,2,0},
            P[5]={6,5,6,4,4},S[5]={3,4,2,1,6},T[5]={7,2,2,2,2},
            V[5]={5,5,5,5,2},Y[5]={5,5,2,2,2},blank[5]={};
        switch(ch){case 'A':return A;case 'B':return B;case 'C':return C;
            case 'E':return E;case 'I':return I;case 'H':return H;case 'N':return N;case 'O':return O;case 'R':return R;case ':':return colon;case 'P':return P;
            case 'S':return S;case 'T':return T;case 'V':return V;
            case 'Y':return Y;default:return blank;}
    }
    void tinyText(int x,int y,const char* words,int color){
        for(const char* p=words;*p;++p,x+=4){
            const auto* rows=tinyGlyph(*p);
            for(int j=0;j<5;++j)for(int i=0;i<3;++i)
                if(rows[j]&(1<<(2-i)))dot(x+i,y+j,color);
        }
    }
    // A hand-drawn 3x4 pixel face for secondary logo information.
    static const unsigned char* microGlyph(char ch) {
        static const unsigned char letters[][4]={
            {2,5,7,5}, {6,7,5,6}, {3,4,4,3}, {6,5,5,6},
            {7,6,4,7}, {7,6,4,4}, {3,4,5,3}, {5,7,5,5},
            {7,2,2,7}, {1,1,5,2}, {5,6,6,5}, {4,4,4,7},
            {5,7,5,5}, {5,7,7,5}, {2,5,5,2}, {6,5,6,4},
            {2,5,7,3}, {6,5,6,5}, {3,6,1,6}, {7,2,2,2},
            {5,5,5,7}, {5,5,5,2}, {5,5,7,5}, {5,2,2,5},
            {5,5,2,2}, {7,1,4,7}
        };
        static const unsigned char digits[][4]={
            {7,5,5,7}, {2,6,2,7}, {6,1,2,7}, {6,3,1,6},
            {5,7,1,1}, {7,6,1,6}, {3,4,7,7}, {7,1,2,2},
            {7,7,5,7}, {7,7,1,6}
        };
        static const unsigned char blank[4]={},dot[4]={0,0,0,2};
        if(ch>='a'&&ch<='z')ch-=32;
        if(ch>='A'&&ch<='Z')return letters[ch-'A'];
        if(ch>='0'&&ch<='9')return digits[ch-'0'];
        return ch=='.'?dot:blank;
    }
    void microText(int x,int y,const char* words,int color){
        for(const char* p=words;*p;++p,x+=4){
            const auto* rows=microGlyph(*p);
            for(int j=0;j<4;++j)for(int i=0;i<3;++i)
                if(rows[j]&(1<<(2-i)))dot(x+i,y+j,color);
        }
    }
    std::vector<unsigned> dither(){
        std::vector<RGB> error(PW*PH);std::vector<unsigned> out(PW*PH);
        auto spread=[&](int x,int y,RGB e,float factor){if(x>=0&&x<PW&&y<PH){auto& a=error[y*PW+x];a.r+=e.r*factor;a.g+=e.g*factor;a.b+=e.b*factor;}};
        for(int y=0;y<PH;++y)for(int x=0;x<PW;++x){const int at=y*PW+x;auto c=pixels[at].c;if(pixels[at].texture){c.r+=error[at].r;c.g+=error[at].g;c.b+=error[at].b;}
            int nearest=0;float best=1e30f;for(int i=0;i<32;++i){const auto p=palette[i];const float d=(c.r-p.r)*(c.r-p.r)+(c.g-p.g)*(c.g-p.g)+(c.b-p.b)*(c.b-p.b);if(d<best){best=d;nearest=i;}}
            const auto p=palette[nearest];out[at]=0xff000000u|(unsigned(p.r)<<16)|(unsigned(p.g)<<8)|unsigned(p.b);
            if(pixels[at].texture){const RGB e{c.r-p.r,c.g-p.g,c.b-p.b};spread(x+1,y,e,7.f/16);spread(x-1,y+1,e,3.f/16);spread(x,y+1,e,5.f/16);spread(x+1,y+1,e,1.f/16);}
        }return out;
    }
};
inline void cursorIcon(Raster& r,int style,int x,int y,int size){
    const CursorSprite s=style==0?CursorSprite{11,11,refinedFloppy}:arrowCursorSprite;const int maximum=std::max(s.width,s.height);
    const int w=std::max(1,s.width*size/maximum),h=std::max(1,s.height*size/maximum);
    for(int yy=0;yy<h;++yy)for(int xx=0;xx<w;++xx){
        const int sx=xx*s.width/w,sy=yy*s.height/h;auto c=s.pixels[sy*s.width+sx];
        if(style>=1&&style<=5)c=cursorPalettePixel(style,c);if(!(c>>24))continue;
        const int px=x+xx,py=y+yy+r.originY;if(px>=0&&px<PW&&py>=0&&py<PH)
            r.pixels[py*PW+px]={{float((c>>16)&255),float((c>>8)&255),float(c&255)},false};
    }
}
enum class Menu { None, Presets, SaveName, Rename, Delete, Scale, Options, Help };
struct Presentation {
    Menu menu=Menu::None;
    std::vector<std::string> entries;
    int selected=0,scroll=0,bank=0,zoom=3,editId=-1,editCaret=0,pressedDigital=-1,cursorStyle=defaultCursorStyle;
    bool caretOn=true,selectAll=false,toneLow=false,customCursor=true;std::string editText;
    float dpi=1.f;
    std::string presetName="CURRENT",input,status,sbName="Off",aaName="Off",toneName="On";
};
inline constexpr int dials[12]={Size,Decay,Density,Geometry,Hull,Width,Tone,Body,Clock,Bits,Output,Mix};
inline int slot(int id){if(id==LowDamp)id=Tone;for(int i=0;i<12;++i)if(dials[i]==id)return i;return -1;}
inline RectF controlRect(int id){const int s=slot(id);return s<0?RectF():RectF(float(5+(s%4)*78),float(46+(s/4)*57),76,54);}
inline RectF readoutRect(int id){const auto b=controlRect(id);return RectF(b.X+35,b.Y+22,37,18);}
inline int hitControl(float x,float y){for(int id:dials)if(id!=Clock&&id!=Bits&&controlRect(id).Contains(x,y))return id;return -1;}
inline RectF bypassRect(){return RectF(5,-20,310,44);}
inline RectF presetRect(){return RectF(5,28,93,13);}
inline RectF clockRect(){return RectF(11,165,68,21);}
inline RectF bitsRect(){return RectF(85,165,68,21);}
inline RectF truncRect(){return RectF(11,191,68,21);}
inline RectF ditherRect(){return RectF(85,191,68,21);}
inline int digitalParamAt(float x,float y){
    if(clockRect().Contains(x,y))return Clock;if(bitsRect().Contains(x,y))return Bits;
    if(truncRect().Contains(x,y))return Truncate;if(ditherRect().Contains(x,y))return Dither;return -1;
}
inline RectF toneHiRect(){return RectF(216,107,17,12);}
inline RectF toneLoRect(){return RectF(196,107,17,12);}
inline RectF optionsRect(){return RectF(128,28,59,13);}
inline RectF scaleRect(){return RectF(191,28,59,13);}
inline RectF helpRect(){return RectF(254,28,59,13);}
inline RectF presetPrevRect(){return RectF(99,28,13,13);}
inline RectF presetNextRect(){return RectF(114,28,13,13);}
inline RectF menuRow(int visible){return RectF(13,float(65+visible*11),212,11);}
inline RectF menuButton(int index){const int col=index%3,row=index/3;return RectF(float(13+col*74),float(153+row*24),68,17);}
inline const char* hints[Count]={
 "SIZE 0.05X-32X / Chamber SCALE", "DECAY / 0.025-4S AT HIGH CLOCK", "DENSITY / 16-24 NORMAL / 64 EXPERIMENTAL", "GEOMETRY / REFLECTION PATHS",
 "SCATTER / FRAGMENT SPACING AND IRREGULARITY", "L/H SELECT EDIT / BOTH CUT FILTERS STAY ACTIVE", "WIDTH 0-100% / WET STEREO SPREAD", "MIX / REVERB AMOUNT BEFORE OUTPUT DAC",
 "WET LEVEL -36 TO +12 DB BEFORE OUTPUT DAC", "CLOCK / 22K NORMAL / 11K ECONOMY-EXTREME", "RESOLUTION / 8  12  16 BIT PCM", "BYPASS / TAIL CONTINUES INTERNALLY", "BODY / LOW-MID WEIGHT AND SOFT LIMITING", "TRUNC / DISCARD LOW BITS IN INTERNAL MATH",
 "TONE L / BASS CUTOFF HZ / 0 IS OFF", "DITHER / TPDF AT INTERNAL STORAGE AND 8-BIT DAC", "ADC FILTER / OFF  3.2K  8.8K", "SB PRO 2 / 3.2 KHZ OUTPUT FILTER", "ADC FILTER / 3.2 OR 8.8 KHZ"
};
inline std::string readout(int id,double n,double clockN=1.){char b[32]{};double p=id==Decay?decaySeconds(n,clockN):plain(id,n);if(id==LowDamp&&p==0.)return "OFF";if(id==Density){std::snprintf(b,sizeof(b),"%.0f",p);}else if(id==Clock||id==Tone||id==LowDamp){if(p>=1000)std::snprintf(b,sizeof(b),"%.1fK",p/1000);else std::snprintf(b,sizeof(b),"%.0fHZ",p);}else if(id==Size)std::snprintf(b,sizeof(b),p<10?"%.2fX":"%.1fX",p);else if(id==Decay)std::snprintf(b,sizeof(b),p<10?"%.2fS":p<100?"%.1fS":"%.0fS",p);else if(id==Bits)std::snprintf(b,sizeof(b),"%.0f",p);else if(id==Output)std::snprintf(b,sizeof(b),"%+.1f",p);else std::snprintf(b,sizeof(b),"%.0f%%",p);return b;}
inline void button(Raster& r,int x,int y,int w,int h,const std::string& name,bool pressed=false,bool enabled=true) {
    r.texture(x,y,w,h,pressed?20:7,pressed?18:5);r.frame(x,y,w,h,pressed,12,1);
    r.text(x+(w-int(name.size())*6)/2,y+(h-7)/2,name,enabled?13:8);
}
inline void optionRow(Raster& r,int y,const char* label,const char* detail,const std::string& state,bool pressed) {
    r.texture(44,y,232,37,pressed?20:7,pressed?18:5);r.frame(44,y,232,37,pressed,12,1);
    r.text(53,y+4,label,13);r.text(53,y+19,detail,11,1,27);
    r.text(235,y+14,state,pressed?14:12);
}
inline void overlay(Raster& r,const Presentation& p,const Values& v) {
    if(p.menu==Menu::None)return;
    if(p.menu==Menu::Options) {
        r.texture(35,37,250,171,6,3);r.frame(35,37,250,171,false,13,0);
        r.text(45,44,"Options",14);r.text(184,44,"Click to close",11);
        r.text(45,59,"AUDIO I/O",30);r.line(105,62,274,62,7);
        const auto row=[&](int y,const char* label,const char* detail,const std::string& state,bool active){
            r.texture(44,y,232,29,active?20:7,active?18:5);r.frame(44,y,232,29,active,12,1);
            r.text(53,y+(*detail?4:11),label,13);if(*detail)r.text(53,y+17,detail,11);r.text(235,y+11,state,active?14:12);
        };
        row(72,"ADC input filter","8-bit capture / optional LP",p.aaName,p.aaName!="Off");
        row(106,"SB output filter","Softens highs above 3.2kHz",p.sbName,p.sbName=="On");
        r.text(45,143,"GUI",30);r.line(69,146,274,146,7);
        r.texture(44,155,232,42,p.customCursor?20:7,p.customCursor?18:5);r.frame(44,155,232,42,p.customCursor,12,1);
        r.text(53,159,"Cursor style",13);
        if(p.cursorStyle!=0){cursorIcon(r,p.cursorStyle,53,175,13);r.text(75,178,cursorStyleNames[p.cursorStyle],13);}
        else r.text(53,176,"Off",13);
        return;
    }
    if(p.menu==Menu::Scale) {
        r.rect(115,43,200,86,0);r.texture(115,43,200,86,6,3);r.frame(115,43,200,86,false,13,0);
        r.text(123,49,"Interface size",14);
        for(int i=0;i<5;++i)button(r,123+i*37,70,35,20,(i==0?"1.5X":std::to_string(i+1)+"X"),p.zoom==i+1);
        r.text(123,101,"DPI "+std::to_string(int(std::lround(p.dpi*100)))+"% / host scale",9);
        return;
    }
    if(p.menu==Menu::Help) {
        r.rect(9,43,302,174,1);r.texture(9,43,302,174,6,3);r.frame(9,43,302,174,false,13,0);
        // Keep texture around the frame, with a quiet surface behind the guide.
        r.rect(17,45,286,169,2);
        r.text(19,47,"+---------------------------------------------+",10);
        r.text(19,209,"+---------------------------------------------+",10);
        r.line(21,54,21,208,10);r.line(297,54,297,208,10);
        r.text(25,57,"BULKHEAD HELP",14);r.text(213,57,"Click to close",11);
        auto heading=[&](int y,const char* name){
            r.text(25,y,name,30);
            r.line(33+int(std::strlen(name))*6,y+3,291,y+3,7);
        };
        heading(71,"CHAMBER");
        r.text(25,81,"Size: spacing / Decay: tail length",13);
        r.text(25,91,"Density: count / Geometry: clusters",13);
        r.text(25,101,"Scatter: irregularity / Width: stereo spread",13);
        heading(116,"SOUND");
        r.text(25,126,"Tone L/H: bass/treble cut / both active",13);
        r.text(25,136,"Clock: sample rate / Bits: precision",13);
        r.text(25,146,"Body: low-mid weight / Output: wet level",13);
        r.text(25,156,"Mix: reverb amount / Truncate / Dither",13);
        heading(171,"EDITING");
        r.text(25,181,"Drag / wheel: adjust / Shift: fine",13);
        r.text(25,191,"Double-click: reset / Click readout: type",13);
        r.text(25,201,"Enter: apply / Esc: cancel / Logo: bypass",13);
        return;
    }
    r.rect(8,47,234,169,0);r.texture(5,44,234,169,6,3);r.frame(5,44,234,169,false,13,0);
    const char* title=p.menu==Menu::Presets?"PRESET LIBRARY":p.menu==Menu::SaveName?"SAVE PRESET AS":p.menu==Menu::Rename?"RENAME PRESET":"DELETE PRESET";
    r.text(13,50,title,14);r.text(226,50,"X",12);r.line(11,60,232,60,2);
    if(p.menu==Menu::Presets){r.text(155,50,p.bank?"USER":"FACTORY",11);r.text(205,50,"< >",14);}
    if(p.menu==Menu::Presets) {
        r.rect(11,63,218,82,1);r.frame(11,63,218,82,true,10,0);
        for(int row=0;row<7;++row) {
            const int i=p.scroll+row;if(i>=int(p.entries.size())||(p.bank==0&&i>presetCount)||(p.bank==1&&i<=presetCount))break;
            if(i==p.selected){r.texture(13,65+row*11,212,11,20,18);r.frame(13,65+row*11,212,11,true,22,16);}
            r.text(16,67+row*11,p.entries[i],i==p.selected?14:11,1,34);
        }
        const char* names[]={"LOAD","SAVE AS","UPDATE","RENAME","DELETE","CLOSE"};
        const bool user=p.selected>=presetCount+1&&p.selected<int(p.entries.size());
        for(int i=0;i<6;++i){const auto b=menuButton(i);button(r,int(b.X),int(b.Y),int(b.Width),int(b.Height),names[i],false,i!=2&&i!=3&&i!=4?true:user);}
        r.text(13,200,"F: FACTORY / U: USER / WHEEL: SCROLL",9,1,36);
    } else if(p.menu==Menu::Delete) {
        r.text(15,76,"DELETE THIS USER PRESET?",12);
        r.text(15,94,p.input,14,1,34);
        r.text(15,117,"THIS REMOVES THE SAVED FILE.",10);
        button(r,15,155,96,20,"DELETE");button(r,126,155,96,20,"CANCEL");
    } else {
        r.text(15,76,"NAME",11);r.rect(14,91,215,20,1);r.frame(14,91,215,20,true,11,0);
        r.text(19,97,p.input,14,1,28);r.line(19+int(p.input.size())*6,96,19+int(p.input.size())*6,104,13);
        r.text(15,121,"A-Z  0-9  SPACE  DASH",10);r.text(15,134,"ENTER: SAVE / ESC: CANCEL",10);
        button(r,15,155,96,20,"SAVE");button(r,126,155,96,20,"CANCEL");
    }
}
inline std::vector<unsigned> render(const Values& v,int hovered=-1,const Presentation& p={}) {
    Raster r;r.texture(0,0,PW,PH,7,4);r.frame(0,0,PW,PH,false,13,0);
    const bool bypass=v[Bypass]>=.5;
    r.texture(5,4,310,44,bypass?4:7,bypass?2:4);r.frame(5,4,310,44,bypass,13,0);
    const int shift=bypass?1:0;
    // Enlarge the original ANTORB1 bitmap with nearest-neighbour cells.
    // Lift its darkest shades so the full word stays readable on blue-grey.
    for(int ch=0;ch<8;++ch)for(int y=0;y<21;++y)for(int x=0;x<28;++x){
        const int source=titleGlyphs[ch][y*14/21][x*21/28];
        if(source==255)continue;
        const int ink=source==17?15:source==19?14:source==21?13:
            source==23?12:source==26?11:source==27?10:9;
        r.dot(41+ch*30+x+shift,13+y+shift,ink);
    }
    r.microText(9+shift,40+shift,bypass?"BYPASS":"ACTIVE",bypass?11:13);
    r.microText(289+shift,34+shift,versionString,10);
    r.microText(289+shift,40+shift,"PHAOS",10);
    r.originY=offset;
    cursorIcon(r,0,6,29,11);
    r.rect(20,28,78,13,2);r.frame(20,28,78,13,true,8,0);
    r.text(24,31,p.presetName,13,1,12);
    button(r,99,28,13,13,"<",false);button(r,114,28,13,13,">",false);
    button(r,128,28,59,13,"Options",p.menu==Menu::Options);
    button(r,191,28,59,13,"Size",p.menu==Menu::Scale);
    button(r,254,28,59,13,"Help",p.menu==Menu::Help);
    for(int id:dials){if(id==Clock||id==Bits)continue;const int s=slot(id),x=5+(s%4)*78,y=46+(s/4)*57;const int dialId=id==Tone&&p.toneLow?LowDamp:id;r.texture(x,y,76,54,6,4);r.frame(x,y,76,54,true,10,1);r.text(x+5,y+5,definitions[id].name,hovered==dialId?15:12);
        if(id==Tone){button(r,x+35,y+4,17,12,"L",p.toneLow);button(r,x+55,y+4,17,12,"H",!p.toneLow);}
        const int cx=x+19,cy=y+32;for(int yy=-14;yy<=14;++yy)for(int xx=-14;xx<=14;++xx){const int d=xx*xx+yy*yy;if(d<=196){int shade=d>169?((xx+yy<0)?12:1):((xx+yy<0)?6:3);r.dot(cx+xx,cy+yy,shade);}}
        const double a=(135.+270*v[dialId])*3.141592653589793/180.;r.line(cx+int(4*std::cos(a)),cy+int(4*std::sin(a)),cx+int(11*std::cos(a)),cy+int(11*std::sin(a)),14);
        r.rect(x+36,y+23,35,16,2);r.frame(x+35,y+22,37,18,true,8,0);if(p.editId==dialId){
            if(p.selectAll)r.rect(x+37,y+25,33,11,20);
            const int start=std::max(0,p.editCaret-5);
            r.text(x+38,y+27,p.editText.substr(size_t(start)),14,1,5);
            if(p.caretOn)r.line(x+38+(p.editCaret-start)*6,y+26,x+38+(p.editCaret-start)*6,y+34,14);
        }else r.text(x+38,y+27,readout(dialId,v[dialId],v[Clock]),14,1,5);
        if(id==Output)r.text(x+37,y+43,"DB",9);
    }
    r.texture(5,160,154,54,6,4);r.frame(5,160,154,54,true,10,1);
    const auto digitalButton=[&](RectF b,const char* name,const std::string& value,bool pressed){
        button(r,int(b.X),int(b.Y),int(b.Width),int(b.Height),"",pressed);
        r.text(int(b.X)+(68-int(std::string(name).size())*6)/2,int(b.Y)+3,name,13);
        r.text(int(b.X)+(68-int(value.size())*6)/2,int(b.Y)+12,value,pressed?14:12);
    };
    digitalButton(clockRect(),"Clock",v[Clock]>=.5?"22.05k":"11.025k",p.pressedDigital==Clock);
    digitalButton(bitsRect(),"Bits",std::to_string(int(bitDepths[int(std::lround(v[Bits]*2))])),p.pressedDigital==Bits);
    digitalButton(truncRect(),"Truncate",v[Truncate]>=.5?"On":"Off",v[Truncate]>=.5);
    digitalButton(ditherRect(),"Dither",v[Dither]>=.5?"On":"Off",v[Dither]>=.5);
    overlay(r,p,v);
    r.rect(5,221,310,15,25);r.frame(5,221,310,15,true,30,24);
    r.text(9,225,!p.status.empty()?p.status:hovered>=0?hints[hovered]:"SHIFT: FINE / DOUBLE-CLICK: RESET / WHEEL: NUDGE",30,1,50);

    return r.dither();
}
inline void draw(Gdiplus::Graphics& g,int width,int height,const Values& v,int hovered=-1,const Presentation& p={}) {
    auto pixels=render(v,hovered,p);Gdiplus::Bitmap bitmap(PW,PH,PW*4,PixelFormat32bppARGB,reinterpret_cast<BYTE*>(pixels.data()));
    g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    g.DrawImage(&bitmap,Gdiplus::Rect(0,0,width,height),0,0,PW,PH,Gdiplus::UnitPixel);
}
}
