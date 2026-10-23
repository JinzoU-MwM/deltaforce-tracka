// dfoverlay: external layered overlay, reads shared state, no hooks in game
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <math.h>
#include <string.h>
#include <stddef.h>
#include "shared.h"
#include <stdio.h>
#include <stdarg.h>
#include <vector>
static void OLog(const char* fmt, ...) {
    FILE* f = nullptr; fopen_s(&f, "C:\\Users\\User\\trackA\\overlay.log", "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fprintf(f, "\n"); fclose(f);
}

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

template <int N> struct Obf {
    unsigned char b[N];
    constexpr Obf(const char (&s)[N]) : b{} { for (int i = 0; i < N; ++i) b[i] = (unsigned char)(s[i] ^ 0x5A); }
};
constexpr Obf<4>  S_ESP("ESP");
constexpr Obf<4>  S_BOX("box");
constexpr Obf<5>  S_NAME("name");
constexpr Obf<5>  S_DIST("dist");
constexpr Obf<5>  S_TEAM("team");
constexpr Obf<9>  S_MAXD("max dist");
constexpr Obf<4>  S_AIM("aim");
constexpr Obf<8>  S_AFOV("aim fov");
constexpr Obf<9>  S_ASTEP("max step");

static void Dec(const unsigned char* s, int n, char* out) {
    for (int i = 0; i < n - 1; ++i) out[i] = (char)(s[i] ^ 0x5A);
    out[n - 1] = 0;
}
static void DW(const unsigned char* s, wchar_t* out) { int i = 0; for (; s[i]; ++i) out[i] = (wchar_t)(s[i] ^ 0x5A); out[i] = 0; }
static int SLen(const char* s) { int n = 0; while (s[n]) ++n; return n; }
static void U2W(const char* s, wchar_t* w, int cap) { int i = 0; for (; i < cap - 1 && s[i]; ++i) w[i] = (wchar_t)(unsigned char)s[i]; w[i] = 0; }

static constexpr float PI = 3.14159265358979323846f;

// ---- shared state reader -------------------------------------------------
static HANDLE g_map = nullptr;
static SharedState* g_shm = nullptr;

static bool OpenShm() {
    wchar_t name[64]; DW(kShmName, name);
    g_map = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!g_map) return false;
    g_shm = (SharedState*)MapViewOfFile(g_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedState));
    return g_shm != nullptr;
}

static bool Snapshot(SharedState& out) {
    if (!g_shm) return false;
    for (int tries = 0; tries < 4; ++tries) {
        LONG s1 = g_shm->seq;
        if (s1 & 1) { Sleep(1); continue; }
        memcpy(&out, g_shm, sizeof(SharedState));
        LONG s2 = g_shm->seq;
        if (s1 == s2 && !(s2 & 1)) return out.magic == SHM_MAGIC && out.version == SHM_VERSION;
    }
    return false;
}

// ---- d2d -----------------------------------------------------------------
static ID2D1Factory* g_factory = nullptr;
static ID2D1DCRenderTarget* g_rt = nullptr;
static IDWriteFactory* g_dw = nullptr;
static ID2D1SolidColorBrush* g_brush = nullptr;
static IDWriteTextFormat* g_fmt[8] = {};
static float g_fmtSz[8] = {};
static int g_fmtN = 0;
static HDC g_memDC = nullptr;
static HBITMAP g_bmp = nullptr;
static void* g_bits = nullptr;
static int g_w = 0, g_h = 0;

static IDWriteTextFormat* GetFmt(float sz) {
    for (int i = 0; i < g_fmtN; ++i) if (g_fmtSz[i] == sz) return g_fmt[i];
    if (g_fmtN >= 8 || !g_dw) return nullptr;
    IDWriteTextFormat* f = nullptr;
    g_dw->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                           DWRITE_FONT_STRETCH_NORMAL, sz, L"en-us", &f);
    g_fmt[g_fmtN] = f; g_fmtSz[g_fmtN] = sz; ++g_fmtN;
    return f;
}

