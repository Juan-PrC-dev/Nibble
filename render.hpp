#pragma once
#include <wincodec.h>
#include <objbase.h>
#include <cstring>
#include <iomanip>
#include <sstream>
#include "animation.hpp"
#include "layout.hpp"

template<class T> struct ComPtr {T* p=nullptr;~ComPtr(){if(p)p->Release();}T** out(){return &p;}T* operator->()const{return p;}};
inline void requireHR(HRESULT h){if(FAILED(h))throw std::runtime_error("Could not load Nibble artwork. Keep the assets folder next to Nibble.exe.");}
struct Surface {
    HDC dc=nullptr;HBITMAP bitmap=nullptr;HGDIOBJ old=nullptr;unsigned* pixels=nullptr;int w=0,h=0;
    Surface()=default;Surface(const Surface&)=delete;Surface& operator=(const Surface&)=delete;~Surface(){release();}
    void release(){if(dc){SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);}dc=nullptr;bitmap=nullptr;pixels=nullptr;w=h=0;}
    void create(int width,int height){release();w=width;h=height;dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void**)&pixels,nullptr,0);if(!bitmap||!dc)throw std::runtime_error("Could not allocate Nibble display.");old=SelectObject(dc,bitmap);clear();}
    void clear(){std::memset(pixels,0,(size_t)w*h*4);}
    void load(IWICImagingFactory* factory,const fs::path& path,int width,int height){
        ComPtr<IWICBitmapDecoder> decoder;requireHR(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,decoder.out()));
        ComPtr<IWICBitmapFrameDecode> frame;requireHR(decoder->GetFrame(0,frame.out()));
        ComPtr<IWICBitmapScaler> scaler;requireHR(factory->CreateBitmapScaler(scaler.out()));requireHR(scaler->Initialize(frame.p,width,height,WICBitmapInterpolationModeFant));
        ComPtr<IWICFormatConverter> converter;requireHR(factory->CreateFormatConverter(converter.out()));requireHR(converter->Initialize(scaler.p,GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
        create(width,height);requireHR(converter->CopyPixels(nullptr,w*4,w*h*4,(BYTE*)pixels));
    }
};
struct RenderData {
    Snapshot snapshot;Mood mood=Mood::Happy;bool stale=false,quiet=false,demo=false,fetching=false;unsigned long long tick=0;
    std::array<std::wstring,2> labels,countdowns,percentages;
};
class Renderer {
    ComPtr<IWICImagingFactory> factory;
    Surface scene,atlas,output,hud;fs::path assets;int dpi=0,scaleW=0,scaleH=0,lastSprite=-1,lastBob=-100;std::wstring hudKey;std::array<int,8> anchorX{},anchorY{};
    int p(int n)const{return WidgetLayout::designPixels(n,dpi);}
    static unsigned premul(unsigned rgba){unsigned a=rgba>>24,r=(rgba>>16)&255,g=(rgba>>8)&255,b=rgba&255;return (a<<24)|((r*a/255)<<16)|((g*a/255)<<8)|(b*a/255);}
    void box(int x,int y,int w,int h,int radius,unsigned rgba){x=p(x);y=p(y);w=p(w);h=p(h);radius=p(radius);unsigned colour=premul(rgba);for(int yy=std::max(0,y);yy<std::min(hud.h,y+h);yy++)for(int xx=std::max(0,x);xx<std::min(hud.w,x+w);xx++){int dx=std::max({x+radius-xx,0,xx-(x+w-radius-1)}),dy=std::max({y+radius-yy,0,yy-(y+h-radius-1)});if(dx*dx+dy*dy<=radius*radius)hud.pixels[yy*hud.w+xx]=colour;}}
    void write(const std::wstring& text,int x,int y,int w,int h,int size,COLORREF colour,bool bold=false,bool right=false){
        // Native GDI renders the glyph coverage into a small mask; apply premultiplied alpha explicitly.
        int fontHeight=WidgetLayout::fontPixels(size,dpi);Surface mask;mask.create(p(w),std::max(p(h),fontHeight+WidgetLayout::logicalPixels(2,dpi)));HFONT font=CreateFontW(-fontHeight,0,0,0,bold?FW_SEMIBOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");auto old=SelectObject(mask.dc,font);SetTextColor(mask.dc,RGB(255,255,255));SetBkMode(mask.dc,TRANSPARENT);RECT rect{0,0,mask.w,mask.h};DrawTextW(mask.dc,text.c_str(),-1,&rect,DT_SINGLELINE|DT_NOPREFIX|(right?DT_RIGHT:DT_LEFT));GdiFlush();SelectObject(mask.dc,old);DeleteObject(font);
        int ox=p(x),oy=p(y);unsigned cr=GetRValue(colour),cg=GetGValue(colour),cb=GetBValue(colour);
        for(int yy=0;yy<mask.h;yy++)for(int xx=0;xx<mask.w;xx++){unsigned a=mask.pixels[yy*mask.w+xx]&255;if(!a||ox+xx<0||oy+yy<0||ox+xx>=hud.w||oy+yy>=hud.h)continue;unsigned& d=hud.pixels[(oy+yy)*hud.w+ox+xx];unsigned inv=255-a;d=((a+(d>>24)*inv/255)<<24)|((cr*a/255+((d>>16)&255)*inv/255)<<16)|((cg*a/255+((d>>8)&255)*inv/255)<<8)|(cb*a/255+(d&255)*inv/255);}
    }
    unsigned barColour(double value)const{return value<=10?0xffff6c8a:value<=25?0xffffb66c:value<=50?0xffffdb8b:0xff72f4c3;}
    void cacheHud(const RenderData& d){
        std::wstring key=std::wstring(d.stale?L"s":L"f")+(d.quiet?L"q":L"n")+(d.demo?L"d":L"l")+(d.fetching?L"p":L"i");
        for(int i=0;i<2;i++)key+=d.labels[i]+d.countdowns[i]+d.percentages[i];if(key==hudKey)return;hudKey=key;hud.clear();
        // Soft fixed cyan outline and glass panel, kept independent of the animated character.
        box(5,14,157,139,12,0x1546f3ed);box(7,16,153,135,10,0x4072fff5);box(9,18,149,131,9,0xc718ddd9);box(10,19,147,129,8,0xf20c2937);
        write(L"REMAINING",19,27,130,13,9,RGB(139,234,230),false);
        for(int i=0;i<2;i++){
            int y=46+i*45;auto& a=d.snapshot.limits[i];unsigned colour=a.valid?barColour(a.remaining):0xff7897a4;
            write(d.labels[i],19,y,78,19,13,RGB(233,250,248),false);
            write(d.percentages[i],98,y-1,49,21,15,RGB((colour>>16)&255,(colour>>8)&255,colour&255),false,true);
            box(19,y+22,128,5,2,0xff304c5c);if(a.valid&&a.remaining>0)box(19,y+22,std::max(2,(int)std::round(128*a.remaining/100)),5,2,colour);
            write(d.countdowns[i],19,y+29,128,12,9,RGB(159,197,209));
        }
        std::wstring footer=d.demo?L"DEMO DATA":d.stale?L"STALE / check connection":d.fetching?L"Checking usage...":d.quiet?L"Nibble / quiet":L"Nibble / connected";
        box(58,241,204,17,7,0xe00d2632);write(footer,63,243,194,13,9,d.stale?RGB(255,204,133):RGB(168,230,226),false);
    }
    void blend(Surface& source,int x,int y,int w,int h,int sx=0,int sy=0,int sw=0,int sh=0){BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};AlphaBlend(output.dc,x,y,w,h,source.dc,sx,sy,sw?sw:source.w,sh?sh:source.h,blend);}
public:
    void initialize(const fs::path& dir){assets=dir;requireHR(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_IWICImagingFactory,(void**)factory.out()));}
    void resize(int newDpi){if(dpi==newDpi)return;dpi=newDpi;scaleW=p(WidgetLayout::DesignWidth);scaleH=p(WidgetLayout::DesignHeight);scene.load(factory.p,assets/L"desk.png",p(320),p(320));atlas.load(factory.p,assets/L"character-atlas.png",p(192)*4,p(224)*2);
        // Discard the transparent extraction gutter, including any neighbouring-cell edge pixels.
        for(int row=0;row<2;row++)for(int y=row*p(224);y<row*p(224)+p(16);y++)std::memset(atlas.pixels+(size_t)y*atlas.w,0,atlas.w*4);
        // Anchor extracted cells by their opaque silhouette, so differing margins cannot shift the pet.
        for(int i=0;i<8;i++){int left=p(192),right=0,bottom=0;for(int y=0;y<p(224);y++)for(int x=0;x<p(192);x++){unsigned a=atlas.pixels[(size_t)(i/4*p(224)+y)*atlas.w+i%4*p(192)+x]>>24;if(a>32){left=std::min(left,x);right=std::max(right,x);bottom=std::max(bottom,y);}}anchorX[i]=MulDiv(p(192)/2-(left+right)/2,p(160),p(192));anchorY[i]=MulDiv(p(224)-1-bottom,p(170),p(224));}
        output.create(scaleW,scaleH);hud.create(scaleW,scaleH);hudKey.clear();lastSprite=-1;}
    void draw(HWND window,const RenderData& d,int previewDpi=0){
        resize(previewDpi?previewDpi:GetDpiForWindow(window));auto oldHudKey=hudKey;cacheHud(d);
        int index=spriteFor(d.mood,d.tick);int bob=d.mood==Mood::Sleeping?0:(int)((d.tick/8)%2);if(d.mood==Mood::Reset)bob=(int)(d.tick%8<4?4:0);
        if(index==lastSprite&&bob==lastBob&&oldHudKey==hudKey)return;lastSprite=index;lastBob=bob;output.clear();
        blend(scene,0,-p(54),p(320),p(320));
        blend(atlas,p(154)+anchorX[index],p(-17-bob)+anchorY[index],p(160),p(170),index%4*p(192),index/4*p(224),p(192),p(224));
        blend(hud,0,0,scaleW,scaleH);
        RECT r{};GetWindowRect(window,&r);POINT at{r.left,r.top},from{0,0};SIZE size{scaleW,scaleH};BLENDFUNCTION alpha{AC_SRC_OVER,0,255,AC_SRC_ALPHA};UpdateLayeredWindow(window,nullptr,&at,&size,output.dc,&from,0,&alpha,ULW_ALPHA);
    }
    // Writes a real render artifact through WIC, without capturing any desktop contents.
    void exportPng(const fs::path& file){
        ComPtr<IWICStream> stream;requireHR(factory->CreateStream(stream.out()));requireHR(stream->InitializeFromFilename(file.c_str(),GENERIC_WRITE));ComPtr<IWICBitmapEncoder> encoder;requireHR(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,encoder.out()));requireHR(encoder->Initialize(stream.p,WICBitmapEncoderNoCache));ComPtr<IWICBitmapFrameEncode> frame;requireHR(encoder->CreateNewFrame(frame.out(),nullptr));requireHR(frame->Initialize(nullptr));requireHR(frame->SetSize(output.w,output.h));WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;requireHR(frame->SetPixelFormat(&format));
        std::vector<unsigned> straight(output.pixels,output.pixels+(size_t)output.w*output.h);for(auto& c:straight){unsigned a=c>>24;if(a)c=(a<<24)|(std::min(255u,((c>>16)&255)*255/a)<<16)|(std::min(255u,((c>>8)&255)*255/a)<<8)|std::min(255u,(c&255)*255/a);}
        requireHR(frame->WritePixels(output.h,output.w*4,(UINT)straight.size()*4,(BYTE*)straight.data()));requireHR(frame->Commit());requireHR(encoder->Commit());
    }
};
