#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <stdio.h>
using namespace Gdiplus;
static int GetEnc(const wchar_t* mime, CLSID* clsid){
    UINT n=0, sz=0; GetImageEncodersSize(&n,&sz); if(!sz) return -1;
    ImageCodecInfo* p=(ImageCodecInfo*)malloc(sz); GetImageEncoders(n,sz,p);
    for(UINT i=0;i<n;i++) if(wcscmp(p[i].MimeType,mime)==0){ *clsid=p[i].Clsid; free(p); return 0; }
    free(p); return -1;
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    GdiplusStartupInput si; ULONG_PTR tok; if(GdiplusStartup(&tok,&si,nullptr)!=Ok) return 1;
    int x=GetSystemMetrics(SM_XVIRTUALSCREEN), y=GetSystemMetrics(SM_YVIRTUALSCREEN);
    int w=GetSystemMetrics(SM_CXVIRTUALSCREEN), h=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    HDC dc=GetDC(nullptr); HDC mdc=CreateCompatibleDC(dc); HBITMAP bmp=CreateCompatibleBitmap(dc,w,h);
    SelectObject(mdc,bmp); BitBlt(mdc,0,0,w,h,dc,x,y,SRCCOPY);
    Bitmap b(bmp,nullptr); CLSID clsid; int ok=0;
    if(GetEnc(L"image/png",&clsid)==0 && b.Save(L"C:\\Users\\User\\trackA\\shot.png",&clsid,nullptr)==Ok) ok=1;
    DeleteObject(bmp); DeleteDC(mdc); ReleaseDC(nullptr,dc); GdiplusShutdown(tok);
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\shot_ok.txt",L"w"); if(f){ fprintf(f,"%d %dx%d\n",ok,w,h); fclose(f); }
    return ok?0:2;
}