static void Text(float x, float y, const char* s, D2D1::ColorF col, float sz) {
    if (!g_rt || !g_brush) return;
    IDWriteTextFormat* f = GetFmt(sz);
    if (!f) return;
    wchar_t w[256]; U2W(s, w, 256);
    g_brush->SetColor(col);
    g_rt->DrawText(w, (UINT32)SLen(s), f, D2D1::RectF(x, y, x + 1600.f, y + sz + 6.f), g_brush);
}
static void Rect(float x, float y, float w, float h, D2D1::ColorF col, float t) {
    if (!g_rt || !g_brush) return;
    g_brush->SetColor(col);
    g_rt->DrawRectangle(D2D1::RectF(x, y, x + w, y + h), g_brush, t);
}
static void Fill(float x, float y, float w, float h, D2D1::ColorF col) {
    if (!g_rt || !g_brush) return;
    g_brush->SetColor(col);
    g_rt->FillRectangle(D2D1::RectF(x, y, x + w, y + h), g_brush);
}

static bool InitD2D() {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_factory))) return false;
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.f, 96.f);
    if (FAILED(g_factory->CreateDCRenderTarget(&props, &g_rt))) return false;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&g_dw))) return false;
    g_rt->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &g_brush);
    return true;
}

static bool EnsureSurface(int w, int h) {
    if (w == g_w && h == g_h && g_bmp) return true;
    if (g_bmp) { DeleteObject(g_bmp); g_bmp = nullptr; }
    if (!g_memDC) g_memDC = CreateCompatibleDC(nullptr);
    BITMAPINFO bi{}; bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h; bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    g_bmp = CreateDIBSection(g_memDC, &bi, DIB_RGB_COLORS, &g_bits, nullptr, 0);
    if (!g_bmp) return false;
    SelectObject(g_memDC, g_bmp);
    RECT rc{ 0, 0, w, h };
    if (FAILED(g_rt->BindDC(g_memDC, &rc))) return false;
    g_w = w; g_h = h;
    return true;
}

// ---- w2s -----------------------------------------------------------------
struct V3 { float x, y, z; };
struct RT { float pitch, yaw, roll; };
static bool W2S(const SharedState& st, const V3& wp, float& sx, float& sy, int W, int H) {
    float dx = wp.x - st.camLoc.x, dy = wp.y - st.camLoc.y, dz = wp.z - st.camLoc.z;
    float p = st.camRot.pitch * PI / 180.f, y = st.camRot.yaw * PI / 180.f, r = st.camRot.roll * PI / 180.f;
    float sp = sinf(p), cp = cosf(p), sy2 = sinf(y), cy = cosf(y), sr = sinf(r), cr = cosf(r);
    float fwd = (cp * cy) * dx + (cp * sy2) * dy + sp * dz;
    if (fwd < 1.0f) return false;
    float right = (sr * sp * cy - cr * sy2) * dx + (sr * sp * sy2 + cr * cy) * dy + (-sr * cp) * dz;
    float up = (-(cr * sp * cy + sr * sy2)) * dx + (cy * sr - cr * sp * sy2) * dy + (cr * cp) * dz;
    float th = tanf(st.camFov * PI / 360.f);
    if (th <= 0.001f) return false;
    float scale = (W * 0.5f) / th;
    sx = W * 0.5f + (right * scale) / fwd;
    sy = H * 0.5f - (up * scale) / fwd;
    return true;
}

// ---- ui ------------------------------------------------------------------
static bool g_menu = true, g_esp = true, g_boxes = true, g_names = false, g_dist = false, g_team = false;
static int g_sel = 0;
static bool g_debug = false;
static float g_maxDist = 400.f;
static bool g_aim = false;
static float g_aimFovPx = 160.f, g_aimMaxStep = 0.35f;
static float g_camZ = 0.f, g_camFwd = 0.f, g_fovAdj = 0.f, g_fovAds = 67.f;
static HWND g_gameWnd = nullptr;
static uintptr_t g_aimTarget = 0;
static DWORD g_aimAcquire = 0, g_aimLastSent = 0, g_ctrlChange = 0;
static SRot g_lastCtrl{};
static bool g_aimNoResp = false;

static bool Down(int vk);

