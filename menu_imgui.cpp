#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdio.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "shared.h"

static void MLog(const char* s){ FILE* f=nullptr; fopen_s(&f,"menu.log","a"); if(f){ fprintf(f,"%s\n",s); fclose(f);} }

static ID3D11Device* g_dev = nullptr;
static ID3D11DeviceContext* g_ctx = nullptr;
static ID3D11Texture2D* g_tex = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;
static ID3D11Texture2D* g_stage = nullptr;
static HDC g_memDC = nullptr;
static HBITMAP g_bmp = nullptr;
static void* g_bits = nullptr;
static int g_w = 470, g_h = 660;
static bool g_visible = true;
static bool g_insPrev = false;
static HWND g_hwnd = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static bool MakeD3D() {
    D3D_FEATURE_LEVEL fl;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &g_dev, &fl, &g_ctx))) return false;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = g_w; td.Height = g_h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (FAILED(g_dev->CreateTexture2D(&td, nullptr, &g_tex))) return false;
    if (FAILED(g_dev->CreateRenderTargetView(g_tex, nullptr, &g_rtv))) return false;
    td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(g_dev->CreateTexture2D(&td, nullptr, &g_stage))) return false;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = g_w; bi.bmiHeader.biHeight = -g_h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    HDC screen = GetDC(nullptr);
    g_memDC = CreateCompatibleDC(screen);
    g_bmp = CreateDIBSection(g_memDC, &bi, DIB_RGB_COLORS, &g_bits, nullptr, 0);
    if (g_bmp) SelectObject(g_memDC, g_bmp);
    ReleaseDC(nullptr, screen);
    return g_bmp && g_bits;
}

