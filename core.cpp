// dfcore: read-only game state publisher (no window, no D3D, no Present hook)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include "shared.h"

static uintptr_t g_base = 0;
static volatile uintptr_t g_pcShared = 0;
static uintptr_t g_blocks = 0;
static SharedState* g_shm = nullptr;

template <int N> struct Obf {
    unsigned char b[N];
    constexpr Obf(const char (&s)[N]) : b{} { for (int i = 0; i < N; ++i) b[i] = (unsigned char)(s[i] ^ 0x5A); }
};
constexpr Obf<6>  S_WORLD("World");
constexpr Obf<6>  S_LEVEL("Level");
constexpr Obf<10> S_CHAR("Character");
constexpr Obf<5>  S_PAWN("Pawn");

static void Dec(const unsigned char* s, int n, char* out) {
    for (int i = 0; i < n - 1; ++i) out[i] = (char)(s[i] ^ 0x5A);
    out[n - 1] = 0;
}
static void DW(const unsigned char* s, wchar_t* out) {
    int i = 0; for (; s[i]; ++i) out[i] = (wchar_t)(s[i] ^ 0x5A);
    out[i] = 0;
}

static int SLen(const char* s) { int n = 0; while (s[n]) ++n; return n; }
static void SCopy(char* d, int cap, const char* s) { int i = 0; for (; i < cap - 1 && s[i]; ++i) d[i] = s[i]; d[i] = 0; }
static bool SContains(const char* h, const char* n) {
    if (!*n) return true;
    for (; *h; ++h) { const char* a = h; const char* b = n; while (*a && *b && *a == *b) { ++a; ++b; } if (!*b) return true; }
    return false;
}
static void MMove(void* d, const void* s, size_t n) { unsigned char* dd = (unsigned char*)d; const unsigned char* ss = (const unsigned char*)s; for (size_t i = 0; i < n; ++i) dd[i] = ss[i]; }
static void MZero(void* d, size_t n) { unsigned char* p = (unsigned char*)d; for (size_t i = 0; i < n; ++i) p[i] = 0; }

static bool ReadMem(void* dst, uintptr_t src, size_t n) {
    __try { MMove(dst, (const void*)src, n); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { MZero(dst, n); return false; }
}
template <class T> static T R(uintptr_t a) { T v{}; ReadMem(&v, a, sizeof(T)); return v; }
static bool WriteMem(void* dst, const void* src, size_t n) {
    DWORD old = 0;
    if (!VirtualProtect(dst, n, PAGE_READWRITE, &old)) return false;
    __try { MMove(dst, src, n); }
    __except (EXCEPTION_EXECUTE_HANDLER) { VirtualProtect(dst, n, old, &old); return false; }
    VirtualProtect(dst, n, old, &old);
    return true;
}

static bool ValidPtr(uintptr_t p) { return p > 0x10000 && p < 0x7FFFFFFFFFFFULL; }
static uintptr_t DPtr(uint64_t raw) { return (uintptr_t)(raw & 0xFFFFFFFFFFFFULL); }
static bool FiniteF(float v) { return v == v && v > -1e18f && v < 1e18f; }
static bool PosOK(const SVec3& p) {
    return FiniteF(p.x) && FiniteF(p.y) && FiniteF(p.z) &&
           fabsf(p.x) < 400000.f && fabsf(p.y) < 400000.f && fabsf(p.z) < 20000.f &&
           (p.x * p.x + p.y * p.y) > 1.0f;
}

namespace off {
    constexpr uint64_t GWORLD = 0x1D9E4E78;
    constexpr uint64_t GWORLD_ENC = 0x1D9E6E78;
    constexpr uint64_t GNAMES_BLOCKS = 0x1E5A7788;
    constexpr uint32_t CLASSPRIVATE = 0x08;
    constexpr uint32_t NAMEPRIVATE = 0x1C;
    constexpr uint32_t PERSISTENT_LEVEL = 0xF8;
    constexpr uint32_t OWNING_GI = 0x190;
    constexpr uint32_t LEVEL_ACTORS = 0x98;
    constexpr uint32_t LOCALPLAYERS = 0x38;
    constexpr uint32_t LOCAL_PC = 0x30;
    constexpr uint32_t ACK_PAWN = 0x3F0;
    constexpr uint32_t CAM_MGR = 0x408;
    constexpr uint32_t ROOT_COMP = 0x180;
    constexpr uint32_t REL_LOCATION = 0x168;
    constexpr uint32_t COMPONENT_TW = 0x210;
    constexpr uint32_t PLAYERSTATE = 0x390;
    constexpr uint32_t PLAYERNAME = 0x478;
    constexpr uint32_t CAMP = 0x670;
    constexpr uint32_t TEAMID = 0x66C;
    constexpr uint32_t IS_BOT = 0x39E;
    constexpr uint32_t CAM_CACHE = 0x30870;
    constexpr uint32_t CONTROL_ROT = 0x3D8;
    constexpr uint32_t ROTATION_INPUT = 0x570;
}


static const unsigned char kGameExeName[] = { 0x1E,0x3F,0x36,0x2E,0x3B,0x1C,0x35,0x28,0x39,0x3F,0x19,0x36,0x33,0x3F,0x34,0x2E,0x77,0x0D,0x33,0x34,0x6C,0x6E,0x77,0x09,0x32,0x33,0x2A,0x2A,0x33,0x34,0x3D,0x74,0x3F,0x22,0x3F,0x00 };
static void DecW(const unsigned char* s, wchar_t* out) {
    int i = 0; for (; s[i]; ++i) out[i] = (wchar_t)(s[i] ^ 0x5A); out[i] = 0;
}
static char g_worldTag[16], g_levelTag[16], g_charTag[16], g_pawnTag[16];

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

struct NameEnt { uint32_t idx; char name[104]; };
static NameEnt g_names[2048];

static void FNameToBuf(uint32_t idx, char* out, int cap) {
    NameEnt& e = g_names[idx & 2047];
    if (e.idx == idx && e.name[0]) { SCopy(out, cap, e.name); return; }
    SCopy(out, cap, "?");
    if (!g_blocks) return;
    uintptr_t blk = R<uintptr_t>(g_blocks + (uint64_t)(idx >> 18) * 8);
    if (!ValidPtr(blk)) return;
    uintptr_t ent = blk + (uint64_t)(idx & 0x3FFFF) * 2;
    uint16_t hdr = R<uint16_t>(ent);
    uint32_t len = hdr >> 6;
    if (!len || len > 1023) return;
    if (hdr & 1) {
        wchar_t w[128]; uint32_t n = len < 120 ? len : 120;
        if (ReadMem(w, ent + 2, (size_t)n * 2)) {
            w[n] = 0;
            if (WideCharToMultiByte(CP_UTF8, 0, w, -1, out, cap, nullptr, nullptr) <= 0) SCopy(out, cap, "?");
        }
    } else {
        char s[128]; uint32_t n = len < 120 ? len : 120;
        if (ReadMem(s, ent + 2, n)) {
            uint8_t k = XorKey(len);
            for (uint32_t i = 0; i < n; ++i) s[i] = (char)((uint8_t)s[i] ^ k);
            s[n] = 0; SCopy(out, cap, s);
        }
    }
    SCopy(e.name, 104, out); e.idx = idx;
}

static uintptr_t ObjClass(uintptr_t o) { return ValidPtr(o) ? R<uintptr_t>(o + off::CLASSPRIVATE) : 0; }
static void ClassNameToBuf(uintptr_t o, char* out, int cap) {
    uintptr_t c = ObjClass(o);
    if (!ValidPtr(c)) { SCopy(out, cap, "?"); return; }
    FNameToBuf(R<uint32_t>(c + off::NAMEPRIVATE), out, cap);
}
static bool NameIs(uintptr_t o, const char* want) {
    char buf[104]; ClassNameToBuf(o, buf, 104);
    const char* a = buf; const char* b = want;
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == 0 && *b == 0;
}
static uintptr_t DecryptGWorldMirror(uint64_t raw) {
    uint8_t b[8]; MMove(b, &raw, 8);
    uint8_t b0 = b[0], b7 = b[7];
    b[0] = (uint8_t)(b7 ^ 0x36); b[7] = (uint8_t)(b0 ^ 0x36);
    uint64_t v; MMove(&v, b, 8); return (uintptr_t)v;
}
static uintptr_t GetWorld() {
    uint64_t plain = R<uint64_t>(g_base + off::GWORLD);
    if (ValidPtr((uintptr_t)plain) && NameIs((uintptr_t)plain, g_worldTag)) return (uintptr_t)plain;
    uintptr_t dec = DecryptGWorldMirror(R<uint64_t>(g_base + off::GWORLD_ENC));
    if (ValidPtr(dec) && NameIs(dec, g_worldTag)) return dec;
    return 0;
}

static void ReadFStringToBuf(uintptr_t addr, char* out, int cap) {
    out[0] = 0;
    uintptr_t ptr = DPtr(R<uint64_t>(addr));
    int32_t len = R<int32_t>(addr + 8);
    if (!ValidPtr(ptr) || len <= 0 || len > 128) return;
    wchar_t w[132]; int n = len < 128 ? len : 128;
    if (!ReadMem(w, ptr, (size_t)n * 2)) return;
    w[n] = 0; while (n > 0 && w[n - 1] == 0) w[--n] = 0;
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out, cap, nullptr, nullptr);
}