static void SetAimCmd(int en, float dy, float dp, float ms) {
    if (!g_shm) return;
    InterlockedIncrement(&g_shm->cmd.seq);
    g_shm->cmd.enable = en;
    g_shm->cmd.dYaw = dy;
    g_shm->cmd.dPitch = dp;
    g_shm->cmd.maxStep = ms;
    g_shm->cmd.stamp = GetTickCount();
    InterlockedIncrement(&g_shm->cmd.seq);
}

static void UpdateAim(const SharedState& st, int W, int H) {
    bool active = g_aim && !g_menu && Down(VK_RBUTTON) && st.state == 1 && st.camValid &&
                  g_gameWnd && GetForegroundWindow() == g_gameWnd;
    if (active) {
        if (memcmp(&st.controlRot, &g_lastCtrl, sizeof(SRot)) != 0) {
            g_lastCtrl = st.controlRot; g_ctrlChange = GetTickCount(); g_aimNoResp = false;
        } else if (GetTickCount() - g_ctrlChange > 600) {
            g_aimNoResp = true;
        }
    }
    if (!active) {
        g_aimTarget = 0;
        if (GetTickCount() - g_aimLastSent > 40) SetAimCmd(0, 0, 0, 0);
        return;
    }
    const SPlayer* best = nullptr;
    float bestD = g_aimFovPx;
    for (int i = 0; i < st.playerCount; ++i) {
        const SPlayer& p = st.players[i];
        if (p.dist > g_maxDist) continue;
        if (g_team && st.localCamp >= 0 && p.camp == st.localCamp) continue;
        float sx, sy;
        V3 head{ p.pos.x, p.pos.y, p.pos.z + 65.f };
        if (!W2S(st, head, sx, sy, W, H)) continue;
        if (sx < 0 || sy < 0 || sx >= W || sy >= H) continue;
        float dx = sx - W * 0.5f, dy = sy - H * 0.5f;
        float d = sqrtf(dx * dx + dy * dy);
        if (d < bestD) { bestD = d; best = &p; }
    }
    if (!best) { g_aimTarget = 0; SetAimCmd(0, 0, 0, 0); return; }
    if (best->pawn != g_aimTarget) {
        g_aimTarget = best->pawn;
        g_aimAcquire = GetTickCount() + 120 + (GetTickCount() % 120);
        SetAimCmd(0, 0, 0, 0);
        return;
    }
    if (GetTickCount() < g_aimAcquire) { SetAimCmd(0, 0, 0, 0); return; }
    float dx = best->pos.x - st.camLoc.x;
    float dy = best->pos.y - st.camLoc.y;
    float dz = (best->pos.z + 65.f) - st.camLoc.z;
    float d2 = sqrtf(dx * dx + dy * dy);
    if (d2 < 1.f) { SetAimCmd(0, 0, 0, 0); return; }
    float yaw = atan2f(dy, dx) * 180.f / PI;
    float pitch = atan2f(dz, d2) * 180.f / PI;
    float dyaw = yaw - st.controlRot.yaw;
    while (dyaw > 180.f) dyaw -= 360.f;
    while (dyaw < -180.f) dyaw += 360.f;
    float dpitch = pitch - st.controlRot.pitch;
    SetAimCmd(1, dyaw, dpitch, g_aimMaxStep);
    g_aimLastSent = GetTickCount();
}

static bool Down(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }
static bool Pressed(int vk) {
    static bool prev[256] = {};
    bool now = Down(vk); bool p = now && !prev[vk & 0xFF]; prev[vk & 0xFF] = now; return p;
}

