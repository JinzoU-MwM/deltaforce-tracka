#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
static bool g_dwmOk = true;
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

static bool g_esp = true, g_boxes = true, g_names = true, g_dist = true, g_team = false, g_body = true;
static bool g_aim = false;
static bool g_aimAuto = false;
static float g_aimFov = 320.f, g_aimStep = 0.35f, g_maxDist = 1000.f;
static float g_camZ = 0.f, g_fovAdj = 0.f, g_fovAds = 67.f, g_camFwd = 0.f, g_yawOff = 0.f, g_pitchOff = 0.f;

static int cDrawnLive = 0, cHoldLive = 0;

static void ApplyDefaults() {
    g_esp = true; g_boxes = true; g_names = true; g_dist = true;
    g_team = false; g_body = true; g_aim = false; g_aimAuto = false;
    g_aimFov = 320.f; g_aimStep = 0.35f; g_maxDist = 1000.f;
    g_camZ = 0.f; g_fovAdj = 0.f; g_fovAds = 67.f; g_camFwd = 0.f;
    g_yawOff = 0.f; g_pitchOff = 0.f;
}

static void SaveCfg() {
    FILE* f = nullptr;
    if (fopen_s(&f, "overlay_cfg.txt", "w") != 0 || !f) return;
    fprintf(f, "# DF trackA overlay config v31\n");
    fprintf(f, "esp=%d\nboxes=%d\nnames=%d\ndist=%d\nteam=%d\nbody=%d\naim=%d\naimAuto=%d\n",
            g_esp, g_boxes, g_names, g_dist, g_team, g_body, g_aim, g_aimAuto);
    fprintf(f, "aimFov=%.3f\naimStep=%.4f\nmaxDist=%.2f\n", g_aimFov, g_aimStep, g_maxDist);
    fprintf(f, "camZ=%.3f\nfovAdj=%.3f\nfovAds=%.3f\ncamFwd=%.3f\nyawOff=%.3f\npitchOff=%.3f\n",
            g_camZ, g_fovAdj, g_fovAds, g_camFwd, g_yawOff, g_pitchOff);
    fclose(f);
    OLog("CFG saved");
}

static void LoadCfg() {
    FILE* f = nullptr;
    if (fopen_s(&f, "overlay_cfg.txt", "r") != 0 || !f) { OLog("CFG missing"); return; }
    char line[160];
    while (fgets(line, sizeof(line), f)) {
        char* nl = strpbrk(line, "\r\n"); if (nl) *nl = 0;
        if (line[0] == '#' || line[0] == 0) continue;
        char* eq = strchr(line, '='); if (!eq) continue;
        *eq = 0;
        const char* k = line; const char* v = eq + 1;
        float fv = (float)atof(v); int iv = atoi(v);
        if (!strcmp(k, "esp")) g_esp = iv != 0;
        else if (!strcmp(k, "boxes")) g_boxes = iv != 0;
        else if (!strcmp(k, "names")) g_names = iv != 0;
        else if (!strcmp(k, "dist")) g_dist = iv != 0;
        else if (!strcmp(k, "team")) g_team = iv != 0;
        else if (!strcmp(k, "body")) g_body = iv != 0;
        else if (!strcmp(k, "aim")) g_aim = iv != 0;
        else if (!strcmp(k, "aimAuto")) g_aimAuto = iv != 0;
        else if (!strcmp(k, "aimFov")) g_aimFov = fv;
        else if (!strcmp(k, "aimStep")) g_aimStep = fv;
        else if (!strcmp(k, "maxDist")) g_maxDist = fv;
        else if (!strcmp(k, "camZ")) g_camZ = fv;
        else if (!strcmp(k, "fovAdj")) g_fovAdj = fv;
        else if (!strcmp(k, "fovAds")) g_fovAds = fv;
        else if (!strcmp(k, "camFwd")) g_camFwd = fv;
        else if (!strcmp(k, "yawOff")) g_yawOff = fv;
        else if (!strcmp(k, "pitchOff")) g_pitchOff = fv;
    }
    fclose(f);
    OLog("CFG loaded");
}

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
    float fovUse = st.camFov + g_fovAdj;
    if ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) && g_fovAds > 5.f) fovUse = g_fovAds;
    float th = tanf(fovUse * 3.14159265f / 360.f);
    if (th <= 0.001f) return false;
    float scale = (W * 0.5f) / th;
    sx = W * 0.5f + (right * scale) / fwd;
    sy = H * 0.5f - (up * scale) / fwd;
    return true;
}

