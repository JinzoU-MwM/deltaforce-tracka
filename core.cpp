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
static MenuCfg* g_menuCfg = nullptr;

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

// Offset TUNGGAL: didefinisikan di Offsets.hpp (namespace DF). Tidak ada angka di sini.
#include "Offsets.hpp"
namespace off {
    using namespace DF;

    constexpr uint64_t GWORLD         = DF::Globals::GWorld;
    constexpr uint64_t GWORLD_ENC     = DF::Globals::GWorldMirror;
    constexpr uint64_t GNAMES_BLOCKS  = DF::Globals::GNamesBlocks;
    constexpr uint32_t CLASSPRIVATE   = (uint32_t)DF::UObject_::ClassPrivate;
    constexpr uint32_t NAMEPRIVATE    = (uint32_t)DF::UObject_::NamePrivate;
    constexpr uint32_t PERSISTENT_LEVEL = (uint32_t)DF::UWorld_::PersistentLevel;
    constexpr uint32_t OWNING_GI      = (uint32_t)DF::UWorld_::OwningGameInstance;
    constexpr uint32_t LEVEL_ACTORS   = (uint32_t)DF::ULevel_::Actors;
    constexpr uint32_t LOCALPLAYERS   = (uint32_t)DF::UGameInstance_::LocalPlayers;
    constexpr uint32_t LOCAL_PC       = (uint32_t)DF::ULocalPlayer_::PlayerController;
    constexpr uint32_t ACK_PAWN       = (uint32_t)DF::UPlayerController_::AcknowledgedPawn;
    constexpr uint32_t CAM_MGR        = (uint32_t)DF::UPlayerController_::PlayerCameraManager;
    constexpr uint32_t ROOT_COMP      = (uint32_t)DF::AActor_::RootComponent;
    constexpr uint32_t REL_LOCATION   = (uint32_t)DF::USceneComponent_::RelativeLocation;
    constexpr uint32_t COMPONENT_TW   = (uint32_t)DF::USceneComponent_::ComponentToWorld;
    constexpr uint32_t PLAYERSTATE    = (uint32_t)DF::AActor_::PlayerState;
    constexpr uint32_t PLAYERNAME     = (uint32_t)DF::APlayerState_::PlayerName;
    constexpr uint32_t CAMP           = (uint32_t)DF::APlayerState_::CampAlt;
    constexpr uint32_t CAMP_TRUE      = (uint32_t)DF::APlayerState_::Camp;
    constexpr uint32_t TEAMID         = (uint32_t)DF::APlayerState_::TeamId;
    constexpr uint32_t IS_BOT         = (uint32_t)DF::APlayerState_::IsBot;
    constexpr uint32_t CAM_CACHE      = (uint32_t)DF::UCameraManager_::CameraCacheCandidate;
    constexpr uint32_t CONTROL_ROT    = (uint32_t)DF::UPlayerController_::ControlRotation;
    constexpr uint32_t ROTATION_INPUT = (uint32_t)DF::UPlayerController_::RotationInput;
    constexpr uint32_t MESH           = (uint32_t)DF::ACharacter_::Mesh;
    constexpr uint32_t BONES          = (uint32_t)DF::USkeletalMeshComponent_::BoneArray;
    constexpr uint32_t BONE_STRIDE    = (uint32_t)DF::USkeletalMeshComponent_::BoneStride;
    constexpr uint32_t MESH_ASSET     = (uint32_t)DF::USkeletalMeshComponent_::SkeletalMeshAsset;
    constexpr uint32_t SKEL_BONETREE  = (uint32_t)DF::USkeleton_::BoneTree;
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
    // Anti-flap: baca GWorld 3x; kalau gagal semua, pakai pointer valid terakhir.
    static uintptr_t cached = 0;
    for (int t = 0; t < 3; ++t) {
        uint64_t plain = R<uint64_t>(g_base + off::GWORLD);
        if (ValidPtr((uintptr_t)plain) && NameIs((uintptr_t)plain, g_worldTag)) { cached = (uintptr_t)plain; return cached; }
        uintptr_t dec = DecryptGWorldMirror(R<uint64_t>(g_base + off::GWORLD_ENC));
        if (ValidPtr(dec) && NameIs(dec, g_worldTag)) { cached = dec; return cached; }
        if (t < 2) Sleep(0);
    }
    if (cached && ValidPtr(cached) && NameIs(cached, g_worldTag)) return cached;
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
static inline void QRot(float qx,float qy,float qz,float qw, float vx,float vy,float vz, float& ox,float& oy,float& oz);
static void DumpSkeletonFull(uintptr_t pawn, FILE* f);
static void DumpBoneNames(uintptr_t mesh, FILE* f);
static void DumpBody(uintptr_t pawn) {
    FILE* f = nullptr; fopen_s(&f, "C:\\Users\\User\\trackA\\body.txt", "a");
    if (!f) return;
    uintptr_t ps = DPtr(R<uint64_t>(pawn + off::PLAYERSTATE));
    int team = ValidPtr(ps) ? R<int32_t>(ps + off::CAMP_TRUE) : -1;
    fprintf(f, "\n=== PAWN %p ps=%p team=%d ===\n", (void*)pawn, (void*)ps, team);
    SVec3 ppos{};
    uintptr_t root = DPtr(R<uint64_t>(pawn + off::ROOT_COMP));
    if (ValidPtr(root)) ppos = R<SVec3>(root + off::REL_LOCATION);
    fprintf(f, "root=%p pos=(%.1f,%.1f,%.1f)\n", (void*)root, ppos.x, ppos.y, ppos.z);
    { char pc[104]; ClassNameToBuf(pawn, pc, 104); fprintf(f, "pawnClass=%s\n", pc); }
    { static bool g_bn = false; if (!g_bn) { g_bn = true; uintptr_t mm = R<uintptr_t>(pawn + off::MESH); if (ValidPtr(mm)) DumpBoneNames(mm, f); DumpSkeletonFull(pawn, f); } }
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
                float x = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 0x10), y = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 0x14), z = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 0x18);
                float q0 = R<float>(bp + (uint64_t)i*off::BONE_STRIDE), q1 = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 4), q2 = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 8), q3 = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 12);
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
            float q0 = R<float>(bestPtr + (uint64_t)i*off::BONE_STRIDE), q1 = R<float>(bestPtr + (uint64_t)i*off::BONE_STRIDE + 4);
            float q2 = R<float>(bestPtr + (uint64_t)i*off::BONE_STRIDE + 8), q3 = R<float>(bestPtr + (uint64_t)i*off::BONE_STRIDE + 12);
            float x = R<float>(bestPtr + (uint64_t)i*off::BONE_STRIDE + 0x10), y = R<float>(bestPtr + (uint64_t)i*off::BONE_STRIDE + 0x14), z = R<float>(bestPtr + (uint64_t)i*off::BONE_STRIDE + 0x18);
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
        uintptr_t m3 = DPtr(R<uint64_t>(a + off::MESH));
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