static void DumpBones(uintptr_t actor, FILE* f);
static void DumpBody(uintptr_t pawn) {
    FILE* f = nullptr; fopen_s(&f, "C:\\Users\\User\\trackA\\body.txt", "a");
    if (!f) return;
    uintptr_t ps = DPtr(R<uint64_t>(pawn + off::PLAYERSTATE));
    int team = ValidPtr(ps) ? R<int32_t>(ps + 0x660) : -1;
    fprintf(f, "\n=== PAWN %p ps=%p team=%d ===\n", (void*)pawn, (void*)ps, team);
    SVec3 ppos{};
    uintptr_t root = DPtr(R<uint64_t>(pawn + off::ROOT_COMP));
    if (ValidPtr(root)) ppos = R<SVec3>(root + off::REL_LOCATION);
    fprintf(f, "root=%p pos=(%.1f,%.1f,%.1f)\n", (void*)root, ppos.x, ppos.y, ppos.z);
    { char pc[104]; ClassNameToBuf(pawn, pc, 104); fprintf(f, "pawnClass=%s\n", pc); }
    DumpBones(pawn, f);
    for (uint32_t o = 0x100; o <= 0xA00; o += 8) {
        uintptr_t p = DPtr(R<uint64_t>(pawn + o));
        if (!ValidPtr(p)) continue;
        char cn[104]; ClassNameToBuf(p, cn, 104);
        if (cn[0] && cn[0] != '?')
            fprintf(f, "obj off=0x%X p=%p class=%s\n", o, (void*)p, cn);
        if (!SContains(cn, "SkeletalMesh")) continue;
        fprintf(f, "MESH off=0x%X p=%p class=%s\n", o, (void*)p, cn);
        for (uint32_t a = 0; a + 16 <= 0x1000; a += 8) {
            uintptr_t dp = R<uintptr_t>(p + a);
            int32_t c = R<int32_t>(p + a + 8), m = R<int32_t>(p + a + 12);
            if (!ValidPtr(dp) || c < 20 || c > 700 || m < c || m > 4000) continue;
            float q0 = R<float>(dp), q1 = R<float>(dp + 4), q2 = R<float>(dp + 8), q3 = R<float>(dp + 12);
            float n2 = q0*q0 + q1*q1 + q2*q2 + q3*q3;
            float tx = R<float>(dp + 0x10), ty = R<float>(dp + 0x14), tz = R<float>(dp + 0x18);
            bool tok = (tx==tx) && (ty==ty) && (tz==tz) && fabsf(tx)<6000 && fabsf(ty)<6000 && fabsf(tz)<6000;
            if (n2 > 0.75f && n2 < 1.25f && tok)
                fprintf(f, "  ARR32 off=0x%X ptr=%p cnt=%d max=%d q=(%.3f,%.3f,%.3f,%.3f) t=(%.1f,%.1f,%.1f)\n",
                        a, (void*)dp, c, m, q0, q1, q2, q3, tx, ty, tz);
        }
        for (uint32_t a = 0; a + 0x30 <= 0x1000; a += 4) {
            float x = R<float>(p + a), y = R<float>(p + a + 4), z = R<float>(p + a + 8);
            if (!((x==x)&&(y==y)&&(z==z))) continue;
            float dx = x - ppos.x, dy = y - ppos.y, dz = z - ppos.z;
            if (dx*dx + dy*dy + dz*dz < 9.0e4f) {
                float q0 = R<float>(p + a - 0x10), q1 = R<float>(p + a - 0x0C), q2 = R<float>(p + a - 8), q3 = R<float>(p + a - 4);
                float n2 = q0*q0 + q1*q1 + q2*q2 + q3*q3;
                if (n2 > 0.75f && n2 < 1.25f)
                    fprintf(f, "  CTW? off=0x%X t=(%.1f,%.1f,%.1f) q=(%.3f,%.3f,%.3f,%.3f)\n", a - 0x10, x, y, z, q0, q1, q2, q3);
            }
        }
    }
    for (uint32_t o = 0x100; o <= 0xA00; o += 8) {
        uintptr_t dp = R<uintptr_t>(pawn + o);
        int32_t c = R<int32_t>(pawn + o + 8), m = R<int32_t>(pawn + o + 12);
        if (!ValidPtr(dp) || c < 1 || c > 400 || m < c || m > 2000) continue;
        for (int32_t j = 0; j < c && j < 80; ++j) {
            uintptr_t e = R<uintptr_t>(dp + (uint64_t)j * 8);
            if (!ValidPtr(e)) continue;
            char en[104]; ClassNameToBuf(e, en, 104);
            if (SContains(en, "SkeletalMesh"))
                fprintf(f, "ARRCOMP base=0x%X cnt=%d j=%d e=%p class=%s\n", o, c, j, (void*)e, en);
        }
    }
    fclose(f);
}


