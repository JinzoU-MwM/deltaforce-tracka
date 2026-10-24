// dfrec — probe recoil/weapon (READ-ONLY). docs/ANTI-RECOIL-RESEARCH §5.
// Tujuan: menemukan offset CurrentWeapon/WeaponData/Recoil/Spread + merekam
// delta ControlRotation per shot. Tidak menulis apa pun ke proses game.
//
// Log: rec.log  (di folder proses pemanggil / C:\Users\User\trackA bila ada)
// Format: R tick dt ctrlP ctrlY rotP rotY lmb | SHOT n dP dY | CAND off class
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <string.h>
#include "Offsets.hpp"

static uintptr_t g_base = 0, g_blocks = 0;

// Log ditulis per-baris dengan CreateFileA + FILE_SHARE_READ/WRITE supaya
// file tetap bisa dibaca saat probe berjalan (fopen "w" mengunci eksklusif).
static void L(const char* fmt, ...) {
    char buf[512];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int)sizeof(buf) - 2) n = (int)sizeof(buf) - 2;
    buf[n++] = '\r'; buf[n++] = '\n';
    // fallback nama: instance lama bisa masih mengunci rec.log (tidak bisa unload)
    static const char* paths[] = {
        "C:\\Users\\User\\trackA\\rec.log",
        "C:\\Users\\User\\trackA\\rec2.log",
        "C:\\Users\\User\\trackA\\rec3.log",
        "C:\\Users\\User\\trackA\\rec4.log",
    };
    HANDLE f = INVALID_HANDLE_VALUE;
    for (int i = 0; i < 4; ++i) {
        f = CreateFileA(paths[i], GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE) break;
    }
    if (f == INVALID_HANDLE_VALUE) return;
    SetFilePointer(f, 0, nullptr, FILE_END);
    DWORD w = 0; WriteFile(f, buf, (DWORD)n, &w, nullptr);
    CloseHandle(f);
}