static void DumpBoneNames(uintptr_t mesh, FILE* f) {
    uintptr_t asset = R<uintptr_t>(mesh + off::MESH_ASSET);
    if (!ValidPtr(asset)) { fprintf(f, "BONENAMES asset invalid\n"); return; }
    uintptr_t skel = R<uintptr_t>(asset + 0x60);
    if (!ValidPtr(skel)) { fprintf(f, "BONENAMES skel invalid asset=%p\n", (void*)asset); return; }
    uintptr_t bt = R<uintptr_t>(skel + off::SKEL_BONETREE);
    int32_t cnt = R<int32_t>(skel + 0x40);
    fprintf(f, "BONENAMES asset=%p skel=%p tree=%p cnt=%d\n", (void*)asset, (void*)skel, (void*)bt, cnt);
    if (!ValidPtr(bt) || cnt <= 0 || cnt > 500) return;
    for (int i = 0; i < 32; ++i) {
        uint32_t v = R<uint32_t>(bt + (uint64_t)i * 4);
        fprintf(f, "  RAWBT %d 0x%08X\n", i, v);
    }
    for (int stride = 0x10; stride <= 0x18; stride += 8) {
        for (int i = 0; i < 5; ++i) {
            uint32_t nm = R<uint32_t>(bt + (uint64_t)i * stride);
            int32_t par = R<int32_t>(bt + (uint64_t)i * stride + 8);
            char nb[104]; FNameToBuf(nm, nb, 104);
            fprintf(f, "  TRY st=0x%X i=%d nm=%u par=%d name=%s\n", stride, i, nm, par, nb);
        }
    }
    for (int i = 0; i < cnt && i < 200; ++i) {
        uint32_t nm = R<uint32_t>(bt + (uint64_t)i * 0x10);
        int32_t par = R<int32_t>(bt + (uint64_t)i * 0x10 + 8);
        char nb[104]; FNameToBuf(nm, nb, 104);
        fprintf(f, "  BONE %d parent=%d name=%s\n", i, par, nb);
    }
}