static void DumpMeshArrays(uintptr_t p, const char* tag, FILE* f) {
    fprintf(f, "MESH %s p=%p\n", tag, (void*)p);
    for (uint32_t o = 0; o <= 0x2000; o += 8) {
        uintptr_t q = R<uintptr_t>(p + o);
        if (!ValidPtr(q)) continue;
        char qn[104]; ClassNameToBuf(q, qn, 104);
        if (qn[0] && qn[0] != '?') fprintf(f, "  UOBJ off=0x%X p=%p class=%s\n", o, (void*)q, qn);
    }
    for (uint32_t a = 0; a + 16 <= 0x2000; a += 8) {
        uintptr_t dp = R<uintptr_t>(p + a);
        int32_t c = R<int32_t>(p + a + 8), m = R<int32_t>(p + a + 12);
        if (!ValidPtr(dp) || c < 1 || c > 4000 || m < c || m > 20000) continue;
        fprintf(f, "  TARR off=0x%X ptr=%p cnt=%d max=%d\n", a, (void*)dp, c, m);
    }
    for (int k = 0; k < 2; ++k) {
        uint32_t bo = k ? 0x0A08 : 0x09F8;
        uintptr_t bp = R<uintptr_t>(p + bo);
        int32_t bc = R<int32_t>(p + bo + 8), bm = R<int32_t>(p + bo + 12);
        fprintf(f, "  BONEOFF off=0x%X ptr=%p cnt=%d max=%d\n", bo, (void*)bp, bc, bm);
        if (ValidPtr(bp) && bc > 0 && bc < 2000) {
            for (int i = 0; i < 4 && i < bc; ++i) {
                float x = R<float>(bp + (uint64_t)i*0x30 + 0x10), y = R<float>(bp + (uint64_t)i*0x30 + 0x14), z = R<float>(bp + (uint64_t)i*0x30 + 0x18);
                float q0 = R<float>(bp + (uint64_t)i*0x30), q1 = R<float>(bp + (uint64_t)i*0x30 + 4), q2 = R<float>(bp + (uint64_t)i*0x30 + 8), q3 = R<float>(bp + (uint64_t)i*0x30 + 12);
                fprintf(f, "    b%d t=(%.2f,%.2f,%.2f) q=(%.2f,%.2f,%.2f,%.2f)\n", i, x, y, z, q0, q1, q2, q3);
            }
        }
    }
    int best = 0; uint32_t bestOff = 0; uintptr_t bestPtr = 0;
    for (uint32_t a = 0; a + 16 <= 0x1400; a += 8) {
        uintptr_t dp = R<uintptr_t>(p + a);
        int32_t c = R<int32_t>(p + a + 8), m = R<int32_t>(p + a + 12);
        if (!ValidPtr(dp) || c < 20 || c > 1200 || m < c || m > 6000) continue;
        float q0 = R<float>(dp), q1 = R<float>(dp + 4), q2 = R<float>(dp + 8), q3 = R<float>(dp + 12);
        float n2 = q0*q0 + q1*q1 + q2*q2 + q3*q3;
        float tx = R<float>(dp + 0x10), ty = R<float>(dp + 0x14), tz = R<float>(dp + 0x18);
        bool tok = (tx==tx) && (ty==ty) && (tz==tz) && fabsf(tx)<6000 && fabsf(ty)<6000 && fabsf(tz)<6000;
        if (n2 > 0.75f && n2 < 1.25f && tok) {
            fprintf(f, "  ARR off=0x%X ptr=%p cnt=%d max=%d q0=(%.3f,%.3f,%.3f,%.3f) t0=(%.1f,%.1f,%.1f)\n",
                    a, (void*)dp, c, m, q0, q1, q2, q3, tx, ty, tz);
            if (c > best) { best = c; bestOff = a; bestPtr = dp; }
        }
    }
    if (bestPtr) {
        fprintf(f, "  BEST off=0x%X ptr=%p cnt=%d\n", bestOff, (void*)bestPtr, best);
        for (int i = 0; i < 10; ++i) {
            float q0 = R<float>(bestPtr + (uint64_t)i*0x30), q1 = R<float>(bestPtr + (uint64_t)i*0x30 + 4);
            float q2 = R<float>(bestPtr + (uint64_t)i*0x30 + 8), q3 = R<float>(bestPtr + (uint64_t)i*0x30 + 12);
            float x = R<float>(bestPtr + (uint64_t)i*0x30 + 0x10), y = R<float>(bestPtr + (uint64_t)i*0x30 + 0x14), z = R<float>(bestPtr + (uint64_t)i*0x30 + 0x18);
            fprintf(f, "    B%d t=(%.1f,%.1f,%.1f) q=(%.3f,%.3f,%.3f,%.3f)\n", i, x, y, z, q0, q1, q2, q3);
        }
    }
}

static void DumpBones(uintptr_t actor, FILE* f);
static void DumpActorsMeshes(uintptr_t arr, int32_t cnt) {
    FILE* f = nullptr; fopen_s(&f, "C:\\Users\\User\\trackA\\body.txt", "a");
    if (!f) return;
    int dumped = 0;
    for (int32_t i = 0; i < cnt && dumped < 60; ++i) {
        uintptr_t a = R<uintptr_t>(arr + (uint64_t)i * 8);
        if (!ValidPtr(a)) continue;
        uintptr_t root = DPtr(R<uint64_t>(a + off::ROOT_COMP));
        SVec3 pos{}; if (ValidPtr(root)) pos = R<SVec3>(root + off::REL_LOCATION);
        char anc[104]; ClassNameToBuf(a, anc, 104);
        if (anc[0] && anc[0] != '?')
            fprintf(f, "ACT %p pos=(%.1f,%.1f,%.1f) class=%s\n", (void*)a, pos.x, pos.y, pos.z, anc);
        uintptr_t m3 = DPtr(R<uint64_t>(a + 0x3D0));
        if (ValidPtr(m3)) {
            char mn[104]; ClassNameToBuf(m3, mn, 104);
            fprintf(f, "  M3D0 %p class=%s\n", (void*)m3, mn);
            DumpBones(a, f);
        } else if (anc[0] && anc[0] != '?' && SContains(anc, "Character")) DumpBones(a, f);
        for (uint32_t o = 0x100; o <= 0xA00; o += 8) {
            uintptr_t p = DPtr(R<uint64_t>(a + o));
            if (!ValidPtr(p)) continue;
            char cn[104]; ClassNameToBuf(p, cn, 104);
            if (!SContains(cn, "SkeletalMesh")) continue;
            fprintf(f, "ACTOR %p pos=(%.1f,%.1f,%.1f) compOff=0x%X class=%s\n", (void*)a, pos.x, pos.y, pos.z, o, cn);
            DumpMeshArrays(p, cn, f);
            ++dumped; break;
        }
    }
    fprintf(f, "ACTORS_DUMPED=%d\n", dumped);
    fclose(f);
}