static SharedState* g_shm = nullptr;
static uint32_t g_shmVer = 0;
static MenuCfg* g_menu = nullptr;

// --- anti-blink: snapshot tahan-last-good ---------------------------------
static SharedState g_lastGood;
static DWORD       g_lastGoodT = 0;
static bool        g_snapHas = false;
static int         g_snapMiss = 0;

static bool Snapshot(SharedState& out) {
    if (!g_shm) return false;
    // seqlock: spin, jangan Sleep (Sleep(1) bikin frame drop)
    for (int t = 0; t < 64; ++t) {
        LONG s1 = g_shm->seq;
        if (s1 & 1) { YieldProcessor(); continue; }
        memcpy(&out, g_shm, sizeof(SharedState));
        _ReadWriteBarrier();
        LONG s2 = g_shm->seq;
        if (s1 == s2 && !(s2 & 1) &&
            out.magic == SHM_MAGIC && out.version >= 22 && out.version <= 30) {
            g_lastGood = out;
            g_lastGoodT = GetTickCount();
            g_snapHas = true;
            g_snapMiss = 0;
            return true;
        }
        YieldProcessor();
    }
    // gagal baca konsisten: pakai snapshot terakhir (ESP beku, bukan hilang)
    if (g_snapHas) {
        out = g_lastGood;
        ++g_snapMiss;
        return true;
    }
    ++g_snapMiss;
    return false;
}



static SharedState g_draw;
static SRot g_prevRot{}; static SVec3 g_prevLoc{}; static DWORD g_prevT = 0; static LONG g_prevSeq = 0;
static float g_velYaw = 0, g_velPitch = 0, g_velX = 0, g_velY = 0, g_velZ = 0;
static SPlayer g_pp[128]; static DWORD g_ppT[128]; static int g_ppN = 0;

static void UpdatePrediction() {
    if (!g_haveState) { if (g_snapHas) g_draw = g_lastGood; return; }
    DWORD now = GetTickCount();
    bool newSnap = (g_st.seq != g_prevSeq);
    if (newSnap) {
        if (g_prevT) {
            float dt = (now - g_prevT) / 1000.f;
            if (dt > 0.001f && dt < 0.5f) {
                float dy = g_st.camRot.yaw - g_prevRot.yaw;
                while (dy > 180.f) dy -= 360.f; while (dy < -180.f) dy += 360.f;
                float nvy = dy / dt, nvp = (g_st.camRot.pitch - g_prevRot.pitch) / dt;
                float nvx = (g_st.camLoc.x - g_prevLoc.x) / dt;
                float nvyy = (g_st.camLoc.y - g_prevLoc.y) / dt;
                float nvz = (g_st.camLoc.z - g_prevLoc.z) / dt;
                g_velYaw = g_velYaw * 0.65f + nvy * 0.35f;
                g_velPitch = g_velPitch * 0.65f + nvp * 0.35f;
                g_velX = g_velX * 0.65f + nvx * 0.35f;
                g_velY = g_velY * 0.65f + nvyy * 0.35f;
                g_velZ = g_velZ * 0.65f + nvz * 0.35f;
            }
        }
        g_prevRot = g_st.camRot; g_prevLoc = g_st.camLoc; g_prevT = now; g_prevSeq = g_st.seq;
    }
    float pred = (now - g_prevT) / 1000.f;
    if (pred < 0) pred = 0; if (pred > 0.04f) pred = 0.04f;
    g_draw = g_st;
    float py = g_velYaw * pred, pp = g_velPitch * pred;
    float px = g_velX * pred, pyy = g_velY * pred, pz = g_velZ * pred;
    if (py > 1.5f) py = 1.5f; if (py < -1.5f) py = -1.5f;
    if (pp > 1.5f) pp = 1.5f; if (pp < -1.5f) pp = -1.5f;
    if (px > 40.f) px = 40.f; if (px < -40.f) px = -40.f;
    if (pyy > 40.f) pyy = 40.f; if (pyy < -40.f) pyy = -40.f;
    if (pz > 40.f) pz = 40.f; if (pz < -40.f) pz = -40.f;
    g_draw.camRot.yaw += py;
    g_draw.camRot.pitch += pp;
    g_draw.camLoc.x += px;
    g_draw.camLoc.y += pyy;
    g_draw.camLoc.z += pz;
    for (int i = 0; i < g_st.playerCount && i < 128; ++i) {
        SPlayer p = g_st.players[i];
        if (i < g_ppN && g_pp[i].pawn == p.pawn) {
            float dt = (now - g_ppT[i]) / 1000.f;
            if (dt > 0.001f && dt < 0.5f) {
                float vx = (p.pos.x - g_pp[i].pos.x) / dt;
                float vy = (p.pos.y - g_pp[i].pos.y) / dt;
                float vz = (p.pos.z - g_pp[i].pos.z) / dt;
                float dx = vx * pred, dy2 = vy * pred, dz = vz * pred;
                if (dx > 40.f) dx = 40.f; if (dx < -40.f) dx = -40.f;
                if (dy2 > 40.f) dy2 = 40.f; if (dy2 < -40.f) dy2 = -40.f;
                if (dz > 40.f) dz = 40.f; if (dz < -40.f) dz = -40.f;
                p.pos.x += dx; p.pos.y += dy2; p.pos.z += dz;
                for (int b = 0; b < p.boneCount && b < 80; ++b) {
                    p.bones[b].x += dx; p.bones[b].y += dy2; p.bones[b].z += dz;
                }
            }
        }
        g_draw.players[i] = p;
    }
    if (newSnap) {
        g_ppN = g_st.playerCount;
        for (int i = 0; i < g_st.playerCount && i < 128; ++i) { g_pp[i] = g_st.players[i]; g_ppT[i] = now; }
    }
}
static void WriteAimCmd(bool en, float yaw, float pitch) {
    if (!g_shm) return;
    InterlockedIncrement(&g_shm->cmd.seq);
    g_shm->cmd.enable = en ? 1 : 0;
    g_shm->cmd.yaw = yaw; g_shm->cmd.pitch = pitch;
    g_shm->cmd.maxStep = g_aimStep;
    g_shm->cmd.stamp = GetTickCount();
    g_shm->cmd.mode = 0;
    InterlockedIncrement(&g_shm->cmd.seq);
}