static void MMove(void* d, const void* s, size_t n) { unsigned char* dd=(unsigned char*)d; const unsigned char* ss=(const unsigned char*)s; for (size_t i=0;i<n;++i) dd[i]=ss[i]; }
static bool ValidPtr(uintptr_t p) { return p > 0x10000 && p < 0x7FFFFFFFFFFFULL; }
static uintptr_t DPtr(uint64_t raw) { return (uintptr_t)(raw & DF::PtrTagMask); }
template<class T> static T R(uintptr_t a) { T v{}; if (ValidPtr(a)) __try { MMove(&v, (void*)a, sizeof(T)); } __except (EXCEPTION_EXECUTE_HANDLER) { v = T{}; } return v; }
static bool ReadMem(void* d, uintptr_t s, size_t n) { if (!ValidPtr(s)) return false; __try { MMove(d, (void*)s, n); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }

// ---- FName (pool UE5 + XOR per-panjang, sama seperti core) ----
static uint8_t XorKey(size_t len) {
    uint32_t r9d = (uint32_t)len, rem = r9d % 9; uint8_t b = (uint8_t)r9d, k = 0;
    switch (rem) {
    case 0: k = (b & 0x1F) + 0x80; k = (uint8_t)(k + b); k |= 0x7F; break;
    case 1: k = (uint8_t)(b ^ 0xDF); k = (uint8_t)(k + 0x80); k = (uint8_t)(k + b); k |= 0x7F; break;
    case 2: k = (uint8_t)(b | 0xCF); k = (uint8_t)(k + 0x80); k = (uint8_t)(k + b); k |= 0x7F; break;
    case 3: { int8_t s = (int8_t)b; k = (uint8_t)(s * 0x21); k = (uint8_t)(k + 0x80); k |= 0x7F; break; }
    case 4: k = (uint8_t)(b >> 2); k = (uint8_t)(k + 0x80); k = (uint8_t)(k + b); k |= 0x7F; break;
    case 5: { uint8_t t = (uint8_t)(b - 0x29); k = (uint8_t)(t + t); k = (uint8_t)(k + t); k |= 0x7F; break; }
    case 6: k = (uint8_t)((b << 2) | 0x05); k = (uint8_t)(k + 0x80); k = (uint8_t)(k + b); k |= 0x7F; break;
    case 7: k = (uint8_t)((b >> 4) | 0x07); k = (uint8_t)(k + 0x80); k = (uint8_t)(k + b); k |= 0x7F; break;
    default: k = (uint8_t)(b ^ 0x40); k = (uint8_t)(k + 0x80); k = (uint8_t)(k + b); k |= 0x7F; break;
    }
    return k;
}
static void FName(uint32_t idx, char* out, int cap) {
    out[0] = 0;
    if (!g_blocks) return;
    uintptr_t blk = R<uintptr_t>(g_blocks + (uint64_t)(idx >> 18) * 8);
    if (!ValidPtr(blk)) return;
    uintptr_t ent = blk + (uint64_t)(idx & 0x3FFFF) * 2;
    uint16_t hdr = R<uint16_t>(ent);
    uint32_t len = hdr >> 6;
    if (!len || len > 200) return;
    if (hdr & 1) {
        wchar_t w[128]{}; uint32_t n = len < 120 ? len : 120;
        if (ReadMem(w, ent + 2, (size_t)n * 2)) WideCharToMultiByte(CP_UTF8, 0, w, -1, out, cap, nullptr, nullptr);
    } else {
        char s[160]{}; uint32_t n = len < 140 ? len : 140;
        if (ReadMem(s, ent + 2, n)) {
            uint8_t k = XorKey(len);
            for (uint32_t i = 0; i < n; ++i) s[i] = (char)((uint8_t)s[i] ^ k);
            s[n] = 0;
            int i = 0; for (; i < cap - 1 && s[i]; ++i) out[i] = s[i]; out[i] = 0;
        }
    }
}
static void ClassName(uintptr_t obj, char* out, int cap) {
    out[0] = 0;
    uintptr_t cls = R<uintptr_t>(obj + DF::UObject_::ClassPrivate);
    if (!ValidPtr(cls)) return;
    FName(R<uint32_t>(cls + DF::UObject_::NamePrivate), out, cap);
}
static bool StrHas(const char* h, const char* n) {
    for (const char* p = h; *p; ++p) { const char* a = p; const char* b = n; while (*a && *b && *a == *b) { ++a; ++b; } if (!*b) return true; }
    return false;
}

static uintptr_t GetWorld() {
    uint64_t plain = R<uint64_t>(g_base + DF::Globals::GWorld);
    if (ValidPtr((uintptr_t)plain)) { char n[64]; ClassName((uintptr_t)plain, n, 64); if (StrHas(n, "World")) return (uintptr_t)plain; }
    uint64_t raw = R<uint64_t>(g_base + DF::Globals::GWorldMirror);
    uint8_t b[8]; MMove(b, &raw, 8); uint8_t b0 = b[0], b7 = b[7];
    b[0] = (uint8_t)(b7 ^ 0x36); b[7] = (uint8_t)(b0 ^ 0x36);
    uint64_t v; MMove(&v, b, 8);
    uintptr_t dec = (uintptr_t)v;
    if (ValidPtr(dec)) { char n[64]; ClassName(dec, n, 64); if (StrHas(n, "World")) return dec; }
    return 0;
}

struct Rot { float pitch, yaw, roll; };
struct SVec3 { float x, y, z; };

static double NowSec() {
    static LARGE_INTEGER f{}; static bool init = false;
    if (!init) { QueryPerformanceFrequency(&f); init = true; }
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}

static void DumpCandidates(uintptr_t pawn) {
    L("# CAND pawn=%p scan 0x100..0x1B00", (void*)pawn);
    int found = 0;
    for (uint32_t off = 0x100; off <= 0x1B00; off += 8) {
        uintptr_t p = DPtr(R<uint64_t>(pawn + off));
        if (!ValidPtr(p)) continue;
        char cn[96]; ClassName(p, cn, 96);
        if (!cn[0] || cn[0] == '?') continue;
        if (StrHas(cn, "Weapon") || StrHas(cn, "Recoil") || StrHas(cn, "Spread") ||
            StrHas(cn, "Ammo") || StrHas(cn, "Fire") || StrHas(cn, "Inventory")) {
            L("CAND off=0x%X obj=%p class=%s", off, (void*)p, cn);
            ++found;
        }
    }
    L("# CAND total=%d", found);
}

// Bongkar objek weapon: pointer -> kelas, dan float yang masuk rentang statistik.
// Rentang dipilih supaya tidak banjir: recoil 0.05..30, RPM 60..1500, spread 0..60,
// ammo 0..400, dan skala kecil 0.01..5.
static void DumpWeapon(uintptr_t obj, const char* tag) {
    L("# WEAPON %s obj=%p", tag, (void*)obj);
    char cn[96]; ClassName(obj, cn, 96);
    L("#   class=%s", cn);
    int nptr = 0, nflt = 0;
    for (uint32_t off = 0; off <= 0x800; off += 8) {
        uintptr_t p = DPtr(R<uint64_t>(obj + off));
        if (ValidPtr(p)) {
            char pn[96]; ClassName(p, pn, 96);
            if (pn[0] && pn[0] != '?') { L("WPTR off=0x%X obj=%p class=%s", off, (void*)p, pn); ++nptr; }
            else if (pn[0] == '?') { L("WPTR off=0x%X obj=%p class=?", off, (void*)p); ++nptr; }
        }
        if (nptr > 40) break;
    }
    for (uint32_t off = 0; off <= 0x800; off += 4) {
        float f = R<float>(obj + off);
        if (!(f == f)) continue;
        float a = fabsf(f);
        if (a < 0.0001f) continue;
        const char* tagf = nullptr;
        if (a >= 60.f && a <= 1500.f)      tagf = "rpm?";
        else if (a >= 0.05f && a <= 30.f)  tagf = "recoil?";
        else if (a >= 0.0001f && a < 0.05f) tagf = "small?";
        if (tagf) { L("WFLT off=0x%X v=%.4f %s", off, f, tagf); ++nflt; }
        if (nflt > 60) break;
    }
    L("# WEAPON end ptr=%d flt=%d", nptr, nflt);
}

// ---- Watcher perubahan: temukan field yang bereaksi saat menembak ----
// Snapshot int32/float32 0x0..0x600 pada objek weapon, bandingkan tiap tick,
// log HANYA saat LMB ditekan (supaya tidak banjir). Ini yang mengaitkan offset
// dengan event: ammo turun, shot index naik, recoil terakumulasi, dll.
#define WATCH_MAX 0x600
static uint8_t g_snapA[WATCH_MAX];
static uint8_t g_snapB[WATCH_MAX];
static uintptr_t g_snapObj = 0;
static DWORD g_chgT[WATCH_MAX / 4];

static void WatchTick(uintptr_t obj, bool firing) {
    (void)firing;
    if (!ValidPtr(obj)) return;
    if (obj != g_snapObj) {
        g_snapObj = obj;
        ReadMem(g_snapA, obj, WATCH_MAX);
        return;
    }
    bool okAll = true;
    for (uint32_t c = 0; c < WATCH_MAX; c += 0x100)
        if (!ReadMem(g_snapB + c, obj + c, 0x100)) okAll = false;
    if (!okAll) { g_snapObj = 0; return; }
    {
        for (uint32_t off = 0; off + 4 <= WATCH_MAX; off += 4) {
            int32_t ov, nv; float of, nf;
            MMove(&ov, g_snapA + off, 4); MMove(&nv, g_snapB + off, 4);
            MMove(&of, g_snapA + off, 4); MMove(&nf, g_snapB + off, 4);
            if (ov == nv) continue;
            DWORD now = GetTickCount();
            if (now - g_chgT[off / 4] < 400) continue;   // dedupe
            g_chgT[off / 4] = now;
            bool intLike = (ov > -100000 && ov < 100000);
            bool fltLike = (of == of && nf == nf && fabsf(of) < 1e6f && fabsf(nf) < 1e6f);
            if (intLike) L("CHG off=0x%X int %d -> %d", off, ov, nv);
            else if (fltLike) L("CHG off=0x%X flt %.4f -> %.4f", off, of, nf);
        }
    }
    MMove(g_snapA, g_snapB, WATCH_MAX);
}

static DWORD WINAPI Worker(LPVOID) {
    DeleteFileA("C:\\Users\\User\\trackA\\rec.log");

    HMODULE h = GetModuleHandleW(L"DeltaForceClient-Win64-Shipping.exe");
    if (!h) h = GetModuleHandleW(nullptr);
    g_base = (uintptr_t)h;
    g_blocks = g_base + DF::Globals::GNamesBlocks;
    L("# dfrec start base=%p blocks=%p", (void*)g_base, (void*)g_blocks);

    uintptr_t lastPawn = 0;
    Rot prevCtrl{}, prevIn{};
    DWORD prevT = GetTickCount();
    int shot = 0; bool lmbPrev = false; int dumped = 0;

    for (;;) {
        uintptr_t w = GetWorld();
        uintptr_t gi = w ? DPtr(R<uint64_t>(w + DF::UWorld_::OwningGameInstance)) : 0;
        uintptr_t lpArr = gi ? R<uintptr_t>(gi + DF::UGameInstance_::LocalPlayers) : 0;
        uintptr_t lp = ValidPtr(lpArr) ? DPtr(R<uint64_t>(lpArr)) : 0;
        uintptr_t pc = lp ? DPtr(R<uint64_t>(lp + DF::ULocalPlayer_::PlayerController)) : 0;
        uintptr_t pawn = pc ? DPtr(R<uint64_t>(pc + DF::UPlayerController_::AcknowledgedPawn)) : 0;

        if (ValidPtr(pc)) {
            Rot ctrl = R<Rot>(pc + DF::UPlayerController_::ControlRotation);
            Rot in = R<Rot>(pc + DF::UPlayerController_::RotationInput);
            DWORD now = GetTickCount();
            static double prevS = NowSec();
            double nowS2 = NowSec();
            float dt = (float)(nowS2 - prevS);
            prevS = nowS2;
            bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

            if (lmb && !lmbPrev) { ++shot; L("SHOT %d ctrl=(%.3f,%.3f)", shot, ctrl.pitch, ctrl.yaw); }
            static double lastStat = 0;
            static Rot anchor{};
            double nowS = NowSec();
            if (nowS - lastStat >= 0.25) {
                // delta terhadap anchor 250 ms (bukan tick 8 ms) supaya climb recoil terlihat
                float dp = ctrl.pitch - anchor.pitch, dy = ctrl.yaw - anchor.yaw;
                while (dy > 180.f) dy -= 360.f; while (dy < -180.f) dy += 360.f;
                SVec3 sp = ValidPtr(pawn) ? R<SVec3>(DPtr(R<uint64_t>(pawn + DF::AActor_::RootComponent)) + DF::USceneComponent_::RelativeLocation) : SVec3{};
                L("T dt=%.3f ctrl=(%.3f,%.3f) d=(%.3f,%.3f) in=(%.3f,%.3f) lmb=%d self=(%.0f,%.0f,%.0f)",
                  dt, ctrl.pitch, ctrl.yaw, dp, dy, in.pitch, in.yaw, lmb ? 1 : 0, sp.x, sp.y, sp.z);
                anchor = ctrl;
                lastStat = nowS;
            }
            prevCtrl = ctrl; prevIn = in; prevT = now; lmbPrev = lmb;

            uintptr_t w1 = ValidPtr(pawn) ? DPtr(R<uint64_t>(pawn + 0x1788)) : 0;
            uintptr_t w2 = ValidPtr(pawn) ? DPtr(R<uint64_t>(pawn + 0xD30)) : 0;
            WatchTick(w1, lmb);
            if (w2 != w1) WatchTick(w2, lmb);

            if (ValidPtr(pawn) && pawn != lastPawn && dumped < 6) {
                lastPawn = pawn; ++dumped;
                L("# pawn=%p pc=%p", (void*)pawn, (void*)pc);
                DumpCandidates(pawn);
                uintptr_t wp1 = DPtr(R<uint64_t>(pawn + 0xD30));
                uintptr_t wp2 = DPtr(R<uint64_t>(pawn + 0x1788));
                if (ValidPtr(wp1)) DumpWeapon(wp1, "pawn+D30");
                if (ValidPtr(wp2) && wp2 != wp1) DumpWeapon(wp2, "pawn+1788");
            }
        }
        Sleep(8);
    }
    return 0;
}

static void Marker(const char* tag) {
    HANDLE f = CreateFileA("C:\\Users\\User\\trackA\\dfrec_hit.txt", GENERIC_WRITE, FILE_SHARE_READ,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    SetFilePointer(f, 0, nullptr, FILE_END);
    DWORD w = 0; WriteFile(f, tag, (DWORD)strlen(tag), &w, nullptr);
    WriteFile(f, "\r\n", 2, &w, nullptr);
    CloseHandle(f);
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        Marker("attach");
        HANDLE th = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
        Marker(th ? "worker-started" : "worker-fail");
    }
    return TRUE;
}