static void DumpSkeletonFull(uintptr_t pawn, FILE* f) {
    uintptr_t mesh = DPtr(R<uint64_t>(pawn + off::MESH));
    if (!ValidPtr(mesh)) { fprintf(f, "SKEL no mesh\n"); return; }
    uintptr_t asset = DPtr(R<uint64_t>(mesh + off::MESH_ASSET));
    uintptr_t skel = ValidPtr(asset) ? DPtr(R<uint64_t>(asset + 0x60)) : 0;
    uintptr_t bt = ValidPtr(skel) ? DPtr(R<uint64_t>(skel + off::SKEL_BONETREE)) : 0;
    int32_t cnt = ValidPtr(skel) ? R<int32_t>(skel + 0x40) : 0;
    fprintf(f, "SKEL mesh=%p asset=%p skel=%p bt=%p cnt=%d\n", (void*)mesh, (void*)asset, (void*)skel, (void*)bt, cnt);
    if (ValidPtr(bt) && cnt > 0) {
        int n32 = cnt * 6; if (n32 > 0x600 / 4) n32 = 0x600 / 4;
        for (int i = 0; i < n32; ++i) {
            uint32_t v = R<uint32_t>(bt + (uint64_t)i * 4);
            fprintf(f, "U %d %08X\n", i, v);
        }
        int strides[3] = { 0xC, 0x10, 0x18 };
        for (int si = 0; si < 3; ++si) {
            int st = strides[si];
            for (int i = 0; i < cnt && i < 25; ++i) {
                uint32_t n0 = R<uint32_t>(bt + (uint64_t)i * st);
                uint32_t n1 = R<uint32_t>(bt + (uint64_t)i * st + 4);
                int32_t par = R<int32_t>(bt + (uint64_t)i * st + 8);
                char nb[104]; FNameToBuf(n0, nb, 104);
                fprintf(f, "ST st=0x%X i=%d n0=%u n1=%u par=%d name=%s\n", st, i, n0, n1, par, nb);
            }
        }
    }
    uintptr_t bp = R<uintptr_t>(mesh + off::BONES);
    int32_t bc = R<int32_t>(mesh + off::BONES + 8);
    fprintf(f, "BONES ptr=%p cnt=%d\n", (void*)bp, bc);
    if (ValidPtr(bp) && bc > 0) {
        float qx=R<float>(mesh+off::COMPONENT_TW),qy=R<float>(mesh+off::COMPONENT_TW+4),qz=R<float>(mesh+off::COMPONENT_TW+8),qw=R<float>(mesh+off::COMPONENT_TW+12);
        float tx=R<float>(mesh+off::COMPONENT_TW+0x10),ty=R<float>(mesh+off::COMPONENT_TW+0x14),tz=R<float>(mesh+off::COMPONENT_TW+0x18);
        for (int i = 0; i < bc && i < 80; ++i) {
            float lx=R<float>(bp+(uint64_t)i*off::BONE_STRIDE+0x10), ly=R<float>(bp+(uint64_t)i*off::BONE_STRIDE+0x14), lz=R<float>(bp+(uint64_t)i*off::BONE_STRIDE+0x18);
            float wx,wy,wz; QRot(qx,qy,qz,qw,lx,ly,lz,wx,wy,wz);
            fprintf(f, "BT i=%d local=(%.1f,%.1f,%.1f) world=(%.1f,%.1f,%.1f)\n", i, lx,ly,lz, wx+tx,wy+ty,wz+tz);
        }
    }
}

static void DumpBones(uintptr_t actor, FILE* f) {
    uintptr_t mesh = R<uintptr_t>(actor + off::MESH);
    if (!ValidPtr(mesh)) return;
    char mc[104]; ClassNameToBuf(mesh, mc, 104);
    uintptr_t root = DPtr(R<uint64_t>(actor + off::ROOT_COMP));
    SVec3 ppos{}; if (ValidPtr(root)) ppos = R<SVec3>(root + off::REL_LOCATION);
    fprintf(f, "CHAR %p mesh=%p class=%s pos=(%.1f,%.1f,%.1f)\n", (void*)actor, (void*)mesh, mc, ppos.x, ppos.y, ppos.z);
    {
        float cq0=R<float>(mesh+off::COMPONENT_TW), cq1=R<float>(mesh+off::COMPONENT_TW+4), cq2=R<float>(mesh+off::COMPONENT_TW+8), cq3=R<float>(mesh+off::COMPONENT_TW+12);
        float ct0=R<float>(mesh+off::COMPONENT_TW+0x10), ct1=R<float>(mesh+off::COMPONENT_TW+0x14), ct2=R<float>(mesh+off::COMPONENT_TW+0x18);
        fprintf(f, "  CTW210 t=(%.1f,%.1f,%.1f) q=(%.3f,%.3f,%.3f,%.3f)\n", ct0,ct1,ct2,cq0,cq1,cq2,cq3);
        uintptr_t cap = DPtr(R<uint64_t>(actor + 0x108));
        if (ValidPtr(cap)) {
            float kx=R<float>(cap+off::COMPONENT_TW+0x10), ky=R<float>(cap+off::COMPONENT_TW+0x14), kz=R<float>(cap+off::COMPONENT_TW+0x18);
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
            float z = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 0x18);
            if (z == z) { if (z < minz) minz = z; if (z > maxz) maxz = z; }
        }
        for (int i = 0; i < n; ++i) {
            float q0 = R<float>(bp + (uint64_t)i*off::BONE_STRIDE), q1 = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 4);
            float q2 = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 8), q3 = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 12);
            float x = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 0x10), y = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 0x14), z = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 0x18);
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


