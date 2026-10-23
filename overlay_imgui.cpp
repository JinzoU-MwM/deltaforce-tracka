#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdio.h>
#include <math.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "shared.h"
static void OLog(const char* s){ FILE* f=nullptr; fopen_s(&f,"ovimg.log","a"); if(f){ fprintf(f,"%s\n",s); fclose(f);} }

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static int W = 1920, H = 1080;
static ID3D11Device* g_dev = nullptr;
static ID3D11DeviceContext* g_ctx = nullptr;
static ID3D11Texture2D* g_tex = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;
static ID3D11Texture2D* g_stage = nullptr;
static HDC g_memDC = nullptr;
static HBITMAP g_bmp = nullptr;
static void* g_bits = nullptr;
static HWND g_hwnd = nullptr, g_gameWnd = nullptr;
static bool g_open = false, g_insPrev = false;
static HHOOK g_hook = nullptr;
static bool g_insDown = false;
static SharedState g_st;
static bool g_haveState = false;

static bool g_esp = true, g_boxes = true, g_names = true, g_dist = true, g_team = true, g_body = true;
static bool g_aim = false;
static float g_aimFov = 160.f, g_aimStep = 0.35f, g_maxDist = 400.f;
static float g_camZ = 0.f, g_fovAdj = 0.f, g_fovAds = 67.f, g_camFwd = 0.f, g_yawOff = 0.f, g_pitchOff = 0.f;

struct V3 { float x, y, z; };

static bool W2S(const SharedState& st, const V3& wp, float& sx, float& sy) {
    float p = (st.camRot.pitch + g_pitchOff) * 3.14159265f / 180.f;
    float y = (st.camRot.yaw + g_yawOff) * 3.14159265f / 180.f;
    float r = st.camRot.roll * 3.14159265f / 180.f;
    float ox = st.camLoc.x, oy = st.camLoc.y, oz = st.camLoc.z + g_camZ;
    ox += cosf(p) * cosf(y) * g_camFwd;
    oy += cosf(p) * sinf(y) * g_camFwd;
    oz += sinf(p) * g_camFwd;
    float dx = wp.x - ox, dy = wp.y - oy, dz = wp.z - oz;
    float sp = sinf(p), cp = cosf(p), sy2 = sinf(y), cy = cosf(y), sr = sinf(r), cr = cosf(r);
    float fwd = (cp * cy) * dx + (cp * sy2) * dy + sp * dz;
    if (fwd < 1.0f) return false;
    float right = (sr * sp * cy - cr * sy2) * dx + (sr * sp * sy2 + cr * cy) * dy + (-sr * cp) * dz;
    float up = (-(cr * sp * cy + sr * sy2)) * dx + (cy * sr - cr * sp * sy2) * dy + (cr * cp) * dz;
    float th = tanf((st.camFov + g_fovAdj) * 3.14159265f / 360.f);
    if (th <= 0.001f) return false;
    float scale = (W * 0.5f) / th;
    sx = W * 0.5f + (right * scale) / fwd;
    sy = H * 0.5f - (up * scale) / fwd;
    return true;
}

static SharedState* g_shm = nullptr;

static bool Snapshot(SharedState& out) {
    if (!g_shm) return false;
    for (int t = 0; t < 4; ++t) {
        LONG s1 = g_shm->seq;
        if (s1 & 1) { Sleep(1); continue; }
        memcpy(&out, g_shm, sizeof(SharedState));
        LONG s2 = g_shm->seq;
        if (s1 == s2 && !(s2 & 1)) return out.magic == SHM_MAGIC && out.version == SHM_VERSION;
    }
    return false;
}

static void SetOpen(bool open) {
    g_open = open;
    OLog(open ? "OPEN" : "CLOSE");
    LONG_PTR ex = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
    if (open) {
        ex &= ~(LONG_PTR)WS_EX_TRANSPARENT;
        SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, ex);
        SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, W, H, SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
        ShowCursor(TRUE);
        ClipCursor(nullptr);
        SetCursor(LoadCursorA(nullptr, IDC_ARROW));
        SetForegroundWindow(g_hwnd);
        SetActiveWindow(g_hwnd);
    } else {
        ex |= WS_EX_TRANSPARENT;
        SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, ex);
        SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, W, H, SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
        ShowCursor(FALSE);
        if (g_gameWnd) SetForegroundWindow(g_gameWnd);
    }
}