static int g_objStride = 0x18;
static void DumpObjectsFromArray(uintptr_t chunks, int num, FILE* f) {
    int found = 0, checked = 0;
    for (uint32_t i = 0; i < (uint32_t)num && found < 40 && checked < 400000; ++i, ++checked) {
        uintptr_t chunk = R<uintptr_t>(chunks + (uint64_t)(i >> 16) * 8);
        if (!ValidPtr(chunk)) continue;
        uintptr_t obj = R<uintptr_t>(chunk + (uint64_t)(i & 0xFFFF) * (uint64_t)g_objStride);
        if (!ValidPtr(obj)) continue;
        char cn[104]; ClassNameToBuf(obj, cn, 104);
        if (!SContains(cn, "SkeletalMesh")) continue;
        uintptr_t outer = R<uintptr_t>(obj + 0x20);
        char on[104]; ClassNameToBuf(outer, on, 104);
        fprintf(f, "GOBJ i=%u obj=%p class=%s outer=%s\n", i, (void*)obj, cn, on);
        DumpMeshArrays(obj, cn, f);
        ++found;
    }
    fprintf(f, "GOBJ_FOUND=%d checked=%d\n", found, checked);
}

static bool TryGoLayout(uintptr_t A, int layout, FILE* f, uintptr_t& chunksOut, int& numOut) {
    uintptr_t chunks = (layout == 2) ? R<uintptr_t>(A + 0x20) : R<uintptr_t>(A + 0x10);
    int maxE, numE, numC;
    if (layout == 0) { maxE = R<int32_t>(A + 0x20); numE = R<int32_t>(A + 0x24); numC = R<int32_t>(A + 0x2C); }
    else if (layout == 1) { maxE = R<int32_t>(A + 0x18); numE = R<int32_t>(A + 0x1C); numC = R<int32_t>(A + 0x24); }
    else { maxE = R<int32_t>(A + 0x10); numE = R<int32_t>(A + 0x14); numC = R<int32_t>(A + 0x1C); }
    if (!ValidPtr(chunks)) return false;
    if (numE < 10000 || numE > 10000000 || maxE < numE) return false;
    if (numC <= 0 || numC > 1024) return false;
    int expect = (maxE + 65535) / 65536;
    if (numC != expect && numC < expect) return false;
    uintptr_t c0 = R<uintptr_t>(chunks);
    if (!ValidPtr(c0)) return false;
    uintptr_t o0 = R<uintptr_t>(c0);
    if (!ValidPtr(o0)) return false;
    char cn[104]; ClassNameToBuf(o0, cn, 104);
    if (!cn[0] || cn[0] == '?') {
        fprintf(f, "GO near A=%p L%d numE=%d maxE=%d numC=%d class0=%s\n", (void*)A, layout, numE, maxE, numC, cn);
        return false;
    }
    fprintf(f, "GO HIT A=%p L%d numE=%d maxE=%d numC=%d class0=%s\n", (void*)A, layout, numE, maxE, numC, cn);
    chunksOut = chunks; numOut = numE;
    return true;
}

static void FindGObjectsAndDump() {
    FILE* f = nullptr; fopen_s(&f, "C:\\Users\\User\\trackA\\body.txt", "a");
    if (!f) return;
    auto* dos = (IMAGE_DOS_HEADER*)g_base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) { fclose(f); return; }
    auto* nt = (IMAGE_NT_HEADERS*)((uint8_t*)g_base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) { fclose(f); return; }
    auto* sec = IMAGE_FIRST_SECTION(nt);
    int hits = 0;
    for (unsigned si = 0; si < nt->FileHeader.NumberOfSections && hits < 2; ++si) {
        auto& sh = sec[si];
        if (!(sh.Characteristics & IMAGE_SCN_MEM_WRITE)) continue;
        uintptr_t start = g_base + sh.VirtualAddress;
        uintptr_t end = start + sh.Misc.VirtualSize;
        for (uintptr_t A = start; A + 0x40 < end; A += 8) {
            for (int L = 0; L < 3; ++L) {
                uintptr_t chunks; int num;
                if (TryGoLayout(A, L, f, chunks, num)) {
                    for (int st = 0; st < 2; ++st) {
                        g_objStride = st ? 0x20 : 0x18;
                        fprintf(f, "TRY stride=0x%X\n", g_objStride);
                        for (int k = 0; k < 5; ++k) {
                            uintptr_t ch = R<uintptr_t>(chunks + (uint64_t)(k >> 16) * 8);
                            uintptr_t ob = R<uintptr_t>(ch + (uint64_t)(k & 0xFFFF) * (uint64_t)g_objStride);
                            char kn[104]; ClassNameToBuf(ob, kn, 104);
                            fprintf(f, "  obj[%d] p=%p class=%s\n", k, (void*)ob, kn);
                        }
                    }
                    g_objStride = 0x18;
                    DumpObjectsFromArray(chunks, num, f);
                    ++hits;
                    break;
                }
            }
            if (hits >= 2) break;
        }
    }
    if (!hits) fprintf(f, "GOBJECTS not found (full writable scan)\n");
    fclose(f);
}



