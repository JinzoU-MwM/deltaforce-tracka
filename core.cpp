// dfcore: read-only game state publisher (no window, no D3D, no Present hook)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <math.h>
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

static void Refresh(SharedState& st) {
    MZero(&st, sizeof(st));
    st.magic = SHM_MAGIC; st.version = SHM_VERSION;
    uintptr_t w = GetWorld();
    if (!w) { st.state = 2; return; }
    uintptr_t level = DPtr(R<uint64_t>(w + off::PERSISTENT_LEVEL));
    uintptr_t gi = DPtr(R<uint64_t>(w + off::OWNING_GI));
    if (!ValidPtr(level) || !NameIs(level, g_levelTag)) { st.state = 3; return; }
    if (!ValidPtr(gi)) { st.state = 4; return; }

    st.world = w; st.level = level; st.gi = gi;
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
    g_pcShared = pc;

    uintptr_t arr = R<uintptr_t>(level + off::LEVEL_ACTORS);
    int32_t cnt = R<int32_t>(level + off::LEVEL_ACTORS + 8);
    if (cnt < 0 || cnt > 200000) cnt = 0;
    st.actorCount = cnt;

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

        if (!SContains(cn, g_charTag) && !SContains(cn, g_pawnTag)) continue;
        if (SContains(cn, "Health") || SContains(cn, "PropertyReplication") || SContains(cn, "Redeploy") ||
            SContains(cn, "MapRandom") || SContains(cn, "Pickup") || SContains(cn, "Item") || SContains(cn, "Default__")) continue;
        if (a == localPawn) continue;
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
        if (!ValidPtr(ps)) continue;
        SPlayer& p = st.players[n];
        p.pawn = a; p.ps = ps;
        if (ValidPtr(ps)) {
            ReadFStringToBuf(ps + off::PLAYERNAME, p.name, 64);
            if (!p.name[0]) ReadFStringToBuf(ps + 0x488, p.name, 64);
            if (SContains(p.name, "BP_") || SContains(p.name, "PropertyReplication") || SContains(p.name, "Default__")) p.name[0] = 0;
            p.camp = R<int32_t>(ps + 0x660);   // match team id (0 = local team)
            p.team = R<int32_t>(ps + 0x66C);   // squad slot 1..4
            p.bot = 0;
        }
        p.pos = pos;
        if (st.camValid) {
            float dx = pos.x - st.camLoc.x, dy = pos.y - st.camLoc.y, dz = pos.z - st.camLoc.z;
            p.dist = (float)sqrtf(dx * dx + dy * dy + dz * dz) * 0.01f;
        }
        if (!p.name[0]) SCopy(p.name, 64, cn);
        ++n;
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
    for (;;) {
        Refresh(st);
        Publish(st);
        Sleep(33);
    }
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