static void Calibrate() {
    if (!g_haveState) return;
    const SharedState& st = g_st;
    if (st.state != 1 || !st.camValid) return;
    float best = 250.f; float bx = 0, by = 0; int bi = -1;
    for (int i = 0; i < st.playerCount; ++i) {
        const SPlayer& p = st.players[i];
        if (g_team && st.localCamp >= 0 && p.camp == st.localCamp) continue;
        V3 c{ p.pos.x, p.pos.y, p.pos.z + 60.f };
        float sx, sy;
        if (!W2S(st, c, sx, sy)) continue;
        float dx = sx - W * 0.5f, dy = sy - H * 0.5f;
        float d = sqrtf(dx*dx + dy*dy);
        if (d < best) { best = d; bx = sx; by = sy; bi = i; }
    }
    if (bi < 0) { OLog("CAL no target"); return; }
    {
        const SPlayer& t = st.players[bi];
        if (t.boneCount >= 15) {
            V3 hb{ t.bones[0].x, t.bones[0].y, t.bones[0].z };
            V3 fb{ t.bones[14].x, t.bones[14].y, t.bones[14].z };
            float hx, hy, fx, fy;
            if (W2S(st, hb, hx, hy) && W2S(st, fb, fx, fy)) {
                float Hw = fabsf(hb.z - fb.z);
                float Hp = fabsf(hy - fy);
                float d = t.dist * 100.f;
                if (Hw > 50.f && Hp > 10.f && d > 100.f) {
                    float scale = Hp * d / Hw;
                    if (scale > 1.f) {
                        float fovNew = 2.f * atanf((W * 0.5f) / scale) * 180.f / 3.14159265f;
                        if (fovNew > 30.f && fovNew < 140.f) {
                            g_fovAdj = fovNew - st.camFov;
                            OLog("CAL fov");
                        }
                    }
                }
            }
        }
    }
    float fov = st.camFov + g_fovAdj;
    float scale = (W * 0.5f) / tanf(fov * 3.14159265f / 360.f);
    float dyaw = atan2f(bx - W * 0.5f, scale) * 180.f / 3.14159265f;
    float dpitch = atan2f(by - H * 0.5f, scale) * 180.f / 3.14159265f;
    g_yawOff += dyaw;
    g_pitchOff -= dpitch;
    OLog("CAL done");
}

static LRESULT CALLBACK LLKeyboard(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* k = (KBDLLHOOKSTRUCT*)lParam;
        if (k && k->vkCode == VK_F9) {
            if (wParam == WM_KEYDOWN && !g_insDown) { g_insDown = true; Calibrate(); }
            if (wParam == WM_KEYUP) g_insDown = false;
            return 1;
        }
        if (k && k->vkCode == VK_INSERT) {
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                if (!g_insDown) { g_insDown = true; OLog("LL ins down"); SetOpen(!g_open); }
                return 1;
            }
            if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) { g_insDown = false; return 1; }
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_open && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_HOTKEY:
        OLog("WM_HOTKEY");
        if (wParam == 1) SetOpen(!g_open);
        return 0;
    case WM_SETCURSOR:
        if (g_open) { SetCursor(LoadCursorA(nullptr, IDC_ARROW)); return TRUE; }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static bool MakeD3D() {
    D3D_FEATURE_LEVEL fl;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &g_dev, &fl, &g_ctx))) return false;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = W; td.Height = H; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (FAILED(g_dev->CreateTexture2D(&td, nullptr, &g_tex))) return false;
    if (FAILED(g_dev->CreateRenderTargetView(g_tex, nullptr, &g_rtv))) return false;
    td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(g_dev->CreateTexture2D(&td, nullptr, &g_stage))) return false;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = -H;
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
    for (int y = 0; y < H; ++y)
        memcpy(dst + (size_t)y * W * 4, (unsigned char*)m.pData + (size_t)y * m.RowPitch, (size_t)W * 4);
    g_ctx->Unmap(g_stage, 0);
    for (int i = 0; i < W * H; ++i) {
        unsigned char* p = dst + (size_t)i * 4;
        unsigned int a = p[3];
        p[0] = (unsigned char)((p[0] * a) / 255);
        p[1] = (unsigned char)((p[1] * a) / 255);
        p[2] = (unsigned char)((p[2] * a) / 255);
    }
    HDC screen = GetDC(nullptr);
    POINT pt = { 0, 0 };
    SIZE sz = { W, H };
    POINT src = { 0, 0 };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(g_hwnd, screen, &pt, &sz, g_memDC, &src, 0, &bf, ULW_ALPHA);
    ReleaseDC(nullptr, screen);
}