static void DumpBones(uintptr_t actor, FILE* f) {
    uintptr_t mesh = R<uintptr_t>(actor + 0x3D0);
    if (!ValidPtr(mesh)) return;
    char mc[104]; ClassNameToBuf(mesh, mc, 104);
    uintptr_t root = DPtr(R<uint64_t>(actor + off::ROOT_COMP));
    SVec3 ppos{}; if (ValidPtr(root)) ppos = R<SVec3>(root + off::REL_LOCATION);
    fprintf(f, "CHAR %p mesh=%p class=%s pos=(%.1f,%.1f,%.1f)\n", (void*)actor, (void*)mesh, mc, ppos.x, ppos.y, ppos.z);
    {
        float cq0=R<float>(mesh+0x210), cq1=R<float>(mesh+0x214), cq2=R<float>(mesh+0x218), cq3=R<float>(mesh+0x21C);
        float ct0=R<float>(mesh+0x220), ct1=R<float>(mesh+0x224), ct2=R<float>(mesh+0x228);
        fprintf(f, "  CTW210 t=(%.1f,%.1f,%.1f) q=(%.3f,%.3f,%.3f,%.3f)\n", ct0,ct1,ct2,cq0,cq1,cq2,cq3);
        uintptr_t cap = DPtr(R<uint64_t>(actor + 0x108));
        if (ValidPtr(cap)) {
            float kx=R<float>(cap+0x220), ky=R<float>(cap+0x224), kz=R<float>(cap+0x228);
            fprintf(f, "  CAP108 p=%p CTW t=(%.1f,%.1f,%.1f)\n", (void*)cap, kx,ky,kz);
        }
    }
    uint32_t cand[4] = {0xEB8, 0x9F8, 0xA08, 0};
    for (int ci = 0; ci < 3; ++ci) {
        uint32_t off = cand[ci];
        uintptr_t bp = R<uintptr_t>(mesh + off);
        int32_t c = R<int32_t>(mesh + off + 8), m = R<int32_t>(mesh + off + 12);
        fprintf(f, "  BONEARR off=0x%X ptr=%p cnt=%d max=%d\n", off, (void*)bp, c, m);
        if (!ValidPtr(bp) || c <= 0 || c > 1000) continue;
        float minz = 1e9f, maxz = -1e9f;
        int n = c < 10 ? c : 10;
        for (int i = 0; i < c && i < 400; ++i) {
            float z = R<float>(bp + (uint64_t)i*0x30 + 0x18);
            if (z == z) { if (z < minz) minz = z; if (z > maxz) maxz = z; }
        }
        for (int i = 0; i < n; ++i) {
            float q0 = R<float>(bp + (uint64_t)i*0x30), q1 = R<float>(bp + (uint64_t)i*0x30 + 4);
            float q2 = R<float>(bp + (uint64_t)i*0x30 + 8), q3 = R<float>(bp + (uint64_t)i*0x30 + 12);
            float x = R<float>(bp + (uint64_t)i*0x30 + 0x10), y = R<float>(bp + (uint64_t)i*0x30 + 0x14), z = R<float>(bp + (uint64_t)i*0x30 + 0x18);
            fprintf(f, "    B%d t=(%.2f,%.2f,%.2f) q=(%.2f,%.2f,%.2f,%.2f)\n", i, x, y, z, q0, q1, q2, q3);
        }
        fprintf(f, "    ZRANGE %.1f..%.1f\n", minz, maxz);
    }
    for (uint32_t a = 0; a + 0x30 <= 0x2000; a += 4) {
        float x = R<float>(mesh + a), y = R<float>(mesh + a + 4), z = R<float>(mesh + a + 8);
        if (!((x==x)&&(y==y)&&(z==z))) continue;
        float dx = x - ppos.x, dy = y - ppos.y, dz = z - ppos.z;
        if (dx*dx + dy*dy + dz*dz < 9.0e4f) {
            float q0 = R<float>(mesh + a - 0x10), q1 = R<float>(mesh + a - 0x0C), q2 = R<float>(mesh + a - 8), q3 = R<float>(mesh + a - 4);
            float n2 = q0*q0 + q1*q1 + q2*q2 + q3*q3;
            if (n2 > 0.75f && n2 < 1.25f)
                fprintf(f, "  CTW off=0x%X t=(%.1f,%.1f,%.1f) q=(%.3f,%.3f,%.3f,%.3f)\n", a - 0x10, x, y, z, q0, q1, q2, q3);
        }
    }
}

static void DumpWorldLevels(uintptr_t w) {
    FILE* f = nullptr; fopen_s(&f, "C:\\Users\\User\\trackA\\body.txt", "a");
    if (!f) return;
    fprintf(f, "WORLD %p levels scan\n", (void*)w);
    for (uint32_t o = 0; o <= 0x2000; o += 8) {
        uintptr_t dp = R<uintptr_t>(w + o);
        int32_t c = R<int32_t>(w + o + 8), m = R<int32_t>(w + o + 12);
        if (!ValidPtr(dp) || c < 1 || c > 200 || m < c || m > 4000) continue;
        int lv = 0; uintptr_t first = 0;
        for (int32_t j = 0; j < c && j < 64; ++j) {
            uintptr_t e = R<uintptr_t>(dp + (uint64_t)j * 8);
            char en[104]; ClassNameToBuf(e, en, 104);
            if (SContains(en, "Level")) { ++lv; if (!first) first = e; }
        }
        if (!lv) continue;
        fprintf(f, "WORLDARR off=0x%X cnt=%d max=%d levels=%d first=%p\n", o, c, m, lv, (void*)first);
        for (int32_t j = 0; j < c && j < 32; ++j) {
            uintptr_t lvptr = R<uintptr_t>(dp + (uint64_t)j * 8);
            char ln[104]; ClassNameToBuf(lvptr, ln, 104);
            if (!SContains(ln, "Level")) continue;
            uintptr_t arr = R<uintptr_t>(lvptr + off::LEVEL_ACTORS);
            int32_t cnt = R<int32_t>(lvptr + off::LEVEL_ACTORS + 8);
            if (!ValidPtr(arr) || cnt <= 0 || cnt > 50000) continue;
            fprintf(f, "  LEVEL %p actors=%d\n", (void*)lvptr, cnt);
            DumpActorsMeshes(arr, cnt);
        }
    }
    fclose(f);
}


static inline void QRot(float qx,float qy,float qz,float qw, float vx,float vy,float vz, float& ox,float& oy,float& oz) {
    float tx = qy*vz - qz*vy + qw*vx;
    float ty = qz*vx - qx*vz + qw*vy;
    float tz = qx*vy - qy*vx + qw*vz;
    ox = vx + 2.f*(qy*tz - qz*ty);
    oy = vy + 2.f*(qz*tx - qx*tz);
    oz = vz + 2.f*(qx*ty - qy*tx);
}