static void DrawMenu() {
    char esp[8], box[8], nm[8], dist[8], team[8], maxd[16], aim[8], afov[16], astep[16];
    Dec(S_ESP.b, 4, esp); Dec(S_BOX.b, 4, box); Dec(S_NAME.b, 5, nm); Dec(S_DIST.b, 5, dist);
    Dec(S_TEAM.b, 5, team); Dec(S_MAXD.b, 9, maxd); Dec(S_AIM.b, 8, aim);
    Dec(S_AFOV.b, 8, afov); Dec(S_ASTEP.b, 9, astep);
    struct It { int kind; void* p; float mn, mx, st; char* lbl; };
    It it[9] = {
        {0,&g_esp,0,0,0,esp}, {0,&g_boxes,0,0,0,box}, {0,&g_names,0,0,0,nm},
        {0,&g_dist,0,0,0,dist}, {0,&g_team,0,0,0,team}, {0,&g_aim,0,0,0,aim},
        {1,&g_aimFovPx,20,600,10,afov}, {1,&g_aimMaxStep,0.05f,1.5f,0.05f,astep},
        {1,&g_maxDist,50,1000,25,maxd} };
    const int n = 9;
    if (g_menu) {
        if (Pressed(VK_DOWN)) g_sel = (g_sel + 1) % n;
        if (Pressed(VK_UP)) g_sel = (g_sel + n - 1) % n;
        if (Pressed(VK_LEFT) || Pressed(VK_RIGHT)) {
            It& i = it[g_sel];
            if (i.kind == 0) *(bool*)i.p = !*(bool*)i.p;
            else {
                float& v = *(float*)i.p; v += Pressed(VK_RIGHT) ? i.st : -i.st;
                if (v < i.mn) v = i.mn; if (v > i.mx) v = i.mx;
            }
        }
        float x = 30.f, y = 40.f, w = 270.f, row = 20.f;
        Fill(x, y, w, 52.f + n * row, D2D1::ColorF(0.03f, 0.03f, 0.05f, 0.82f));
        Rect(x, y, w, 52.f + n * row, D2D1::ColorF(0.2f, 0.8f, 1.f, 0.9f), 1.f);
        Text(x + 10.f, y + 6.f, "1.14.0.772", D2D1::ColorF(0.3f, 0.9f, 1.f), 14.f);
        for (int i = 0; i < n; ++i) {
            float yy = y + 30.f + i * row;
            if (i == g_sel) Fill(x + 5.f, yy - 2.f, w - 10.f, row - 2.f, D2D1::ColorF(0.15f, 0.35f, 0.55f, 0.65f));
            char line[64];
            if (it[i].kind == 0) wsprintfA(line, "%s %s", it[i].lbl, *(bool*)it[i].p ? "1" : "0");
            else if (i == 7) wsprintfA(line, "%s %d.%02d", it[i].lbl, (int)*(float*)it[i].p, (int)(*(float*)it[i].p * 100) % 100);
            else wsprintfA(line, "%s %d", it[i].lbl, (int)*(float*)it[i].p);
            Text(x + 12.f, yy, line, D2D1::ColorF(0.85f, 0.85f, 0.85f), 12.f);
        }
    }
}

static void DrawDebug(const SharedState& st, int W, int H) {
    if (!g_debug) return;
    float cx = W * 0.5f, cy = H * 0.5f;
    Rect(cx - 8.f, cy - 1.f, 16.f, 2.f, D2D1::ColorF(0.2f, 1.f, 0.3f, 0.9f), 1.f);
    Rect(cx - 1.f, cy - 8.f, 2.f, 16.f, D2D1::ColorF(0.2f, 1.f, 0.3f, 0.9f), 1.f);
    char b[192];
    wsprintfA(b, "win %dx%d cam%d p%d a%d d%d", W, H, st.camMode, st.playerCount, st.actorCount, st.dbgCount);
    Text(10.f, H - 54.f, b, D2D1::ColorF(0.3f, 1.f, 0.4f), 12.f);
    wsprintfA(b, "cam %.0f %.0f %.0f fov %.0f", st.camLoc.x, st.camLoc.y, st.camLoc.z, st.camFov);
    Text(10.f, H - 40.f, b, D2D1::ColorF(0.3f, 1.f, 0.4f), 12.f);
    wsprintfA(b, "self %.0f %.0f %.0f rot %.0f %.0f", st.selfPos.x, st.selfPos.y, st.selfPos.z,
              st.controlRot.pitch, st.controlRot.yaw);
    Text(10.f, H - 26.f, b, D2D1::ColorF(0.3f, 1.f, 0.4f), 12.f);
    for (int i = 0; i < st.dbgCount && i < 40; ++i) {
        V3 p{ st.dbg[i].pos.x, st.dbg[i].pos.y, st.dbg[i].pos.z };
        float sx, sy;
        if (!W2S(st, p, sx, sy, W, H)) continue;
        if (sx < 0 || sy < 0 || sx >= W || sy >= H) continue;
        Fill(sx - 2.f, sy - 2.f, 4.f, 4.f, D2D1::ColorF(1.f, 0.9f, 0.2f, 0.95f));
        Text(sx + 4.f, sy - 6.f, st.dbg[i].cls, D2D1::ColorF(1.f, 0.9f, 0.2f), 9.f);
    }
}