static void DrawESP() {
    if (!g_haveState) return;
    const SharedState& st = g_st;
    if (st.state != 1 || !st.camValid) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    for (int i = 0; i < st.playerCount; ++i) {
        const SPlayer& p = st.players[i];
        if (p.dist > g_maxDist) continue;
        if (g_team && st.localCamp >= 0 && p.camp == st.localCamp) continue;
        V3 feet{ p.pos.x, p.pos.y, p.pos.z };
        V3 head{ p.pos.x, p.pos.y, p.pos.z + 72.f };
        float fx, fy, hx, hy;
        if (!W2S(st, feet, fx, fy) || !W2S(st, head, hx, hy)) continue;
        float h = fy > hy ? fy - hy : hy - fy;
        if (h > H) continue;
        float bw = h * 0.45f;
        bool en = (st.localCamp >= 0 && p.camp >= 0 && p.camp != st.localCamp);
        bool neu = (p.camp < 0);
        ImU32 fill = en ? IM_COL32(255, 30, 30, 140) : (neu ? IM_COL32(240, 230, 40, 140) : IM_COL32(40, 200, 255, 140));
        ImU32 out  = en ? IM_COL32(255, 80, 80, 250) : (neu ? IM_COL32(255, 245, 60, 250) : IM_COL32(60, 220, 255, 250));
        if (g_body) {
            float sxh, syh, sxf, syf;
            bool okh = false, okf = false;
            if (p.boneCount >= 15) {
                V3 hb{ p.bones[0].x, p.bones[0].y, p.bones[0].z };
                V3 fb{ p.bones[14].x, p.bones[14].y, p.bones[14].z };
                okh = W2S(st, hb, sxh, syh);
                okf = W2S(st, fb, sxf, syf);
            }
            if (!okh || !okf) { sxh = hx; syh = hy; sxf = fx; syf = fy; okh = okf = true; }
            if (okh && okf) {
                float hh = fabsf(syf - syh);
                if (hh > 6.f) {
                    float mx = (sxh + sxf) * 0.5f;
                    float ySh = syh + hh * 0.17f, yHip = syh + hh * 0.55f;
                    float wSh = hh * 0.17f, wHip = hh * 0.115f;
                    float yElb = syh + hh * 0.36f, yHand = syh + hh * 0.60f, yKnee = syh + hh * 0.78f;
                    float hr = hh * 0.062f; if (hr < 2.2f) hr = 2.2f; if (hr > 22.f) hr = 22.f;
                    float lt = hh * 0.045f; if (lt < 1.5f) lt = 1.5f; if (lt > 14.f) lt = 14.f;
                    float lg = hh * 0.055f; if (lg < 1.7f) lg = 1.7f; if (lg > 16.f) lg = 16.f;
                    ImU32 glow = en ? IM_COL32(255, 20, 20, 50) : (neu ? IM_COL32(255, 210, 20, 50) : IM_COL32(20, 150, 255, 50));
                    ImVec2 torso[4] = { ImVec2(mx - wSh, ySh), ImVec2(mx + wSh, ySh), ImVec2(mx + wHip, yHip), ImVec2(mx - wHip, yHip) };
                    // glow
                    dl->AddPolyline(torso, 4, glow, ImDrawFlags_Closed, 5.5f);
                    dl->AddCircle(ImVec2(sxh, syh), hr + 3.5f, glow, 0, 4.f);
                    dl->AddLine(ImVec2(mx - wSh*0.9f, ySh), ImVec2(mx - wSh - hh*0.05f, yElb), glow, lt*2.1f);
                    dl->AddLine(ImVec2(mx - wSh - hh*0.05f, yElb), ImVec2(mx - wSh - hh*0.02f, yHand), glow, lt*2.1f);
                    dl->AddLine(ImVec2(mx + wSh*0.9f, ySh), ImVec2(mx + wSh + hh*0.05f, yElb), glow, lt*2.1f);
                    dl->AddLine(ImVec2(mx + wSh + hh*0.05f, yElb), ImVec2(mx + wSh + hh*0.02f, yHand), glow, lt*2.1f);
                    dl->AddLine(ImVec2(mx - wHip*0.55f, yHip), ImVec2(mx - wHip*0.65f, yKnee), glow, lg*2.1f);
                    dl->AddLine(ImVec2(mx - wHip*0.65f, yKnee), ImVec2(sxf, syf), glow, lg*2.1f);
                    dl->AddLine(ImVec2(mx + wHip*0.55f, yHip), ImVec2(mx + wHip*0.65f, yKnee), glow, lg*2.1f);
                    dl->AddLine(ImVec2(mx + wHip*0.65f, yKnee), ImVec2(sxf, syf), glow, lg*2.1f);
                    // core
                    dl->AddConvexPolyFilled(torso, 4, fill);
                    dl->AddPolyline(torso, 4, out, ImDrawFlags_Closed, 1.6f);
                    dl->AddCircleFilled(ImVec2(sxh, syh), hr, fill);
                    dl->AddCircle(ImVec2(sxh, syh), hr, out, 0, 1.6f);
                    dl->AddLine(ImVec2(mx - wSh*0.9f, ySh), ImVec2(mx - wSh - hh*0.05f, yElb), fill, lt);
                    dl->AddLine(ImVec2(mx - wSh - hh*0.05f, yElb), ImVec2(mx - wSh - hh*0.02f, yHand), fill, lt);
                    dl->AddLine(ImVec2(mx + wSh*0.9f, ySh), ImVec2(mx + wSh + hh*0.05f, yElb), fill, lt);
                    dl->AddLine(ImVec2(mx + wSh + hh*0.05f, yElb), ImVec2(mx + wSh + hh*0.02f, yHand), fill, lt);
                    dl->AddLine(ImVec2(mx - wHip*0.55f, yHip), ImVec2(mx - wHip*0.65f, yKnee), fill, lg);
                    dl->AddLine(ImVec2(mx - wHip*0.65f, yKnee), ImVec2(sxf, syf), fill, lg);
                    dl->AddLine(ImVec2(mx + wHip*0.55f, yHip), ImVec2(mx + wHip*0.65f, yKnee), fill, lg);
                    dl->AddLine(ImVec2(mx + wHip*0.65f, yKnee), ImVec2(sxf, syf), fill, lg);
                }
            }
        }
        if (!g_esp) continue;
        if (h < 4.f) { dl->AddRectFilled(ImVec2(hx-2, hy-2), ImVec2(hx+2, hy+2), out); continue; }
        if (g_boxes) dl->AddRect(ImVec2(fx - bw*0.5f, hy), ImVec2(fx + bw*0.5f, fy), out, 0.f, 0, 1.6f);
        if (g_names) dl->AddText(ImVec2(fx - bw*0.5f, hy - 15.f), out, p.name);
        if (g_dist) { char b[32]; wsprintfA(b, "%dm", (int)p.dist); dl->AddText(ImVec2(fx - bw*0.5f, hy - 1.f), IM_COL32(230,230,150,255), b); }
    }
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    W = GetSystemMetrics(SM_CXSCREEN); H = GetSystemMetrics(SM_CYSCREEN);
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc); wc.style = CS_CLASSDC; wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW); wc.lpszClassName = L"DFImGuiOverlay";
    RegisterClassExW(&wc);
    g_hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
        wc.lpszClassName, L"DF Overlay", WS_POPUP, 0, 0, W, H, nullptr, nullptr, hInst, nullptr);
    if (!g_hwnd) return 1;
    ShowWindow(g_hwnd, SW_SHOW);
    SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, W, H, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    wchar_t gw[64]; int gi = 0; for (; kGameWndName[gi]; ++gi) gw[gi] = (wchar_t)(kGameWndName[gi] ^ 0x5A); gw[gi] = 0;
    g_gameWnd = FindWindowW(gw, nullptr);
    if (!MakeD3D()) return 2;

    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; ImGui::StyleColorsDark();
    ImGuiStyle& st = ImGui::GetStyle(); st.WindowRounding = 6.f; st.FrameRounding = 4.f; st.GrabRounding = 4.f;
    st.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.06f, 0.08f, 0.97f);
    ImGui_ImplWin32_Init(g_hwnd); ImGui_ImplDX11_Init(g_dev, g_ctx);

    wchar_t shmName[64]; int si = 0; for (; kShmName[si]; ++si) shmName[si] = (wchar_t)(kShmName[si] ^ 0x5A); shmName[si] = 0;
    HANDLE map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(SharedState), shmName);
    if (map) g_shm = (SharedState*)MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedState));
    { BOOL rh = RegisterHotKey(g_hwnd, 1, MOD_NOREPEAT, VK_INSERT); OLog(rh ? "reg ok" : "reg FAIL"); }
    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, LLKeyboard, GetModuleHandleW(nullptr), 0);
    OLog(g_hook ? "hook ok" : "hook FAIL");

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;
        if (GetFileAttributesA("C:\\Users\\User\\trackA\\menu.flag") != INVALID_FILE_ATTRIBUTES) {
            DeleteFileA("C:\\Users\\User\\trackA\\menu.flag");
            OLog("FLAG toggle");
            SetOpen(!g_open);
        }
        bool ins = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (ins && !g_insPrev) { OLog("INS edge"); SetOpen(!g_open); }
        g_insPrev = ins;

        g_haveState = Snapshot(g_st);
        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        DrawESP();
        if (g_open) {
            ImGui::SetNextWindowPos(ImVec2(40, 60));
            ImGui::SetNextWindowSize(ImVec2(430, 620));
            ImGui::Begin("DF MOD MENU", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.f, 1.f), "DF MOD MENU v3 (ImGui)");
            ImGui::TextDisabled("INSERT = show/hide  |  mouse aktif saat menu");
            ImGui::SeparatorText("ESP");
            ImGui::Checkbox("ESP", &g_esp);
            ImGui::Checkbox("Box", &g_boxes);
            ImGui::Checkbox("Name", &g_names);
            ImGui::Checkbox("Distance", &g_dist);
            ImGui::Checkbox("Team filter (enemy only)", &g_team);
            ImGui::Checkbox("Body chams", &g_body);
            ImGui::SeparatorText("Aim");
            ImGui::Checkbox("Aim assist (RMB)", &g_aim);
            ImGui::SliderFloat("Aim FOV px", &g_aimFov, 20.f, 600.f, "%.0f");
            ImGui::SliderFloat("Max step", &g_aimStep, 0.05f, 1.5f, "%.2f");
            ImGui::SeparatorText("Camera");
            ImGui::SliderFloat("Cam Z", &g_camZ, -50.f, 50.f, "%.1f");
            ImGui::SliderFloat("FOV adj", &g_fovAdj, -20.f, 20.f, "%.1f");
            ImGui::SliderFloat("FOV ADS", &g_fovAds, 30.f, 120.f, "%.0f");
            ImGui::SliderFloat("Cam Fwd", &g_camFwd, -100.f, 100.f, "%.1f");
            ImGui::SliderFloat("Yaw offset", &g_yawOff, -15.f, 15.f, "%.2f");
            ImGui::SliderFloat("Pitch offset", &g_pitchOff, -15.f, 15.f, "%.2f");
            if (ImGui::Button("Calibrate (F9)  - aim at target")) Calibrate();
            ImGui::SeparatorText("World");
            ImGui::SliderFloat("Max dist", &g_maxDist, 50.f, 1000.f, "%.0f");
            ImGui::End();
        }
        ImGui::Render();
        const float clear[4] = { 0.f, 0.f, 0.f, 0.f };
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        PresentLayered();
        Sleep(16);
    }

    if (g_hook) UnhookWindowsHookEx(g_hook);
    UnregisterHotKey(g_hwnd, 1);
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    if (g_shm) UnmapViewOfFile(g_shm);
    if (map) CloseHandle(map);
    if (g_rtv) g_rtv->Release(); if (g_stage) g_stage->Release(); if (g_tex) g_tex->Release();
    if (g_ctx) g_ctx->Release(); if (g_dev) g_dev->Release();
    DestroyWindow(g_hwnd); UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