static void FillBones(SPlayer& p, uintptr_t actor) {
    p.boneCount = 0;
    uintptr_t mesh = DPtr(R<uint64_t>(actor + 0x3D0));
    if (!ValidPtr(mesh)) return;
    uintptr_t bp = R<uintptr_t>(mesh + 0x760);
    int32_t cnt = R<int32_t>(mesh + 0x760 + 8);
    if (!ValidPtr(bp) || cnt < 10 || cnt > 400) return;
    float qx = R<float>(mesh + 0x210), qy = R<float>(mesh + 0x214), qz = R<float>(mesh + 0x218), qw = R<float>(mesh + 0x21C);
    float tx = R<float>(mesh + 0x220), ty = R<float>(mesh + 0x224), tz = R<float>(mesh + 0x228);
    float qn = qx*qx + qy*qy + qz*qz + qw*qw;
    if (!(qn > 0.8f && qn < 1.2f)) return;
    struct BP { float x, y, z; };
    BP pts[400]; int n = 0;
    float minz = 1e9f, maxz = -1e9f;
    for (int i = 0; i < cnt && i < 400; ++i) {
        float lx = R<float>(bp + (uint64_t)i*0x30 + 0x10), ly = R<float>(bp + (uint64_t)i*0x30 + 0x14), lz = R<float>(bp + (uint64_t)i*0x30 + 0x18);
        if (!((lx==lx)&&(ly==ly)&&(lz==lz))) continue;
        float wx, wy, wz;
        QRot(qx,qy,qz,qw, lx,ly,lz, wx,wy,wz);
        pts[n].x = wx + tx; pts[n].y = wy + ty; pts[n].z = wz + tz;
        if (pts[n].z < minz) minz = pts[n].z;
        if (pts[n].z > maxz) maxz = pts[n].z;
        ++n;
    }
    float h = maxz - minz;
    if (n < 10 || h < 50.f || h > 300.f) return;
    auto pickC = [&](float frac) -> int {
        int bi = -1; float bd = 1e9f;
        for (int i = 0; i < n; ++i) { float f = (pts[i].z - minz) / h; float d = fabsf(f - frac); if (d < bd) { bd = d; bi = i; } }
        return bi;
    };
    auto pickSide = [&](float frac, int side) -> int {
        int bi = -1; float bx = side < 0 ? 1e9f : -1e9f;
        for (int i = 0; i < n; ++i) {
            float f = (pts[i].z - minz) / h;
            if (fabsf(f - frac) > 0.09f) continue;
            if (side < 0) { if (pts[i].x < bx) { bx = pts[i].x; bi = i; } }
            else { if (pts[i].x > bx) { bx = pts[i].x; bi = i; } }
        }
        if (bi < 0) bi = pickC(frac);
        return bi;
    };
    int head = pickC(1.0f);
    int neck = pickC(0.90f), chest = pickC(0.75f), pelvis = pickC(0.55f);
    int ls = pickSide(0.82f, -1), rs = pickSide(0.82f, 1);
    int le = pickSide(0.68f, -1), re = pickSide(0.68f, 1);
    int lh = pickSide(0.58f, -1), rh = pickSide(0.58f, 1);
    int lhip = pickSide(0.50f, -1), rhip = pickSide(0.50f, 1);
    int lk = pickSide(0.28f, -1), rk = pickSide(0.28f, 1);
    int lf = pickSide(0.05f, -1), rf = pickSide(0.05f, 1);
    int idx[16] = { head, neck, chest, pelvis, ls, rs, le, re, lh, rh, lhip, rhip, lk, rk, lf, rf };
    for (int i = 0; i < 16; ++i) {
        int j = idx[i]; if (j < 0) j = 0;
        p.bones[i].x = pts[j].x; p.bones[i].y = pts[j].y; p.bones[i].z = pts[j].z;
    }
    p.boneCount = 16;
}