static void CaptureBmp() {
    int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    HDC dc = GetDC(nullptr); HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, w, h); HGDIOBJ old = SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, w, h, dc, 0, 0, SRCCOPY);
    BITMAPINFOHEADER bi{}; bi.biSize = sizeof(bi); bi.biWidth = w; bi.biHeight = h; bi.biPlanes = 1; bi.biBitCount = 24; bi.biCompression = BI_RGB;
    DWORD row = ((w * 3 + 3) & ~3); DWORD sz = row * h;
    std::vector<BYTE> px(sz);
    GetDIBits(mem, bmp, 0, h, px.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    BITMAPFILEHEADER fh{}; fh.bfType = 0x4D42; fh.bfOffBits = sizeof(fh) + sizeof(bi); fh.bfSize = fh.bfOffBits + sz;
    FILE* f = nullptr; fopen_s(&f, "C:\\Users\\User\\trackA\\screen.bmp", "wb");
    if (f) { fwrite(&fh, sizeof(fh), 1, f); fwrite(&bi, sizeof(bi), 1, f); fwrite(px.data(), 1, sz, f); fclose(f); OLog("shot %dx%d", w, h); }
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); ReleaseDC(nullptr, dc);
}

static void LogState(const SharedState& st, int W, int H) {
    float fx = 0, fy = 0, hx = 0, hy = 0; bool ok = false;
    if (st.playerCount > 0) {
        V3 feet{ st.players[0].pos.x, st.players[0].pos.y, st.players[0].pos.z };
        V3 head{ feet.x, feet.y, feet.z + 72.f };
        bool a = W2S(st, feet, fx, fy, W, H); bool b = W2S(st, head, hx, hy, W, H); ok = a && b;
    }
    OLog("STATE W=%d H=%d st=%d cam=%d mode=%d fov=%.2f loc=%.1f,%.1f,%.1f rot=%.2f,%.2f,%.2f ctrl=%.2f,%.2f self=%.1f,%.1f,%.1f P=%d A=%d lc=%d lt=%d dead=%d",
        W, H, st.state, st.camValid, st.camMode, st.camFov, st.camLoc.x, st.camLoc.y, st.camLoc.z,
        st.camRot.pitch, st.camRot.yaw, st.camRot.roll, st.controlRot.pitch, st.controlRot.yaw,
        st.selfPos.x, st.selfPos.y, st.selfPos.z, st.playerCount, st.actorCount, st.localCamp, st.localTeam, st.selfDead);
    if (st.playerCount > 0) {
        OLog("P0 name=%s camp=%d bot=%d pos=%.1f,%.1f,%.1f dist=%.1f w2s=%d feet=%.1f,%.1f head=%.1f,%.1f",
            st.players[0].name, st.players[0].camp, st.players[0].bot, st.players[0].pos.x, st.players[0].pos.y, st.players[0].pos.z,
            st.players[0].dist, ok ? 1 : 0, fx, fy, hx, hy);
    }
}