static bool LocalHead(uintptr_t pawn, float& ox, float& oy, float& oz) {
    uintptr_t mesh = DPtr(R<uint64_t>(pawn + off::MESH));
    if (!ValidPtr(mesh)) return false;
    uintptr_t bp = R<uintptr_t>(mesh + off::BONES);
    int32_t cnt = R<int32_t>(mesh + off::BONES + 8);
    if (!ValidPtr(bp) || cnt < 10 || cnt > 400) return false;
    float qx = R<float>(mesh + off::COMPONENT_TW), qy = R<float>(mesh + off::COMPONENT_TW + 4), qz = R<float>(mesh + off::COMPONENT_TW + 8), qw = R<float>(mesh + off::COMPONENT_TW + 12);
    float tx = R<float>(mesh + off::COMPONENT_TW + 0x10), ty = R<float>(mesh + off::COMPONENT_TW + 0x14), tz = R<float>(mesh + off::COMPONENT_TW + 0x18);
    float qn = qx*qx + qy*qy + qz*qz + qw*qw;
    if (!(qn > 0.8f && qn < 1.2f)) return false;
    float bestz = -1e9f; float bx = 0, by = 0, bz = 0;
    for (int i = 0; i < cnt && i < 400; ++i) {
        float lx = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 0x10), ly = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 0x14), lz = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 0x18);
        if (!((lx==lx)&&(ly==ly)&&(lz==lz))) continue;
        float wx, wy, wz;
        QRot(qx,qy,qz,qw, lx,ly,lz, wx,wy,wz);
        if (wz > bestz) { bestz = wz; bx = wx + tx; by = wy + ty; bz = wz + tz; }
    }
    if (bestz < -1e8f) return false;
    ox = bx; oy = by; oz = bz;
    return true;
}

static void FillBones(SPlayer& p, uintptr_t actor) {
    p.boneCount = 0;
    uintptr_t mesh = DPtr(R<uint64_t>(actor + off::MESH));
    if (!ValidPtr(mesh)) return;
    uintptr_t bp = R<uintptr_t>(mesh + off::BONES);
    int32_t cnt = R<int32_t>(mesh + off::BONES + 8);
    if (!ValidPtr(bp) || cnt < 10 || cnt > 200) return;
    float qx = R<float>(mesh + off::COMPONENT_TW), qy = R<float>(mesh + off::COMPONENT_TW + 4), qz = R<float>(mesh + off::COMPONENT_TW + 8), qw = R<float>(mesh + off::COMPONENT_TW + 12);
    float tx = R<float>(mesh + off::COMPONENT_TW + 0x10), ty = R<float>(mesh + off::COMPONENT_TW + 0x14), tz = R<float>(mesh + off::COMPONENT_TW + 0x18);
    float qn = qx*qx + qy*qy + qz*qz + qw*qw;
    if (!(qn > 0.8f && qn < 1.2f)) return;
    int n = 0;
    for (int i = 0; i < cnt && i < 80; ++i) {
        float lx = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 0x10), ly = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 0x14), lz = R<float>(bp + (uint64_t)i*off::BONE_STRIDE + 0x18);
        if (!((lx==lx)&&(ly==ly)&&(lz==lz))) continue;
        float wx, wy, wz;
        QRot(qx,qy,qz,qw, lx,ly,lz, wx,wy,wz);
        p.bones[n].x = wx + tx; p.bones[n].y = wy + ty; p.bones[n].z = wz + tz;
        ++n;
    }
    p.boneCount = n;
}