static void Refresh(SharedState& st) {
#ifdef PROBE_ONLY
    { static bool g_goDumped = false; if (!g_goDumped) { g_goDumped = true; FindGObjectsAndDump(); } }
#endif
    MZero(&st, sizeof(st));
    st.magic = SHM_MAGIC; st.version = SHM_VERSION;
    uintptr_t w = GetWorld();
    if (!w) { st.state = 2; return; }
    uintptr_t level = DPtr(R<uint64_t>(w + off::PERSISTENT_LEVEL));
    uintptr_t gi = DPtr(R<uint64_t>(w + off::OWNING_GI));
    if (!ValidPtr(level) || !NameIs(level, g_levelTag)) { st.state = 3; return; }
    if (!ValidPtr(gi)) { st.state = 4; return; }

    st.world = w; st.level = level; st.gi = gi;
#ifdef PROBE_ONLY
    { static bool g_wlDumped = false; if (!g_wlDumped) { g_wlDumped = true; DumpWorldLevels(w); } }
#endif
    uintptr_t lpArr = R<uintptr_t>(gi + off::LOCALPLAYERS);
    int32_t lpCnt = R<int32_t>(gi + off::LOCALPLAYERS + 8);
    uintptr_t pc = 0, localPawn = 0;
    if (ValidPtr(lpArr) && lpCnt > 0) {
        uintptr_t lp = DPtr(R<uint64_t>(lpArr));
        if (ValidPtr(lp)) pc = DPtr(R<uint64_t>(lp + off::LOCAL_PC));
    }
    SVec3 camLoc{}; SRot camRot{}; float camFov = 0.f; bool camOK = false; uintptr_t cam = 0;
    int camMode = 0;
    int localCamp = -1;
    if (ValidPtr(pc)) st.controlRot = R<SRot>(pc + off::CONTROL_ROT);
    if (ValidPtr(pc)) {
        localPawn = DPtr(R<uint64_t>(pc + off::ACK_PAWN));
        cam = DPtr(R<uint64_t>(pc + off::CAM_MGR));
        if (ValidPtr(cam)) {
            uintptr_t povs[5] = { cam + 0x2C5E0, cam + off::CAM_CACHE, cam + 0x2DEC0, cam + 0x2DEF0, cam + 0x2F310 };
            for (int ci = 0; ci < 5 && camMode == 0; ++ci) {
                uintptr_t pov = povs[ci];
                SVec3 l = R<SVec3>(pov);
                SRot r = R<SRot>(pov + 0xC);
                float f = R<float>(pov + 0x18);
                if (FiniteF(l.x) && FiniteF(l.y) && FiniteF(l.z) &&
                    (l.x * l.x + l.y * l.y) > 1.0f && f > 30.f && f < 140.f) {
                    st.camLoc = l; st.camRot = r; st.camFov = f; st.camValid = 1; camMode = 1 + ci;
                }
            }
        }
        if (ValidPtr(localPawn)) {
            uintptr_t ps = DPtr(R<uint64_t>(localPawn + off::PLAYERSTATE));
            if (ValidPtr(ps)) { st.localCamp = R<int32_t>(ps + 0x660); st.localTeam = R<int32_t>(ps + 0x66C); st.selfDead = 0; }
            uintptr_t r0 = DPtr(R<uint64_t>(localPawn + off::ROOT_COMP));
            if (ValidPtr(r0)) {
                SVec3 ctw = R<SVec3>(r0 + off::COMPONENT_TW + 0x10);
                SVec3 rel = R<SVec3>(r0 + off::REL_LOCATION);
                SVec3 sp = rel;
                if (PosOK(sp)) {
                    st.selfPos = sp;
                    st.camLoc = SVec3{ sp.x, sp.y, sp.z + 64.f };
                    st.camRot = st.controlRot;
                    st.camFov = 90.f;
                    st.camValid = 1;
                    camMode = 2;
                }
            }
        }
    }
    // auto-scan CameraManager for the live POV (rotation closest to ControlRotation)
    static DWORD g_povOff = 0;
    static DWORD g_lastScan = 0;
    if (camMode == 0 && ValidPtr(cam) && ValidPtr(pc)) {
        SRot ctrlRot = R<SRot>(pc + off::CONTROL_ROT);
        auto tryOff = [&](DWORD off, float& scoreOut) -> bool {
            SVec3 l = R<SVec3>(cam + off);
            SRot r = R<SRot>(cam + off + 0xC);
            float fv = R<float>(cam + off + 0x18);
            if (!FiniteF(l.x) || !FiniteF(l.y) || !FiniteF(l.z)) return false;
            if (!(fv > 20.f && fv < 160.f)) return false;
            if (!(fabsf(r.pitch) <= 90.f && fabsf(r.yaw) <= 360.f)) return false;
            if (!(fabsf(l.x) > 1000.f || fabsf(l.y) > 1000.f)) return false;
            scoreOut = 0.f;
            return true;
        };
        float score = 0.f;
        bool found = false;
        if (g_povOff && tryOff(g_povOff, score)) found = true;
        if (!found) {
            DWORD now = GetTickCount();
            if (now - g_lastScan > 1000) {
                g_lastScan = now;
                float best = 1e9f; DWORD bestOff = 0;
                for (DWORD off = 0; off + 0x28 <= 0x40000; off += 4) {
                    float sc = 0.f;
                    if (tryOff(off, sc) && sc < best) { best = sc; bestOff = off; }
                }
                if (bestOff) { g_povOff = bestOff; found = true; }
            }
        }
        if (found) {
            SVec3 l = R<SVec3>(cam + g_povOff);
            SRot r = R<SRot>(cam + g_povOff + 0xC);
            float fv = R<float>(cam + g_povOff + 0x18);
            st.camLoc = l; st.camRot = r; st.camFov = fv; st.camValid = 1; camMode = 4;
        }
    }
    st.pc = pc; st.localPawn = localPawn;
    st.camMode = camMode;
#ifdef PROBE_ONLY
    { static bool g_bodyDumped = false;
      if (!g_bodyDumped && ValidPtr(localPawn)) { g_bodyDumped = true; DumpBody(localPawn); } }
#endif

    g_pcShared = pc;

    uintptr_t arr = R<uintptr_t>(level + off::LEVEL_ACTORS);
    int32_t cnt = R<int32_t>(level + off::LEVEL_ACTORS + 8);
    if (cnt < 0 || cnt > 200000) cnt = 0;
    st.actorCount = cnt;
#ifdef PROBE_ONLY
    { static bool g_actorsDumped = false;
      if (!g_actorsDumped && cnt > 0) { g_actorsDumped = true; DumpActorsMeshes(arr, cnt); } }
#endif

    struct Stab { uintptr_t pawn; SVec3 pos; int seen; };
    static Stab stab[256] = {};
    int n = 0;
    for (int32_t i = 0; i < cnt && ValidPtr(arr); ++i) {
        uintptr_t a = R<uintptr_t>(arr + (uint64_t)i * 8);
        if (!ValidPtr(a)) continue;
        char cn[104]; ClassNameToBuf(a, cn, 104);

        uintptr_t root = DPtr(R<uint64_t>(a + off::ROOT_COMP));
        SVec3 pos{};
        bool posOK = false;
        if (ValidPtr(root)) {
            SVec3 ctw = R<SVec3>(root + off::COMPONENT_TW + 0x10);
            SVec3 rel = R<SVec3>(root + off::REL_LOCATION);
            pos = rel;
            posOK = PosOK(pos);
        }
        if (st.dbgCount < 64 && posOK) {
            SDbgActor& d = st.dbg[st.dbgCount++];
            SCopy(d.cls, 40, cn);
            d.pos = pos;
            if (st.camValid) {
                float dx = pos.x - st.camLoc.x, dy = pos.y - st.camLoc.y, dz = pos.z - st.camLoc.z;
                d.dist = (float)sqrtf(dx * dx + dy * dy + dz * dz) * 0.01f;
            }
        }

        bool isTarget = SContains(cn, "RangeTarget") || SContains(cn, "TargetDummy") || SContains(cn, "Dummy");
        if (!SContains(cn, g_charTag) && !SContains(cn, g_pawnTag) && !isTarget) continue;
        if (SContains(cn, "Health") || SContains(cn, "PropertyReplication") || SContains(cn, "Redeploy") ||
            SContains(cn, "MapRandom") || SContains(cn, "Pickup") || SContains(cn, "Item") || SContains(cn, "Default__")) continue;
        if (a == localPawn) continue;
        if (!posOK) {
            uintptr_t m3 = DPtr(R<uint64_t>(a + 0x3D0));
            if (ValidPtr(m3)) {
                float mx = R<float>(m3 + 0x220), my = R<float>(m3 + 0x224), mz = R<float>(m3 + 0x228);
                if (FiniteF(mx) && FiniteF(my) && FiniteF(mz)) {
                    pos = SVec3{ mx, my, mz };
                    posOK = PosOK(pos);
                }
            }
        }
        if (n >= 128 || !posOK) continue;
        {
            Stab* slot = nullptr;
            for (int si = 0; si < 256; ++si) { if (stab[si].seen && stab[si].pawn == a) { slot = &stab[si]; break; } if (!stab[si].seen) { slot = &stab[si]; break; } }
            if (slot) {
                if (slot->seen) {
                    float ddx = pos.x - slot->pos.x, ddy = pos.y - slot->pos.y, ddz = pos.z - slot->pos.z;
                    if ((ddx*ddx + ddy*ddy + ddz*ddz) > 9.0e6f) continue; // >30m jump: junk
                }
                slot->pawn = a; slot->pos = pos; slot->seen = 1;
            }
        }
        uintptr_t ps = DPtr(R<uint64_t>(a + off::PLAYERSTATE));
        if (!ValidPtr(ps) && !isTarget) continue;
        SPlayer& p = st.players[n];
        p.pawn = a; p.ps = ps;
        if (ValidPtr(ps)) {
            ReadFStringToBuf(ps + off::PLAYERNAME, p.name, 64);
            if (!p.name[0]) ReadFStringToBuf(ps + 0x488, p.name, 64);
            if (SContains(p.name, "BP_") || SContains(p.name, "PropertyReplication") || SContains(p.name, "Default__")) p.name[0] = 0;
            p.camp = R<int32_t>(ps + 0x660);   // match team id (0 = local team)
            p.team = R<int32_t>(ps + 0x66C);   // squad slot 1..4
            p.bot = 0;
        } else {
            SCopy(p.name, 64, "Target");
            p.camp = -1; p.team = 0; p.bot = 0;
        }
        p.pos = pos;
        FillBones(p, a);
        if (st.camValid) {
            float dx = pos.x - st.camLoc.x, dy = pos.y - st.camLoc.y, dz = pos.z - st.camLoc.z;
            p.dist = (float)sqrtf(dx * dx + dy * dy + dz * dz) * 0.01f;
        }
        if (!p.name[0]) SCopy(p.name, 64, cn);
        ++n;
    }
    {
        uintptr_t lvArr = R<uintptr_t>(w + 0x158);
        int32_t lvCnt = R<int32_t>(w + 0x160);
        if (ValidPtr(lvArr) && lvCnt > 0 && lvCnt < 256) {
            for (int32_t li = 0; li < lvCnt && n < 128; ++li) {
                uintptr_t lv = R<uintptr_t>(lvArr + (uint64_t)li * 8);
                if (!ValidPtr(lv)) continue;
                uintptr_t a2 = R<uintptr_t>(lv + off::LEVEL_ACTORS);
                int32_t c2 = R<int32_t>(lv + off::LEVEL_ACTORS + 8);
                if (!ValidPtr(a2) || c2 <= 0 || c2 > 50000) continue;
                for (int32_t j = 0; j < c2 && n < 128; ++j) {
                    uintptr_t a = R<uintptr_t>(a2 + (uint64_t)j * 8);
                    if (!ValidPtr(a)) continue;
                    char cn[104]; ClassNameToBuf(a, cn, 104);
                    if (!SContains(cn, "RangeTarget") && !SContains(cn, "TargetDummy") && !SContains(cn, "Dummy")) continue;
                    bool dup = false;
                    for (int k = 0; k < n; ++k) if (st.players[k].pawn == a) { dup = true; break; }
                    if (dup) continue;
                    uintptr_t m3 = DPtr(R<uint64_t>(a + 0x3D0));
                    if (!ValidPtr(m3)) continue;
                    float mx = R<float>(m3 + 0x220), my = R<float>(m3 + 0x224), mz = R<float>(m3 + 0x228);
                    SVec3 pos{ mx, my, mz };
                    if (!PosOK(pos)) continue;
                    SPlayer& p = st.players[n];
                    p.pawn = a; p.ps = 0;
                    SCopy(p.name, 64, "Target");
                    p.camp = -1; p.team = 0; p.bot = 0;
                    p.pos = pos;
                    FillBones(p, a);
                    if (st.camValid) {
                        float dx = pos.x - st.camLoc.x, dy = pos.y - st.camLoc.y, dz = pos.z - st.camLoc.z;
                        p.dist = (float)sqrtf(dx*dx + dy*dy + dz*dz) * 0.01f;
                    }
                    ++n;
                }
            }
        }
    }
    st.playerCount = n;
    st.state = 1;
}

