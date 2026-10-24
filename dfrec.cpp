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
#include "Offsets.hpp"

static uintptr_t g_base = 0, g_blocks = 0;
static FILE* g_log = nullptr;

static void L(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap; va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap);
    fputc('\n', g_log); fflush(g_log);
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

static DWORD WINAPI Worker(LPVOID) {
    g_log = nullptr;
    fopen_s(&g_log, "rec.log", "w");
    if (!g_log) fopen_s(&g_log, "C:\\Users\\User\\trackA\\rec.log", "w");
    if (!g_log) return 0;

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
            float dt = (now - prevT) / 1000.f;
            bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

            if (lmb && !lmbPrev) { ++shot; L("SHOT %d ctrl=(%.3f,%.3f)", shot, ctrl.pitch, ctrl.yaw); }
            if (lmb) {
                float dp = ctrl.pitch - prevCtrl.pitch, dy = ctrl.yaw - prevCtrl.yaw;
                while (dy > 180.f) dy -= 360.f; while (dy < -180.f) dy += 360.f;
                L("R dt=%.4f ctrl=(%.3f,%.3f) d=(%.3f,%.3f) in=(%.3f,%.3f) lmb=1",
                  dt, ctrl.pitch, ctrl.yaw, dp, dy, in.pitch, in.yaw);
            }
            prevCtrl = ctrl; prevIn = in; prevT = now; lmbPrev = lmb;

            if (ValidPtr(pawn) && pawn != lastPawn && dumped < 6) {
                lastPawn = pawn; ++dumped;
                L("# pawn=%p pc=%p", (void*)pawn, (void*)pc);
                DumpCandidates(pawn);
            }
        }
        Sleep(8);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
    }
    return TRUE;
}