static void DumpCam(uintptr_t pc, uintptr_t cam, uintptr_t pawn) {
    FILE* f = nullptr; fopen_s(&f, "C:\\Users\\User\\trackA\\body.txt", "a");
    if (!f) return;
    SRot ctrl = R<SRot>(pc + off::CONTROL_ROT);
    SVec3 pp{};
    uintptr_t root = DPtr(R<uint64_t>(pawn + off::ROOT_COMP));
    if (ValidPtr(root)) pp = R<SVec3>(root + off::REL_LOCATION);
    char ccn[104]; ClassNameToBuf(cam, ccn, 104);
    fprintf(f, "CAMDBG pc=%p cam=%p class=%s pawn=%p ctrl=(%.2f,%.2f,%.2f) ppos=(%.1f,%.1f,%.1f)\n",
            (void*)pc, (void*)cam, ccn, (void*)pawn, ctrl.pitch, ctrl.yaw, ctrl.roll, pp.x, pp.y, pp.z);
    for (uint32_t o = 0x380; o < 0x480; o += 4) {
        float v = R<float>(cam + o);
        fprintf(f, "  RAW 0x%X %08X f=%.3f\n", o, *(uint32_t*)&v, v);
    }
    {
        uintptr_t tc = DPtr(R<uint64_t>(cam + 0x378));
        char tcn[104] = {0}; ClassNameToBuf(tc, tcn, 104);
        float qx=R<float>(tc+off::COMPONENT_TW), qy=R<float>(tc+off::COMPONENT_TW+4), qz=R<float>(tc+off::COMPONENT_TW+8), qw=R<float>(tc+off::COMPONENT_TW+12);
        float tx=R<float>(tc+0x220), ty=R<float>(tc+0x224), tz=R<float>(tc+0x228);
        fprintf(f, "CAMTC p=%p class=%s t=(%.1f,%.1f,%.1f) q=(%.3f,%.3f,%.3f,%.3f)\n",
                (void*)tc, tcn, tx, ty, tz, qx, qy, qz, qw);
        uintptr_t pcown = DPtr(R<uint64_t>(cam + 0x370));
        char pcn[104] = {0}; ClassNameToBuf(pcown, pcn, 104);
        fprintf(f, "CAMPC p=%p class=%s\n", (void*)pcown, pcn);
    }
    {
        fprintf(f, "LOCROT scan\n");
        int n = 0;
        for (uint32_t o = 0; o + 0x20 <= 0x80000; o += 4) {
            float lx = R<float>(cam + o), ly = R<float>(cam + o + 4), lz = R<float>(cam + o + 8);
            float pitch = R<float>(cam + o + 0xC), yaw = R<float>(cam + o + 0x10), roll = R<float>(cam + o + 0x14);
            if (!((lx==lx)&&(ly==ly)&&(lz==lz)&&(pitch==pitch)&&(yaw==yaw))) continue;
            if (!(fabsf(pitch) <= 89.f && yaw >= 0.f && yaw <= 360.f)) continue;
            float dx = lx - pp.x, dy = ly - pp.y, dz = lz - pp.z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);
            if (dist > 1500.f) continue;
            float dyaw = fabsf(yaw - ctrl.yaw); if (dyaw > 180.f) dyaw = 360.f - dyaw;
            float dp = fabsf(pitch - ctrl.pitch);
            if (dyaw > 15.f || dp > 15.f) continue;
            fprintf(f, "  LR off=0x%X dist=%.1f loc=(%.1f,%.1f,%.1f) rot=(%.2f,%.2f,%.2f) dyaw=%.2f dp=%.2f\n",
                    o, dist, lx, ly, lz, pitch, yaw, roll, dyaw, dp);
            if (++n >= 25) break;
        }
    }
    uint32_t bases[3] = { 0x2E360, 0x2F7A0, 0x3E0 };
    for (int bi = 0; bi < 3; ++bi) {
        uint32_t base = bases[bi];
        fprintf(f, "VT base=0x%X\n", base);
        for (uint32_t i = 0; i < 0x80; i += 4) {
            float v = R<float>(cam + base + i);
            if (v != 0.f && v == v && fabsf(v) < 1e6f)
                fprintf(f, "  f 0x%X=%.2f\n", i, v);
        }
        for (uint32_t i = 0; i < 0x80; i += 8) {
            double d = R<double>(cam + base + i);
            if (d != 0.0 && d == d && fabs(d) < 1e6)
                fprintf(f, "  d 0x%X=%.2f\n", i, d);
        }
    }
    struct Cand { float score, dist, fov, pitch, yaw, lx, ly, lz; uint32_t off; };
    Cand best[12] = {};
    int nb = 0;
    for (uint32_t o = 0; o + 0x28 <= 0x80000; o += 4) {
        float lx = R<float>(cam + o), ly = R<float>(cam + o + 4), lz = R<float>(cam + o + 8);
        float pitch = R<float>(cam + o + 0xC), yaw = R<float>(cam + o + 0x10), roll = R<float>(cam + o + 0x14);
        float fov = R<float>(cam + o + 0x18);
        if (!((lx==lx)&&(ly==ly)&&(lz==lz)&&(pitch==pitch)&&(yaw==yaw)&&(fov==fov))) continue;
        if (!(fov > 20.f && fov < 160.f)) continue;
        float dx = lx - pp.x, dy = ly - pp.y, dz = lz - pp.z;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
        if (dist > 5000.f) continue;
        float dyaw = fabsf(yaw - ctrl.yaw); if (dyaw > 180.f) dyaw = 360.f - dyaw;
        float score = dist;
        Cand c{ score, dist, fov, pitch, yaw, lx, ly, lz, o };
        if (nb < 12) best[nb++] = c;
        else {
            int worst = 0; for (int i = 1; i < 12; ++i) if (best[i].score > best[worst].score) worst = i;
            if (score < best[worst].score) best[worst] = c;
        }
    }
    for (int i = 0; i < nb; ++i) {
        Cand& c = best[i];
        float dyaw = fabsf(c.yaw - ctrl.yaw); if (dyaw > 180.f) dyaw = 360.f - dyaw;
        fprintf(f, "  POV off=0x%X dist=%.1f fov=%.2f rot=(%.2f,%.2f) dyaw=%.2f loc=(%.1f,%.1f,%.1f)\n",
                c.off, c.dist, c.fov, c.pitch, c.yaw, dyaw, c.lx, c.ly, c.lz);
    }
    int nd = 0;
    for (uint32_t o = 0; o + 0x30 <= 0x80000; o += 8) {
        double lx = R<double>(cam + o), ly = R<double>(cam + o + 8), lz = R<double>(cam + o + 16);
        float pitch = R<float>(cam + o + 0x18), yaw = R<float>(cam + o + 0x1C), roll = R<float>(cam + o + 0x20), fov = R<float>(cam + o + 0x24);
        if (!((lx==lx)&&(ly==ly)&&(lz==lz)&&(pitch==pitch)&&(yaw==yaw)&&(fov==fov))) continue;
        if (!(fov > 20.f && fov < 160.f)) continue;
        if (!(fabsf(lx) < 400000.0) || !(fabsf(ly) < 400000.0)) continue;
        double ddx = lx - pp.x, ddy = ly - pp.y, ddz = lz - pp.z;
        double dist = sqrt(ddx*ddx + ddy*ddy + ddz*ddz);
        if (dist > 5000.0) continue;
        float dyaw = fabsf(yaw - ctrl.yaw); if (dyaw > 180.f) dyaw = 360.f - dyaw;
        fprintf(f, "  POVD off=0x%X dist=%.1f fov=%.2f rot=(%.2f,%.2f) dyaw=%.2f loc=(%.1f,%.1f,%.1f)\n",
                o, (float)dist, fov, pitch, yaw, dyaw, (float)lx, (float)ly, (float)lz);
        if (++nd >= 12) break;
    }
    if (!nd) fprintf(f, "  POVD none\n");
    fclose(f);
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
    // Level: pointer valid sudah cukup. NameIs dilewati kalau resolusi nama gagal
    // sesaat (FName cache miss) supaya state tidak flap 1<->3.
    if (!ValidPtr(level)) { st.state = 3; return; }
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
            if (ValidPtr(ps)) { st.localCamp = R<int32_t>(ps + off::CAMP_TRUE); st.localTeam = R<int32_t>(ps + off::TEAMID); st.selfDead = 0; }
            uintptr_t r0 = DPtr(R<uint64_t>(localPawn + off::ROOT_COMP));
            if (ValidPtr(r0)) {
                SVec3 ctw = R<SVec3>(r0 + off::COMPONENT_TW + 0x10);
                SVec3 rel = R<SVec3>(r0 + off::REL_LOCATION);
                SVec3 sp = rel;
                if (PosOK(sp)) {
                    st.selfPos = sp;
                    float hx, hy, hz;
                    if (LocalHead(localPawn, hx, hy, hz)) {
                        st.camLoc = SVec3{ hx, hy, hz - 10.f };
                    } else {
                        st.camLoc = SVec3{ sp.x, sp.y, sp.z + 64.f };
                    }
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
#ifdef PROBE_ONLY
    { static bool g_camDumped = false;
      if (!g_camDumped && ValidPtr(cam) && ValidPtr(pc) && ValidPtr(localPawn)) { g_camDumped = true; DumpCam(pc, cam, localPawn); } }
#endif
    st.pc = pc; st.localPawn = localPawn;
    st.camMode = camMode;
    if (ValidPtr(pc)) {
        uint8_t mouse = R<uint8_t>(pc + 0x588);
        uint8_t waiting = R<uint8_t>(pc + 0x520);
        if (mouse || waiting) st.selfDead = 1;
    }
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
            uintptr_t m3 = DPtr(R<uint64_t>(a + off::MESH));
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
            p.camp = R<int32_t>(ps + off::CAMP_TRUE);   // match team id (0 = local team)
            p.team = R<int32_t>(ps + off::TEAMID);   // squad slot 1..4
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
                    bool isChar = SContains(cn, "Character") || SContains(cn, "Pawn") ||
                                  SContains(cn, "RangeTarget") || SContains(cn, "TargetDummy") || SContains(cn, "Dummy");
                    if (!isChar) continue;
                    if (SContains(cn, "Health") || SContains(cn, "PropertyReplication") || SContains(cn, "Redeploy") ||
                        SContains(cn, "MapRandom") || SContains(cn, "Pickup") || SContains(cn, "Item") || SContains(cn, "Default__")) continue;
                    if (a == localPawn) continue;
                    bool dup = false;
                    for (int k = 0; k < n; ++k) if (st.players[k].pawn == a) { dup = true; break; }
                    if (dup) continue;
                    uintptr_t m3 = DPtr(R<uint64_t>(a + off::MESH));
                    if (!ValidPtr(m3)) continue;
                    float mx = R<float>(m3 + 0x220), my = R<float>(m3 + 0x224), mz = R<float>(m3 + 0x228);
                    SVec3 pos{ mx, my, mz };
                    if (!PosOK(pos)) continue;
                    uintptr_t ps = DPtr(R<uint64_t>(a + off::PLAYERSTATE));
                    SPlayer& p = st.players[n];
                    p.pawn = a; p.ps = ps;
                    if (ValidPtr(ps)) {
                        ReadFStringToBuf(ps + off::PLAYERNAME, p.name, 64);
                        if (!p.name[0]) ReadFStringToBuf(ps + 0x488, p.name, 64);
                        if (SContains(p.name, "BP_") || SContains(p.name, "PropertyReplication") || SContains(p.name, "Default__")) p.name[0] = 0;
                        p.camp = R<int32_t>(ps + off::CAMP_TRUE);
                        p.team = R<int32_t>(ps + off::TEAMID);
                    } else {
                        SCopy(p.name, 64, cn);
                        p.camp = -1; p.team = 0;
                    }
                    p.bot = 0;
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
    static bool pm = false;
    bool i = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    bool f = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
    bool h = (GetAsyncKeyState(VK_HOME) & 0x8000) != 0;
    bool m = ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_MENU) & 0x8000) && (GetAsyncKeyState('M') & 0x8000));
    int bits = 0;
    if (i && !pi) bits |= 1;
    if (f && !pf) bits |= 2;
    if (h && !ph) bits |= 4;
    if (m && !pm) bits |= 8;
    pi = i; pf = f; ph = h; pm = m;
    if (bits && g_shm) { g_shm->keyEdges = bits; InterlockedIncrement(&g_shm->keySeq); }
}