static void DrawFrame(SharedState st, int W, int H) {
    {
        float p = st.camRot.pitch * 3.14159265f / 180.f, y = st.camRot.yaw * 3.14159265f / 180.f;
        float fx = cosf(p) * cosf(y), fy = cosf(p) * sinf(y), fz = sinf(p);
        st.camLoc.x += fx * g_camFwd; st.camLoc.y += fy * g_camFwd; st.camLoc.z += fz * g_camFwd + g_camZ;
        st.camFov = Down(VK_RBUTTON) ? g_fovAds : (st.camFov + g_fovAdj);
    }
    UpdateAim(st, W, H);
    DrawDebug(st, W, H);
    if (st.state == 1 && st.camValid && !st.selfDead) {
        for (int i = 0; i < st.playerCount; ++i) {
            const SPlayer& p = st.players[i];
            if (p.dist > g_maxDist) continue;
            if (g_team && ((st.localTeam >= 0 && p.team == st.localTeam) || (st.localCamp >= 0 && p.camp == st.localCamp))) continue;
            float fx, fy, hx, hy;
            V3 feet{ p.pos.x, p.pos.y, p.pos.z };
            V3 head{ p.pos.x, p.pos.y, p.pos.z + 72.f };
            if (!W2S(st, feet, fx, fy, W, H)) continue;
            if (!W2S(st, head, hx, hy, W, H)) continue;
            if (!g_esp) continue;
            float h = fy > hy ? fy - hy : hy - fy;
            if (h < 4.f || h > H) continue;
            float bw = h * 0.45f;
            D2D1::ColorF col = p.bot ? D2D1::ColorF(0.6f, 0.6f, 0.6f) : D2D1::ColorF(1.f, 0.25f, 0.25f);
            if (g_boxes) Rect(fx - bw * 0.5f, hy, bw, h, col, 1.2f);
            if (g_names) Text(fx - bw * 0.5f, hy - 15.f, p.name, col, 12.f);
            if (g_dist) { char b[32]; wsprintfA(b, "%dm", (int)p.dist); Text(fx - bw * 0.5f, hy - 1.f, b, D2D1::ColorF(0.9f, 0.9f, 0.6f), 11.f); }
        }
    }
    if (g_menu) {
        char s[64];
        wsprintfA(s, "W%d P%d A%d C%d", st.state, st.playerCount, st.actorCount, st.camValid);
        Text(30.f, 14.f, s, D2D1::ColorF(0.7f, 0.7f, 0.7f), 11.f);
        if (g_aim && g_aimNoResp) Text(30.f, 28.f, "AIM NO-RESP", D2D1::ColorF(1.f, 0.3f, 0.3f), 12.f);
        DrawMenu();
    }
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    OLog("start");
    { HDESK d = OpenInputDesktop(0, FALSE, GENERIC_READ); wchar_t dn[64]={0}; DWORD n=0;
      if (d) { GetUserObjectInformationW(d, UOI_NAME, dn, sizeof(dn), &n); CloseDesktop(d); }
      OLog("desktop=%ls err=%lu", dn, GetLastError()); }
    SetProcessDPIAware();
    if (!InitD2D()) { OLog("d2d fail err=%lu", GetLastError()); return 1; }
    OLog("d2d ok");
    wchar_t cls[32]; DW(kOverlayCls, cls);
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = hInst;
    wc.lpszClassName = cls;
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        cls, L"", WS_POPUP, 0, 0, 800, 600, nullptr, nullptr, hInst, nullptr);
    if (!hwnd) { OLog("hwnd fail err=%lu", GetLastError()); return 2; }
    OLog("hwnd ok");
    BOOL hk1 = RegisterHotKey(hwnd, 1, MOD_NOREPEAT, VK_INSERT);
    DWORD e1 = hk1 ? 0 : GetLastError();
    BOOL hk2 = RegisterHotKey(hwnd, 2, MOD_NOREPEAT, VK_F8);
    DWORD e2 = hk2 ? 0 : GetLastError();
    BOOL hk3 = RegisterHotKey(hwnd, 3, MOD_NOREPEAT, VK_HOME);
    DWORD e3 = hk3 ? 0 : GetLastError();
    OLog("hotkey insert=%d err=%lu f8=%d err=%lu home=%d err=%lu", hk1 ? 1 : 0, e1, hk2 ? 1 : 0, e2, hk3 ? 1 : 0, e3);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);

    wchar_t gameCls[32]; DW(kGameWndName, gameCls);
    HWND game = nullptr;
    DWORD lastFind = 0;
    SharedState st{};

    for (;;) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return 0;
            if (msg.message == WM_HOTKEY) {
                if (msg.wParam == 1) { g_menu = !g_menu; OLog("HK INSERT menu=%d", g_menu ? 1 : 0); }
                else if (msg.wParam == 2) { g_debug = !g_debug; OLog("HK F8 debug=%d", g_debug ? 1 : 0); }
                else if (msg.wParam == 3) { g_menu = !g_menu; OLog("HK HOME menu=%d", g_menu ? 1 : 0); }
            }
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        static bool pIns = false, pF8 = false;
        bool ins = Down(VK_INSERT); if (ins && !pIns) { g_menu = !g_menu; OLog("INSERT menu=%d", g_menu ? 1 : 0); } pIns = ins;
        bool f8 = Down(VK_F8); if (f8 && !pF8) { g_debug = !g_debug; OLog("F8 debug=%d", g_debug ? 1 : 0); } pF8 = f8;
        {
            if (Down(VK_UP)) g_camZ += 1.f;
            if (Down(VK_DOWN)) g_camZ -= 1.f;
            if (Down(VK_PRIOR)) g_camFwd += 8.f;
            if (Down(VK_NEXT)) g_camFwd -= 8.f;
            if (Down(VK_LEFT)) g_fovAdj -= 0.5f;
            if (Down(VK_RIGHT)) g_fovAdj += 0.5f;
        }
        static bool kInit = false; static LONG lastK = 0;
        if (g_shm) {
            if (!kInit) { lastK = g_shm->keySeq; kInit = true; }
            LONG k1 = g_shm->keySeq;
            if (k1 != lastK) {
                int bits = g_shm->keyEdges; LONG k2 = g_shm->keySeq;
                if (k1 == k2) { lastK = k1;
                    if (bits & 1) { g_menu = !g_menu; OLog("INGAME INSERT menu=%d", g_menu ? 1 : 0); }
                    if (bits & 2) { g_debug = !g_debug; OLog("INGAME F8 debug=%d", g_debug ? 1 : 0); CaptureBmp(); }
                    if (bits & 4) { g_menu = !g_menu; OLog("INGAME HOME menu=%d", g_menu ? 1 : 0); }
                }
            }
        }

        DWORD now = GetTickCount();
        if (now - lastFind > 500) {
            lastFind = now;
            game = FindWindowW(gameCls, nullptr);
            if (game && !IsWindow(game)) game = nullptr;
            g_gameWnd = game;
            if (!g_shm) { bool r = OpenShm(); OLog("shm=%d err=%lu game=%p", r ? 1 : 0, GetLastError(), game); }
        }
        int x = 0, y = 0, W = 800, H = 600;
        if (game) {
            RECT rc; GetClientRect(game, &rc);
            POINT p0{ 0, 0 }; ClientToScreen(game, &p0);
            x = p0.x; y = p0.y; W = rc.right - rc.left; H = rc.bottom - rc.top;
            SetWindowPos(hwnd, HWND_TOPMOST, x, y, W, H, SWP_NOACTIVATE | SWP_NOREDRAW);
        }
        if (W < 16 || H < 16) { Sleep(50); continue; }
        if (!EnsureSurface(W, H)) { Sleep(50); continue; }
        { static DWORD lastCfg = 0; DWORD t = GetTickCount();
          if (t - lastCfg > 500) { lastCfg = t;
            FILE* cf = nullptr; fopen_s(&cf, "C:\\Users\\User\\trackA\\camcfg.txt", "r");
            if (cf) { float a=0,b=0,c=0,d=67.f; if (fscanf_s(cf, "%f %f %f %f", &a, &b, &c, &d) >= 3) { g_camZ=a; g_camFwd=b; g_fovAdj=c; g_fovAds=d; } fclose(cf); }
          } }
        bool ok = Snapshot(st);
        if (!ok) { st.state = 0; st.playerCount = 0; st.actorCount = 0; st.camValid = 0; }
        { static DWORD lastLog = 0; DWORD t = GetTickCount();
          if (g_shm && t - lastLog > 2000) { lastLog = t; LogState(st, W, H); } }

        g_rt->BeginDraw();
        g_rt->SetTransform(D2D1::Matrix3x2F::Identity());
        g_rt->Clear(D2D1::ColorF(0, 0.f));
        DrawFrame(st, W, H);
        g_rt->EndDraw();

        HDC screen = GetDC(nullptr);
        POINT dst{ x, y };
        SIZE sz{ W, H };
        POINT src{ 0, 0 };
        BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        UpdateLayeredWindow(hwnd, screen, &dst, &sz, g_memDC, &src, 0, &bf, ULW_ALPHA);
        ReleaseDC(nullptr, screen);
        Sleep(16);
    }
}