static void AimTick() {
    if (g_shmVer < 23) { WriteAimCmd(false, 0, 0); return; }
    if (!g_haveState || g_draw.state != 1 || !g_draw.camValid) { WriteAimCmd(false, 0, 0); return; }
    bool autoFlag = (GetFileAttributesA("C:\\Users\\User\\trackA\\aim2.flag") != INVALID_FILE_ATTRIBUTES);
    bool active = g_aimAuto || autoFlag || (g_aim && (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
    if (!active) { WriteAimCmd(false, 0, 0); return; }
    float best = 1e9f; int bi = -1; float tx = 0, ty = 0, tz = 0;
    for (int i = 0; i < g_draw.playerCount; ++i) {
        const SPlayer& p = g_draw.players[i];
        if (g_team && g_draw.localCamp >= 0 && p.camp == g_draw.localCamp) continue;
        if (p.dist > g_maxDist) continue;
        float hx, hy, hz;
        if (p.boneCount > 32) { hx = p.bones[32].x; hy = p.bones[32].y; hz = p.bones[32].z; }
        else { hx = p.pos.x; hy = p.pos.y; hz = p.pos.z + 62.f; }
        float sx, sy;
        if (!W2S(g_draw, V3{ hx, hy, hz }, sx, sy)) continue;
        float dx = sx - W * 0.5f, dy = sy - H * 0.5f;
        float d = sqrtf(dx * dx + dy * dy);
        if (d <= g_aimFov && d < best) { best = d; bi = i; tx = hx; ty = hy; tz = hz; }
    }
    static int lastTgt = -1; static DWORD lastSwitch = 0;
    DWORD now = GetTickCount();
    if (bi < 0) { lastTgt = -1; WriteAimCmd(false, 0, 0); return; }
    if (bi != lastTgt) { lastTgt = bi; lastSwitch = now; WriteAimCmd(false, 0, 0); return; }
    if (now - lastSwitch < 120) { WriteAimCmd(false, 0, 0); return; }
    float dx = tx - g_draw.camLoc.x, dy = ty - g_draw.camLoc.y, dz = tz - g_draw.camLoc.z;
    float wy = atan2f(dy, dx) * 180.f / 3.14159265f;
    float wp = atan2f(dz, sqrtf(dx * dx + dy * dy)) * 180.f / 3.14159265f;
    WriteAimCmd(true, wy, wp);
}

static void MaybeSpawnShot() {
    if (GetFileAttributesA("C:\\Users\\User\\trackA\\spawn_shot.flag") == INVALID_FILE_ATTRIBUTES) return;
    DeleteFileA("C:\\Users\\User\\trackA\\spawn_shot.flag");
    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    char cwd[] = "C:\\Users\\User\\trackA";
    if (CreateProcessA("C:\\Users\\User\\trackA\\shot.exe", nullptr, nullptr, nullptr, FALSE, 0, nullptr, cwd, &si, &pi)) {
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
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

    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_open && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {

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
    // ImGui blend (SRC_ALPHA / INV_SRC_ALPHA) di target transparan sudah menghasilkan
    // RGB premultiplied -> loop 2M-piksel per frame tidak perlu (sumber jitter/fps drop).
    HDC screen = GetDC(nullptr);
    POINT pt = { 0, 0 };
    SIZE sz = { W, H };
    POINT src = { 0, 0 };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(g_hwnd, screen, &pt, &sz, g_memDC, &src, 0, &bf, ULW_ALPHA);
    ReleaseDC(nullptr, screen);
}

static int Cross2(const ImVec2& O, const ImVec2& A, const ImVec2& B) {
    float v = (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
    return v > 0 ? 1 : (v < 0 ? -1 : 0);
}
static int CmpPt(const void* a, const void* b) {
    const ImVec2* p = (const ImVec2*)a; const ImVec2* q = (const ImVec2*)b;
    if (p->x < q->x) return -1; if (p->x > q->x) return 1;
    if (p->y < q->y) return -1; if (p->y > q->y) return 1; return 0;
}
static int ConvexHull(ImVec2* p, int n, ImVec2* h) {
    if (n < 3) return 0;
    qsort(p, n, sizeof(ImVec2), CmpPt);
    int k = 0;
    for (int i = 0; i < n; ++i) {
        while (k >= 2 && Cross2(h[k-2], h[k-1], p[i]) <= 0) --k;
        h[k++] = p[i];
    }
    for (int i = n - 2, t = k + 1; i >= 0; --i) {
        while (k >= t && Cross2(h[k-2], h[k-1], p[i]) <= 0) --k;
        h[k++] = p[i];
    }
    return k - 1;
}

static void DrawESP() {
    {
        ImDrawList* dl0 = ImGui::GetBackgroundDrawList();
        bool act = g_aim || g_aimAuto || (GetFileAttributesA("C:\Users\User\trackA\aim2.flag") != INVALID_FILE_ATTRIBUTES);
        dl0->AddCircle(ImVec2(W * 0.5f, H * 0.5f), g_aimFov, act ? IM_COL32(0, 255, 140, 160) : IM_COL32(0, 255, 140, 70), 0, 1.6f);
    }
    static int cDrawn = 0, cTeam = 0, cDist = 0, cOff = 0, cHold = 0;
    static DWORD cT = 0;
    static SharedState g_lastValid; static DWORD g_lastValidT = 0; static bool g_lastValidHas = false;
    if (!g_haveState) return;
    SharedState st = g_draw;
    bool okNow = (st.state == 1 && st.camValid);
    if (okNow) { g_lastValid = st; g_lastValidT = GetTickCount(); g_lastValidHas = true; }
    else if (g_lastValidHas && (GetTickCount() - g_lastValidT) < 600) { st = g_lastValid; ++cHold; }
    else return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    for (int i = 0; i < st.playerCount; ++i) {
        const SPlayer& p = st.players[i];
        if (p.dist > g_maxDist) { ++cDist; continue; }
        if (g_team && st.localCamp >= 0 && p.camp == st.localCamp) { ++cTeam; continue; }
        V3 feet{ p.pos.x, p.pos.y, p.pos.z };
        V3 head{ p.pos.x, p.pos.y, p.pos.z + 72.f };
        float fx, fy, hx, hy;
        if (!W2S(st, feet, fx, fy) || !W2S(st, head, hx, hy)) { ++cOff; continue; }
        float h = fy > hy ? fy - hy : hy - fy;
        if (h > H) { ++cOff; continue; }
        ++cDrawn;
        float bw = h * 0.45f;
        bool en = (st.localCamp >= 0 && p.camp >= 0 && p.camp != st.localCamp);
        bool neu = (p.camp < 0);
        ImU32 fill = en ? IM_COL32(255, 30, 30, 140) : (neu ? IM_COL32(240, 230, 40, 140) : IM_COL32(40, 200, 255, 140));
        ImU32 out  = en ? IM_COL32(255, 80, 80, 250) : (neu ? IM_COL32(255, 245, 60, 250) : IM_COL32(60, 220, 255, 250));
        if (g_body) {
            if (p.boneCount >= 66) {
                ImU32 fill = en ? IM_COL32(255, 30, 30, 160) : (neu ? IM_COL32(240, 230, 40, 160) : IM_COL32(40, 200, 255, 160));
                ImU32 out  = en ? IM_COL32(255, 90, 90, 255) : (neu ? IM_COL32(255, 245, 60, 255) : IM_COL32(60, 220, 255, 255));
                ImU32 glow = en ? IM_COL32(255, 20, 20, 55) : (neu ? IM_COL32(255, 210, 20, 55) : IM_COL32(20, 150, 255, 55));
                auto PP = [&](int i, ImVec2& o) -> bool {
                    if (i < 0 || i >= p.boneCount) return false;
                    V3 b{ p.bones[i].x, p.bones[i].y, p.bones[i].z };
                    return W2S(st, b, o.x, o.y);
                };
                auto L = [&](int a, int b, float th) {
                    ImVec2 p1, p2;
                    if (PP(a, p1) && PP(b, p2)) { dl->AddLine(p1, p2, glow, th * 2.2f); dl->AddLine(p1, p2, out, th); }
                };
                L(1, 2, 2.4f); L(2, 3, 2.3f); L(3, 4, 2.2f); L(4, 5, 2.2f); L(5, 31, 2.0f); L(31, 32, 1.8f);
                L(5, 6, 1.8f); L(6, 7, 2.0f); L(7, 8, 1.8f); L(8, 9, 1.5f);
                L(5, 33, 1.8f); L(33, 34, 2.0f); L(34, 35, 1.8f); L(35, 36, 1.5f);
                L(1, 58, 2.2f); L(58, 59, 2.0f); L(59, 60, 1.8f); L(60, 61, 1.5f);
                L(1, 62, 2.2f); L(62, 63, 2.0f); L(63, 64, 1.8f); L(64, 65, 1.5f);
                ImVec2 hp2, fp2; bool okh2 = PP(32, hp2);
                float hr = 6.f;
                if (okh2 && PP(60, fp2)) { float hh2 = fabsf(fp2.y - hp2.y); hr = hh2 * 0.06f; if (hr < 3.f) hr = 3.f; if (hr > 20.f) hr = 20.f; }
                if (okh2) { dl->AddCircleFilled(hp2, hr, fill); dl->AddCircle(hp2, hr, out, 0, 1.6f); }
                int joints[24] = { 1,2,3,4,5,6,7,8,9,31,32,33,34,35,36,58,59,60,61,62,63,64,65,0 };
                for (int ji = 0; ji < 24; ++ji) { ImVec2 jp; if (PP(joints[ji], jp)) dl->AddCircleFilled(jp, 2.0f, out); }
            }
        }
        if (!g_esp) continue;
        if (h < 4.f) { dl->AddRectFilled(ImVec2(hx-2, hy-2), ImVec2(hx+2, hy+2), out); continue; }
        if (g_boxes) dl->AddRect(ImVec2(fx - bw*0.5f, hy), ImVec2(fx + bw*0.5f, fy), out, 0.f, 0, 1.6f);
        if (g_names) dl->AddText(ImVec2(fx - bw*0.5f, hy - 15.f), out, p.name);
        if (g_dist) { char b[32]; wsprintfA(b, "%dm", (int)p.dist); dl->AddText(ImVec2(fx - bw*0.5f, hy - 1.f), IM_COL32(230,230,150,255), b); }
    }
    cDrawnLive = cDrawn; cHoldLive = cHold;
    {
        DWORD nw = GetTickCount();
        if (nw - cT >= 1000) {
            cT = nw;
            char lg[192];
            sprintf_s(lg, "DRAW ply=%d drawn=%d team=%d dist=%d off=%d hold=%d miss=%d maxd=%.0f tf=%d",
                     st.playerCount, cDrawn, cTeam, cDist, cOff, cHold, g_snapMiss, g_maxDist, g_team ? 1 : 0);
            OLog(lg);
            cDrawn = cTeam = cDist = cOff = cHold = 0;
        }
    }
}


static const unsigned char kName22[24] = {0x16,0x35,0x39,0x3B,0x36,0x06,0x17,0x09,0x0E,0x1C,0x74,0x09,0x32,0x33,0x37,0x19,0x3B,0x39,0x32,0x3F,0x74,0x2C,0x7C,0x00};
static const unsigned char kName23[24] = {0x16,0x35,0x39,0x3B,0x36,0x06,0x17,0x09,0x0E,0x1C,0x74,0x09,0x32,0x33,0x37,0x19,0x3B,0x39,0x32,0x3F,0x74,0x2C,0x7D,0x00};
static const unsigned char kName24[24] = {0x16,0x35,0x39,0x3B,0x36,0x06,0x17,0x09,0x0E,0x1C,0x74,0x09,0x32,0x33,0x37,0x19,0x3B,0x39,0x32,0x3F,0x74,0x2C,0x7E,0x00};
static const unsigned char kName25[24] = {0x16,0x35,0x39,0x3B,0x36,0x06,0x17,0x09,0x0E,0x1C,0x74,0x09,0x32,0x33,0x37,0x19,0x3B,0x39,0x32,0x3F,0x74,0x2C,0x7F,0x00};
static const unsigned char kName26[24] = {0x16,0x35,0x39,0x3B,0x36,0x06,0x17,0x09,0x0E,0x1C,0x74,0x09,0x32,0x33,0x37,0x19,0x3B,0x39,0x32,0x3F,0x74,0x2C,0x80,0x00};
static const unsigned char kName27[24] = {0x16,0x35,0x39,0x3B,0x36,0x06,0x17,0x09,0x0E,0x1C,0x74,0x09,0x32,0x33,0x37,0x19,0x3B,0x39,0x32,0x3F,0x74,0x2C,0x81,0x00};
static const unsigned char kName28[24] = {0x16,0x35,0x39,0x3B,0x36,0x06,0x17,0x09,0x0E,0x1C,0x74,0x09,0x32,0x33,0x37,0x19,0x3B,0x39,0x32,0x3F,0x74,0x2C,0x82,0x00};
static const unsigned char kName29[24] = {0x16,0x35,0x39,0x3B,0x36,0x06,0x17,0x09,0x0E,0x1C,0x74,0x09,0x32,0x33,0x37,0x19,0x3B,0x39,0x32,0x3F,0x74,0x2C,0x83,0x00};
static const unsigned char kName30[24] = {0x16,0x35,0x39,0x3B,0x36,0x06,0x17,0x09,0x0E,0x1C,0x74,0x09,0x32,0x33,0x37,0x19,0x3B,0x39,0x32,0x3F,0x74,0x2C,0x84,0x00};

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

    static const unsigned char* kAllNames[9] = { kName22, kName23, kName24, kName25, kName26, kName27, kName28, kName29, kName30 };
    {
        for (int vi = 8; vi >= 0; --vi) {
            wchar_t nm[64]; int si = 0;
            for (; si < 32 && kAllNames[vi][si]; ++si) nm[si] = (wchar_t)(kAllNames[vi][si] ^ 0x5A);
            nm[si] = 0;
            if (si == 0) continue;
            HANDLE map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(SharedState), nm);
            if (!map) { char lg[64]; sprintf_s(lg, "map fail vi=%d err=%lu", vi, GetLastError()); OLog(lg); continue; }
            void* view = MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, 0);
            if (!view) { char lg[64]; sprintf_s(lg, "view fail vi=%d err=%lu", vi, GetLastError()); OLog(lg); CloseHandle(map); continue; }
            SharedState* cand = (SharedState*)view;
            { char lg[96]; sprintf_s(lg, "cand vi=%d magic=%08X ver=%u state=%d players=%d", vi, cand->magic, cand->version, cand->state, cand->playerCount); OLog(lg); }
            if (cand->magic == SHM_MAGIC && cand->version >= 22 && cand->version <= 30 && cand->state <= 4 && cand->playerCount >= 0 && cand->playerCount <= 128) {
                g_shm = cand; g_shmVer = cand->version;
                char lg[64]; sprintf_s(lg, "shm v%u state=%d players=%d", cand->version, cand->state, cand->playerCount); OLog(lg);
                break;
            }
            UnmapViewOfFile(view); CloseHandle(map);
        }
    }
    { HANDLE mm = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(MenuCfg), L"DFC6_menu_v1");
      if (mm) g_menu = (MenuCfg*)MapViewOfFile(mm, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MenuCfg)); }
    LoadCfg();
    OLog("hotkey: core-only");
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


        g_haveState = Snapshot(g_st);
        UpdatePrediction();
        if (g_menu) {
            g_menu->magic = MENU_MAGIC; g_menu->version = MENU_VERSION;
            g_menu->esp = g_esp; g_menu->boxes = g_boxes; g_menu->names = g_names; g_menu->dist = g_dist;
            g_menu->team = g_team; g_menu->body = g_body; g_menu->aim = 0;
            g_menu->aimFov = g_aimFov; g_menu->aimStep = g_aimStep; g_menu->maxDist = g_maxDist;
            g_menu->camZ = g_camZ; g_menu->fovAdj = g_fovAdj; g_menu->fovAds = g_fovAds;
            g_menu->visible = 0; g_menu->pad = 0;
        }
        {
            static LONG lastKs = 0;
            LONG ks = g_shm ? g_shm->keySeq : 0;
            if (ks != lastKs) {
                lastKs = ks;
                int bits = g_shm ? g_shm->keyEdges : 0;
                if ((bits & 1) || (bits & 8)) SetOpen(!g_open);
            }
        }
        AimTick();
        MaybeSpawnShot();
        {
            static DWORD lastSt = 0;
            DWORD nw = GetTickCount();
            if (nw - lastSt >= 1000) {
                lastSt = nw;
                char lg[256];
                const char* n0 = (g_st.playerCount > 0) ? g_st.players[0].name : "-";
                sprintf_s(lg, "STATE st=%d act=%d ply=%d cam=%d camp=%d team=%d n0=%s", g_st.state, g_st.actorCount, g_st.playerCount, g_st.camValid, g_st.localCamp, g_st.localTeam, n0);
                OLog(lg);
            }
        }
        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        DrawESP();
        if (g_open) {
            ImGui::SetNextWindowPos(ImVec2(40, 60));
            ImGui::SetNextWindowSize(ImVec2(430, 620));
            ImGui::SetNextWindowPos(ImVec2(40, 60), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(468, 560), ImGuiCond_FirstUseEver);
            ImGui::Begin("DF TRACK A  -  MOD MENU", nullptr, 0);
            ImGui::TextColored(ImVec4(0.30f, 0.90f, 1.00f, 1.f), "Delta Force  trackA  v31");
            ImGui::SameLine();
            ImGui::TextDisabled("| shm v%u | %.0f fps", g_shmVer, ImGui::GetIO().Framerate);
            ImGui::TextDisabled("INSERT / Ctrl+Alt+M = show-hide   F9 = calibrate");
            if (ImGui::BeginTabBar("##dftabs", ImGuiTabBarFlags_None)) {

                if (ImGui::BeginTabItem("ESP")) {
                    ImGui::Spacing();
                    ImGui::Checkbox("ESP master", &g_esp);
                    ImGui::Checkbox("Box", &g_boxes);
                    ImGui::Checkbox("Name", &g_names);
                    ImGui::Checkbox("Distance", &g_dist);
                    ImGui::Checkbox("Body chams (skeleton)", &g_body);
                    ImGui::SeparatorText("Filter");
                    ImGui::Checkbox("Team filter (enemy only)", &g_team);
                    ImGui::SliderFloat("Max dist", &g_maxDist, 50.f, 1000.f, "%.0f m");
                    ImGui::SeparatorText("Status");
                    ImGui::Text("players=%d  drawn=%d", g_st.playerCount, cDrawnLive);
                    ImGui::Text("camp=%d  team=%d  cam=%d", g_st.localCamp, g_st.localTeam, g_st.camValid);
                    ImGui::TextDisabled("hold=%d  snapmiss=%d", cHoldLive, g_snapMiss);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("AIM")) {
                    ImGui::Spacing();
                    ImGui::Checkbox("Aim assist (hold RMB)", &g_aim);
                    ImGui::Checkbox("Auto aim (always)", &g_aimAuto);
                    ImGui::SeparatorText("Tuning");
                    ImGui::SliderFloat("Aim FOV (px)", &g_aimFov, 20.f, 600.f, "%.0f px");
                    ImGui::SliderFloat("Max step (deg/8ms)", &g_aimStep, 0.05f, 1.50f, "%.2f");
                    ImGui::SeparatorText("Target");
                    ImGui::TextDisabled("head bone = 32, path = ROTATION_INPUT");
                    if (ImGui::Button("Center FOV to screen")) g_aimFov = (float)H * 0.30f;
                    ImGui::SameLine();
                    if (ImGui::Button("Reset aim")) { g_aimFov = 320.f; g_aimStep = 0.35f; }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("CAMERA")) {
                    ImGui::Spacing();
                    ImGui::SliderFloat("Cam Z", &g_camZ, -50.f, 50.f, "%.1f");
                    ImGui::SliderFloat("Cam Fwd", &g_camFwd, -100.f, 100.f, "%.1f");
                    ImGui::SliderFloat("FOV adj", &g_fovAdj, -20.f, 20.f, "%.1f");
                    ImGui::SliderFloat("FOV ADS", &g_fovAds, 30.f, 120.f, "%.0f");
                    ImGui::SeparatorText("Offset W2S");
                    ImGui::SliderFloat("Yaw offset", &g_yawOff, -15.f, 15.f, "%.2f");
                    ImGui::SliderFloat("Pitch offset", &g_pitchOff, -15.f, 15.f, "%.2f");
                    ImGui::SeparatorText("");
                    if (ImGui::Button("Calibrate (F9) - aim at target", ImVec2(-1, 0))) Calibrate();
                    ImGui::TextDisabled("cam mode=%d  fov=%.1f", g_st.camMode, g_st.camFov);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("SETTINGS")) {
                    ImGui::Spacing();
                    if (ImGui::Button("Save config", ImVec2(140, 0))) SaveCfg();
                    ImGui::SameLine();
                    if (ImGui::Button("Load config", ImVec2(140, 0))) LoadCfg();
                    ImGui::SameLine();
                    if (ImGui::Button("Defaults", ImVec2(120, 0))) ApplyDefaults();
                    ImGui::SeparatorText("Overlay");
                    ImGui::TextDisabled("config file: overlay_cfg.txt (folder overlay)");
                    ImGui::TextDisabled("log: ovimg.log");
                    ImGui::Spacing();
                    ImGui::TextDisabled("Hotkey: INSERT / Ctrl+Alt+M lewat core (PollKeys)");
                    ImGui::TextDisabled("Fallback file: menu.flag (ToggleMenu.bat)");
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("INFO")) {
                    ImGui::Spacing();
                    ImGui::Text("shm version : %u", g_shmVer);
                    ImGui::Text("state       : %d  (1=ok 2=no world 3=level 4=GI)", g_st.state);
                    ImGui::Text("actors      : %d", g_st.actorCount);
                    ImGui::Text("players     : %d", g_st.playerCount);
                    ImGui::Text("camera      : valid=%d mode=%d fov=%.1f", g_st.camValid, g_st.camMode, g_st.camFov);
                    ImGui::Text("local       : camp=%d team=%d dead=%d", g_st.localCamp, g_st.localTeam, g_st.selfDead);
                    ImGui::SeparatorText("Health");
                    ImGui::Text("snapshot miss : %d", g_snapMiss);
                    ImGui::Text("hold frames   : %d", cHoldLive);
                    ImGui::Text("drawn (1s)    : %d", cDrawnLive);
                    ImGui::Text("fps           : %.0f", ImGui::GetIO().Framerate);
                    if (g_st.playerCount > 0) {
                        ImGui::SeparatorText("Players");
                        for (int i = 0; i < g_st.playerCount && i < 8; ++i) {
                            const SPlayer& p = g_st.players[i];
                            ImGui::Text("[%d] %-20s camp=%d dist=%.0f", i, p.name[0] ? p.name : "?", p.camp, p.dist);
                        }
                    }
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
            ImGui::End();
        }
        ImGui::Render();
        const float clear[4] = { 0.f, 0.f, 0.f, 0.f };
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        PresentLayered();
        if (!g_dwmOk || FAILED(DwmFlush())) Sleep(16);   // sinkron 1 present / refresh
    }

    if (g_hook) UnhookWindowsHookEx(g_hook);
    UnregisterHotKey(g_hwnd, 1);
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    if (g_shm) UnmapViewOfFile(g_shm);
    if (g_rtv) g_rtv->Release(); if (g_stage) g_stage->Release(); if (g_tex) g_tex->Release();
    if (g_ctx) g_ctx->Release(); if (g_dev) g_dev->Release();
    DestroyWindow(g_hwnd); UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