static void AimLoop() {
    for (;;) {
        PollKeys();
        uintptr_t pc = (uintptr_t)g_pcShared;
        if (pc && g_shm) {
            LONG s1 = g_shm->cmd.seq;
            if (!(s1 & 1)) {
                int32_t en = g_shm->cmd.enable;
                float wy = g_shm->cmd.yaw, wp = g_shm->cmd.pitch;
                float ms = g_shm->cmd.maxStep;
                uint32_t ts = g_shm->cmd.stamp;
                int32_t mode = g_shm->cmd.mode;
                LONG s2 = g_shm->cmd.seq;
                if (s1 == s2 && en && (GetTickCount() - ts) < 150) {
                    SRot ctrl = R<SRot>(pc + off::CONTROL_ROT);
                    float dY = wy - ctrl.yaw;
                    while (dY > 180.f) dY -= 360.f;
                    while (dY < -180.f) dY += 360.f;
                    float dP = wp - ctrl.pitch;
                    while (dP > 180.f) dP -= 360.f;
                    while (dP < -180.f) dP += 360.f;
                    if (!(ms > 0.02f)) ms = 0.6f;
                    if (ms > 6.0f) ms = 6.0f;
                    if (dY > ms) dY = ms; if (dY < -ms) dY = -ms;
                    if (dP > ms) dP = ms; if (dP < -ms) dP = -ms;
                    if (fabsf(dY) < 0.10f && fabsf(dP) < 0.10f) { Sleep(8); continue; }
                    static uint32_t js = 0x1234567u;
                    js = js * 1664525u + 1013904223u;
                    float jy = ((float)((js >> 16) & 0x3F) - 32.f) * 0.0008f;
                    js = js * 1664525u + 1013904223u;
                    float jp = ((float)((js >> 16) & 0x3F) - 32.f) * 0.0008f;
                    if ((js & 0x1F) == 0) { Sleep(8); continue; }
                    if (mode == 1) {
                        ctrl.yaw += dY; ctrl.pitch += dP;
                        WriteMem((void*)(pc + off::CONTROL_ROT), &ctrl, sizeof(ctrl));
                    } else {
                        SRot r{ dP + jp, dY + jy, 0.f };
                        WriteMem((void*)(pc + off::ROTATION_INPUT), &r, sizeof(r));
                    }
                    static int ac = 0;
                    if ((ac++ & 31) == 0) {
                        FILE* f = nullptr; fopen_s(&f, "C:\Users\User\trackA\aim.log", "a");
                        if (f) { fprintf(f, "APPLY dY=%.2f dP=%.2f ctrl=(%.2f,%.2f)\n", dY, dP, ctrl.pitch, ctrl.yaw); fclose(f); }
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


static void AimCore(SharedState& st) {
    static DWORD lastFlag = 0;
    static bool autoAim = false;
    DWORD now = GetTickCount();
    if (now - lastFlag > 500) {
        lastFlag = now;
        autoAim = (GetFileAttributesA("C:\\Users\\User\\trackA\\aim.flag") != INVALID_FILE_ATTRIBUTES);
    }
    int cfgAim = 0, cfgAuto = 0;
    float cfgFov = 160.f, cfgStep = 0.35f;
    if (g_menuCfg && g_menuCfg->magic == MENU_MAGIC && g_menuCfg->version == MENU_VERSION) {
        cfgAim = g_menuCfg->aim; cfgAuto = g_menuCfg->pad;
        cfgFov = g_menuCfg->aimFov; cfgStep = g_menuCfg->aimStep;
    }
    bool rmb = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    bool active = autoAim || cfgAuto || (cfgAim && rmb);
    if (!active || st.state != 1 || !st.camValid || !st.pc) return;
    float fovHalf = atanf((cfgFov / 960.f) * tanf(st.camFov * 3.14159265f / 360.f)) * 180.f / 3.14159265f;
    if (!(fovHalf > 0.5f)) fovHalf = 1.f;
    if (fovHalf > 60.f) fovHalf = 60.f;
    float p = st.camRot.pitch * 3.14159265f / 180.f;
    float y = st.camRot.yaw * 3.14159265f / 180.f;
    float fx = cosf(p) * cosf(y), fy = cosf(p) * sinf(y), fz = sinf(p);
    float best = 1e9f; int bi = -1; float tx = 0, ty = 0, tz = 0;
    for (int i = 0; i < st.playerCount; ++i) {
        const SPlayer& pl = st.players[i];
        if (pl.dist > 500.f) continue;
        if (st.localCamp >= 0 && pl.camp == st.localCamp) continue;
        float hx, hy, hz;
        if (pl.boneCount > 32) { hx = pl.bones[32].x; hy = pl.bones[32].y; hz = pl.bones[32].z; }
        else { hx = pl.pos.x; hy = pl.pos.y; hz = pl.pos.z + 62.f; }
        float dx = hx - st.camLoc.x, dy = hy - st.camLoc.y, dz = hz - st.camLoc.z;
        float len = sqrtf(dx*dx + dy*dy + dz*dz); if (len < 1.f) continue;
        float dot = (dx*fx + dy*fy + dz*fz) / len;
        if (dot < 0.3f) continue;
        float ang = acosf(dot) * 180.f / 3.14159265f;
        if (ang <= fovHalf && ang < best) { best = ang; bi = i; tx = hx; ty = hy; tz = hz; }
    }
    if (bi < 0) return;
    float dx = tx - st.camLoc.x, dy = ty - st.camLoc.y, dz = tz - st.camLoc.z;
    float wy = atan2f(dy, dx) * 180.f / 3.14159265f;
    float wp = atan2f(dz, sqrtf(dx*dx + dy*dy)) * 180.f / 3.14159265f;
    SRot cur = R<SRot>(st.pc + off::CONTROL_ROT);
    float dY = wy - cur.yaw; while (dY > 180.f) dY -= 360.f; while (dY < -180.f) dY += 360.f;
    float dP = wp - cur.pitch; while (dP > 180.f) dP -= 360.f; while (dP < -180.f) dP += 360.f;
    float ms = cfgStep * 10.f;
    if (!(ms > 0.5f)) ms = 0.5f; if (ms > 20.f) ms = 20.f;
    if (dY > ms) dY = ms; if (dY < -ms) dY = -ms;
    if (dP > ms) dP = ms; if (dP < -ms) dP = -ms;
    cur.yaw += dY; cur.pitch += dP;
    WriteMem((void*)(st.pc + off::CONTROL_ROT), &cur, sizeof(cur));
    static int cnt = 0;
    if ((cnt++ & 15) == 0) {
        FILE* f = nullptr; fopen_s(&f, "C:\\Users\\User\\trackA\\aim.log", "a");
        if (f) { fprintf(f, "AIM tgt=%d ang=%.2f cur=(%.2f,%.2f) want=(%.2f,%.2f) d=(%.2f,%.2f)\n", bi, best, cur.pitch, cur.yaw, wp, wy, dP, dY); fclose(f); }
    }
}

static void EnsureOverlay() {
    static DWORD last = 0; static bool started = false;
    if (started) return;
    DWORD now = GetTickCount();
    if (now - last < 1000) return;
    last = now;
    if (GetFileAttributesA("C:\\Users\\User\\trackA\\start_overlay.flag") == INVALID_FILE_ATTRIBUTES) return;
    DeleteFileA("C:\\Users\\User\\trackA\\start_overlay.flag");
    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    char cwd[] = "C:\\Users\\User\\trackA";
    if (CreateProcessA("C:\\Users\\User\\trackA\\dfimgui_overlay.exe", nullptr, nullptr, nullptr, FALSE, 0, nullptr, cwd, &si, &pi)) {
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread); started = true;
    }
}

static void KeyTest() {
    static DWORD last = 0;
    DWORD now = GetTickCount();
    if (now - last < 500) return;
    last = now;
    if (GetFileAttributesA("C:\\Users\\User\\trackA\\keytest.flag") == INVALID_FILE_ATTRIBUTES) return;
    DeleteFileA("C:\\Users\\User\\trackA\\keytest.flag");
    keybd_event(VK_INSERT, 0, 0, 0);
    Sleep(40);
    keybd_event(VK_INSERT, 0, KEYEVENTF_KEYUP, 0);
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
    { HANDLE mm = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(MenuCfg), L"DFC6_menu_v1");
      if (mm) g_menuCfg = (MenuCfg*)MapViewOfFile(mm, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MenuCfg)); }
    g_shm->magic = SHM_MAGIC;
    g_shm->version = SHM_VERSION;

    SharedState st;
#ifdef PROBE_ONLY
    Refresh(st);
    return;
#else
    for (;;) {
        Refresh(st);
        EnsureOverlay();
        KeyTest();
        Publish(st);
        Sleep(10);
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
