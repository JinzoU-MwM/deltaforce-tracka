#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

static bool Readable(uintptr_t a, size_t n) {
    if (!a) return false;
    MEMORY_BASIC_INFORMATION m{};
    if (!VirtualQuery((LPCVOID)a, &m, sizeof(m))) return false;
    if (m.State != MEM_COMMIT || (m.Protect & PAGE_NOACCESS)) return false;
    return (uintptr_t)m.BaseAddress + m.RegionSize >= a + n;
}
template<typename T> static T Rd(uintptr_t a) { return *(T*)a; }
static uintptr_t D(uint64_t v) { return (uintptr_t)(v & 0xFFFFFFFFFFFFULL); }
static uintptr_t DP(uintptr_t a) { return Readable(a, 8) ? D(Rd<uint64_t>(a)) : 0; }
struct V3 { float x, y, z; };
static void Blob(FILE* f, const char* tag, uintptr_t a, DWORD n) {
    char h[96]; wsprintfA(h, "%s %p %lu\n", tag, (void*)a, n); fwrite(h, 1, lstrlenA(h), f);
    if (a && n && Readable(a, n)) fwrite((void*)a, 1, n, f); else { char z = 0; fwrite(&z, 1, 1, f); }
}
static DWORD WINAPI Th(LPVOID) {
    uintptr_t B = (uintptr_t)GetModuleHandleW(nullptr);
    uintptr_t world = DP(B + 0x1D9E4E78);
    uintptr_t level = world ? DP(world + 0xF8) : 0;
    uintptr_t gi = world ? DP(world + 0x190) : 0;
    uintptr_t lpArr = gi ? Rd<uintptr_t>(gi + 0x38) : 0;
    uintptr_t lp = (lpArr && Readable(lpArr, 8)) ? D(Rd<uint64_t>(lpArr)) : 0;
    uintptr_t pc = (lp && Readable(lp + 0x30, 8)) ? D(Rd<uint64_t>(lp + 0x30)) : 0;
    uintptr_t pawn = pc ? DP(pc + 0x3F0) : 0;
    uintptr_t ps = pawn ? DP(pawn + 0x390) : 0;
    uintptr_t cam = pc ? DP(pc + 0x408) : 0;
    uintptr_t root = pawn ? DP(pawn + 0x180) : 0;
    uintptr_t mesh = pawn ? DP(pawn + 0x3D0) : 0;
    FILE* f = nullptr; _wfopen_s(&f, L"C:\\Users\\User\\trackA\\dump.bin", L"wb");
    if (!f) return 1;
    char line[512]; wsprintfA(line, "B=%p world=%p level=%p gi=%p lpArr=%p lp=%p pc=%p pawn=%p ps=%p cam=%p root=%p mesh=%p\n",
        (void*)B, (void*)world, (void*)level, (void*)gi, (void*)lpArr, (void*)lp, (void*)pc, (void*)pawn, (void*)ps, (void*)cam, (void*)root, (void*)mesh);
    fwrite(line, 1, lstrlenA(line), f);
    Blob(f, "PC", pc, 0x2000);
    Blob(f, "PAWN", pawn, 0x2000);
    Blob(f, "PS", ps, 0x2000);
    Blob(f, "CAM", cam, 0x34000);
    Blob(f, "MESH", mesh, 0x2000);
    Blob(f, "ROOT", root, 0x400);
    fclose(f);
    FILE* g = nullptr; _wfopen_s(&g, L"C:\\Users\\User\\trackA\\scan.txt", L"w");
    if (g) {
        V3 ploc{}; uint16_t fl = 0;
        if (root) { ploc = *(V3*)(root + 0x220); if (Readable(root + 0x240, 2)) fl = *(uint16_t*)(root + 0x240); }
        fprintf(g, "CHAIN pc=%p pawn=%p ps=%p cam=%p root=%p mesh=%p\n", (void*)pc, (void*)pawn, (void*)ps, (void*)cam, (void*)root, (void*)mesh);
        if (pc && Readable(pc + 0x3D8, 12)) { float* c = (float*)(pc + 0x3D8); fprintf(g, "CTRL %.2f %.2f %.2f\n", c[0], c[1], c[2]); }
        if (root) {
            V3 ctw = *(V3*)(root + 0x220), rel = *(V3*)(root + 0x168);
            fprintf(g, "PAWN loc ctw=%.2f,%.2f,%.2f rel=%.2f,%.2f,%.2f flag=%04X\n", ctw.x, ctw.y, ctw.z, rel.x, rel.y, rel.z, fl);
        }
        if (ps) {
            uintptr_t sp = DP(ps + 0x478); int32_t len = Readable(ps + 0x480, 4) ? *(int32_t*)(ps + 0x478 + 8) : 0;
            fprintf(g, "PS name ptr=%p len=%d camp=%d team=%d bot=%d dead=%d\n", (void*)sp, len,
                Readable(ps + 0x670, 4) ? *(int32_t*)(ps + 0x670) : -1,
                Readable(ps + 0x66C, 4) ? *(int32_t*)(ps + 0x66C) : -1,
                Readable(ps + 0x39E, 1) ? *(uint8_t*)(ps + 0x39E) : -1,
                Readable(ps + 0x4B4, 1) ? *(uint8_t*)(ps + 0x4B4) : -1);
            if (sp && len > 0 && len < 60 && Readable(sp, (size_t)len * 2)) {
                wchar_t w[64]; memcpy(w, (void*)sp, (size_t)len * 2); w[len] = 0;
                char u[128]; WideCharToMultiByte(CP_UTF8, 0, w, -1, u, sizeof(u), nullptr, nullptr);
                fprintf(g, "PS name str=%s\n", u);
            }
            // FString scan in PS
            for (DWORD o = 0; o + 16 <= 0x2000; o += 8) {
                if (!Readable(ps + o, 16)) break;
                uintptr_t p = DP(ps + o); int32_t l = *(int32_t*)(ps + o + 8);
                if (p && l >= 2 && l <= 40 && Readable(p, (size_t)l * 2)) {
                    wchar_t w[48]; memcpy(w, (void*)p, (size_t)l * 2); w[l] = 0;
                    bool ok = true; for (int i = 0; i < l; ++i) if (w[i] < 32 || w[i] > 0x2FFF) { ok = false; break; }
                    if (ok) { char u[128]; WideCharToMultiByte(CP_UTF8, 0, w, -1, u, sizeof(u), nullptr, nullptr); fprintf(g, "PS FSTR +0x%X len=%d = %s\n", o, l, u); }
                }
            }
        }
        if (cam) {
            fprintf(g, "CAM scan start\n");
            for (DWORD q = 0; q + 0x28 <= 0x34000; q += 4) {
                if (!Readable(cam + q, 0x28)) { q += 0xFFC; continue; }
                float* v = (float*)(cam + q);
                float fov = v[6];
                if (!(fov > 30.f && fov < 140.f)) continue;
                if (!(fabsf(v[3]) < 90.f && fabsf(v[4]) < 360.f)) continue;
                float dx = v[0] - ploc.x, dy = v[1] - ploc.y, dz = v[2] - ploc.z;
                float d2 = dx * dx + dy * dy + dz * dz;
                if (d2 < 40000.f) fprintf(g, "CAM+0x%X NEAR loc=%.1f,%.1f,%.1f rot=%.2f,%.2f,%.2f fov=%.2f d=%.1f\n", q, v[0], v[1], v[2], v[3], v[4], v[5], fov, sqrtf(d2));
            }
        }
        fclose(g);
    }
    return 0;
}
BOOL APIENTRY DllMain(HMODULE, DWORD r, LPVOID) { if (r == DLL_PROCESS_ATTACH) CreateThread(nullptr, 0, Th, nullptr, 0, nullptr); return TRUE; }