static void Publish(const SharedState& st) {
    if (!g_shm) return;
    InterlockedIncrement(&g_shm->seq);
    MMove((char*)g_shm + offsetof(SharedState, state), &st.state,
          offsetof(SharedState, cmd) - offsetof(SharedState, state));
    InterlockedIncrement(&g_shm->seq);
}


typedef unsigned char(NTAPI* TpMayRunLong_t)(void*);
static TpMayRunLong_t g_mayRunLong = nullptr;

static void PollKeys() {
    static bool pi = false, pf = false, ph = false;
    bool i = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    bool f = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
    bool h = (GetAsyncKeyState(VK_HOME) & 0x8000) != 0;
    int bits = 0;
    if (i && !pi) bits |= 1;
    if (f && !pf) bits |= 2;
    if (h && !ph) bits |= 4;
    pi = i; pf = f; ph = h;
    if (bits && g_shm) { g_shm->keyEdges = bits; InterlockedIncrement(&g_shm->keySeq); }
}

static void AimLoop() {
    uint32_t seed = GetTickCount() | 1u;
    for (;;) {
        PollKeys();
        uintptr_t pc = (uintptr_t)g_pcShared;
        if (pc && g_shm) {
            LONG s1 = g_shm->cmd.seq;
            if (!(s1 & 1)) {
                int32_t en = g_shm->cmd.enable;
                float dY = g_shm->cmd.dYaw, dP = g_shm->cmd.dPitch;
                float ms = g_shm->cmd.maxStep;
                uint32_t ts = g_shm->cmd.stamp;
                LONG s2 = g_shm->cmd.seq;
                if (s1 == s2 && en && (GetTickCount() - ts) < 150) {
                    float cur[3];
                    if (ReadMem(cur, pc + off::ROTATION_INPUT, sizeof(cur)) &&
                        FiniteF(cur[0]) && FiniteF(cur[1]) && FiniteF(cur[2]) &&
                        fabsf(cur[0]) < 10000.f && fabsf(cur[1]) < 10000.f && fabsf(cur[2]) < 10000.f) {
                        if (!(ms > 0.05f)) ms = 0.35f;
                        if (ms > 2.0f) ms = 2.0f;
                        float dy = dY * 0.5f, dp = dP * 0.5f;
                        if (dy > ms) dy = ms; if (dy < -ms) dy = -ms;
                        if (dp > ms) dp = ms; if (dp < -ms) dp = -ms;
                        seed = seed * 1664525u + 1013904223u;
                        float j = ((float)((int)((seed >> 16) & 0x1FF) - 256)) * 0.0002f;
                        SRot r{ dp + j * 0.4f, dy + j, 0.f };
                        WriteMem((void*)(pc + off::ROTATION_INPUT), &r, sizeof(r));
                    }
                }
            }
        }
        Sleep(8);
    }
}

static VOID CALLBACK AimCb(void* inst, void*, void*) {
    if (g_mayRunLong) g_mayRunLong(inst);
    AimLoop();
}

typedef HRESULT(NTAPI* TpAllocWork_t)(void**, void*, void*, void*);
typedef void(NTAPI* TpPostWork_t)(void*);


static VOID CALLBACK Worker(void* inst, void*, void*) {
    if (g_mayRunLong) g_mayRunLong(inst);
    char w[16], l[16], c[16], p[16];
    Dec(S_WORLD.b, 6, w); Dec(S_LEVEL.b, 6, l); Dec(S_CHAR.b, 10, c); Dec(S_PAWN.b, 5, p);
    SCopy(g_worldTag, 16, w); SCopy(g_levelTag, 16, l); SCopy(g_charTag, 16, c); SCopy(g_pawnTag, 16, p);

    wchar_t exeName[64]; DecW(kGameExeName, exeName);
    HMODULE h = GetModuleHandleW(exeName);
    if (!h) h = GetModuleHandleW(nullptr);
    g_base = (uintptr_t)h;
    g_blocks = g_base + off::GNAMES_BLOCKS;

    wchar_t shmName[64]; DW(kShmName, shmName);
    HANDLE map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(SharedState), shmName);
    if (map) g_shm = (SharedState*)MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedState));
    if (!g_shm) return;
    g_shm->magic = SHM_MAGIC;
    g_shm->version = SHM_VERSION;

    SharedState st;
#ifdef PROBE_ONLY
    Refresh(st);
    return;
#else
    for (;;) {
        Refresh(st);
        Publish(st);
        Sleep(33);
    }
#endif
}

static DWORD WINAPI ThreadFallback(LPVOID) { Worker(nullptr, nullptr, nullptr); return 0; }

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        HMODULE nt = GetModuleHandleW(L"ntdll.dll");
        TpAllocWork_t alloc = nt ? (TpAllocWork_t)GetProcAddress(nt, "TpAllocWork") : nullptr;
        TpPostWork_t post = nt ? (TpPostWork_t)GetProcAddress(nt, "TpPostWork") : nullptr;
        g_mayRunLong = nt ? (TpMayRunLong_t)GetProcAddress(nt, "TpCallbackMayRunLong") : nullptr;
        void* work = nullptr;
        if (alloc && post && !alloc(&work, (void*)&Worker, nullptr, nullptr) && work) {
            post(work);
        } else {
            CreateThread(nullptr, 0, ThreadFallback, nullptr, 0, nullptr);
        }
        void* work2 = nullptr;
        if (alloc && post && !alloc(&work2, (void*)&AimCb, nullptr, nullptr) && work2) {
            post(work2);
        }
    }
    return TRUE;
}