static void PresentLayered() {
    if (!g_bits || !g_memDC || !g_ctx) return;
    g_ctx->CopyResource(g_stage, g_tex);
    D3D11_MAPPED_SUBRESOURCE m = {};
    if (FAILED(g_ctx->Map(g_stage, 0, D3D11_MAP_READ, 0, &m))) return;
    unsigned char* dst = (unsigned char*)g_bits;
    for (int y = 0; y < g_h; ++y)
        memcpy(dst + (size_t)y * g_w * 4, (unsigned char*)m.pData + (size_t)y * m.RowPitch, (size_t)g_w * 4);
    g_ctx->Unmap(g_stage, 0);
    for (int i = 0; i < g_w * g_h; ++i) {
        unsigned char* p = dst + (size_t)i * 4;
        unsigned int a = p[3];
        p[0] = (unsigned char)((p[0] * a) / 255);
        p[1] = (unsigned char)((p[1] * a) / 255);
        p[2] = (unsigned char)((p[2] * a) / 255);
    }
    HDC screen = GetDC(nullptr);
    POINT pt = { 60, 80 };
    SIZE sz = { g_w, g_h };
    POINT src = { 0, 0 };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(g_hwnd, screen, &pt, &sz, g_memDC, &src, 0, &bf, ULW_ALPHA);
    ReleaseDC(nullptr, screen);
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_CLOSE:
        g_visible = false; ShowWindow(hWnd, SW_HIDE); return 0;
    case WM_DESTROY:
        PostQuitMessage(0); return 0;
    case WM_LBUTTONDOWN:
        SetForegroundWindow(hWnd); break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    MLog("start");
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"DFImguiMenu";
    RegisterClassExW(&wc);
    g_hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, wc.lpszClassName, L"DF MOD MENU",
        WS_POPUP, 60, 80, g_w, g_h, nullptr, nullptr, hInst, nullptr);
    if (!g_hwnd) { MLog("CreateWindow failed"); return 1; }
    MLog("window created");
    ShowWindow(g_hwnd, SW_SHOW);
    SetWindowPos(g_hwnd, HWND_TOPMOST, 60, 80, g_w, g_h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (!MakeD3D()) { MLog("d3d failed"); return 2; }
    MLog("d3d ok");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 6.f; st.FrameRounding = 4.f; st.GrabRounding = 4.f;
    st.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.06f, 0.08f, 0.97f);
    st.Colors[ImGuiCol_Header] = ImVec4(0.15f, 0.35f, 0.55f, 1.f);
    st.Colors[ImGuiCol_Button] = ImVec4(0.15f, 0.35f, 0.55f, 1.f);
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);
    MLog("imgui ok");

    HANDLE map = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(MenuCfg), MENU_SHM);
    MenuCfg* cfg = map ? (MenuCfg*)MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MenuCfg)) : nullptr;
    if (cfg) { cfg->magic = MENU_MAGIC; cfg->version = MENU_VERSION; }

    bool esp = true, boxes = true, names = true, dist = true, team = true, body = true, aim = false;
    float aimFov = 160.f, aimStep = 0.35f, maxDist = 400.f, camZ = 0.f, fovAdj = 0.f, fovAds = 67.f;

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        bool ins = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (ins && !g_insPrev) {
            g_visible = !g_visible;
            ShowWindow(g_hwnd, g_visible ? SW_SHOW : SW_HIDE);
            if (g_visible) { SetWindowPos(g_hwnd, HWND_TOPMOST, 60, 80, g_w, g_h, SWP_NOACTIVATE | SWP_SHOWWINDOW); SetForegroundWindow(g_hwnd); }
        }
        g_insPrev = ins;

        if (cfg) {
            cfg->magic = MENU_MAGIC; cfg->version = MENU_VERSION;
            cfg->esp = esp ? 1 : 0; cfg->boxes = boxes ? 1 : 0; cfg->names = names ? 1 : 0;
            cfg->dist = dist ? 1 : 0; cfg->team = team ? 1 : 0; cfg->body = body ? 1 : 0; cfg->aim = aim ? 1 : 0;
            cfg->aimFov = aimFov; cfg->aimStep = aimStep; cfg->maxDist = maxDist;
            cfg->camZ = camZ; cfg->fovAdj = fovAdj; cfg->fovAds = fovAds;
            cfg->visible = g_visible ? 1 : 0;
        }
        if (!g_visible) { Sleep(25); continue; }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(8, 8));
        ImGui::SetNextWindowSize(ImVec2((float)g_w - 16.f, (float)g_h - 16.f));
        ImGui::Begin("DF MOD MENU", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.f, 1.f), "DF MOD MENU  v2");
        ImGui::TextDisabled("INSERT = hide/show  |  click to focus");
        ImGui::SeparatorText("ESP");
        ImGui::Checkbox("ESP", &esp);
        ImGui::Checkbox("Box", &boxes);
        ImGui::Checkbox("Name", &names);
        ImGui::Checkbox("Distance", &dist);
        ImGui::Checkbox("Team filter (enemy only)", &team);
        ImGui::Checkbox("Body chams", &body);
        ImGui::SeparatorText("Aim");
        ImGui::Checkbox("Aim assist (RMB)", &aim);
        ImGui::SliderFloat("Aim FOV px", &aimFov, 20.f, 600.f, "%.0f");
        ImGui::SliderFloat("Max step", &aimStep, 0.05f, 1.5f, "%.2f");
        ImGui::SeparatorText("Camera");
        ImGui::SliderFloat("Cam Z", &camZ, -50.f, 50.f, "%.1f");
        ImGui::SliderFloat("FOV adj", &fovAdj, -20.f, 20.f, "%.1f");
        ImGui::SliderFloat("FOV ADS", &fovAds, 30.f, 120.f, "%.0f");
        ImGui::SeparatorText("World");
        ImGui::SliderFloat("Max dist", &maxDist, 50.f, 1000.f, "%.0f");
        ImGui::Separator();
        ImGui::TextDisabled("Settings live via shared memory");
        ImGui::End();

        ImGui::Render();
        const float clear[4] = { 0.f, 0.f, 0.f, 0.f };
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        PresentLayered();
        Sleep(10);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (cfg) UnmapViewOfFile(cfg);
    if (map) CloseHandle(map);
    if (g_rtv) g_rtv->Release();
    if (g_stage) g_stage->Release();
    if (g_tex) g_tex->Release();
    if (g_ctx) g_ctx->Release();
    if (g_dev) g_dev->Release();
    DestroyWindow(g_hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
