#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

static bool Readable(uintptr_t a, size_t n) {
    if (!a) return false;
    MEMORY_BASIC_INFORMATION m{};
    if (!VirtualQuery((LPCVOID)a, &m, sizeof(m))) return false;
    if (m.State != MEM_COMMIT) return false;
    if (m.Protect & PAGE_NOACCESS) return false;
    return (uintptr_t)m.BaseAddress + m.RegionSize >= a + n;
}
template<typename T> static T Rd(uintptr_t a) { return *(T*)a; }
static uintptr_t D(uint64_t v) { return (uintptr_t)(v & 0xFFFFFFFFFFFFULL); }
static uintptr_t DP(uintptr_t a) { return Readable(a, 8) ? D(Rd<uint64_t>(a)) : 0; }

static void DumpBlob(FILE* f, const char* tag, uintptr_t addr, DWORD size) {
    char hdr[128]; wsprintfA(hdr, "%s %p %lu\n", tag, (void*)addr, size); fwrite(hdr, 1, lstrlenA(hdr), f);
    if (addr && size && Readable(addr, size)) fwrite((void*)addr, 1, size, f);
    else { char z[4] = {0}; fwrite(z, 1, 1, f); }
}
static DWORD WINAPI Th(LPVOID) {
    uintptr_t B = (uintptr_t)GetModuleHandleW(nullptr);
    const uintptr_t GWorld = 0x1D9E4E78;
    uintptr_t world = DP(B + GWorld);
    uintptr_t level = world ? DP(world + 0xF8) : 0;
    uintptr_t gi = world ? DP(world + 0x190) : 0;
    uintptr_t lpArr = gi ? Rd<uintptr_t>(gi + 0x38) : 0;
    uintptr_t lp = (lpArr && Readable(lpArr, 8)) ? D(Rd<uint64_t>(lpArr)) : 0;
    uintptr_t pc = (lp && Readable(lp + 0x30, 8)) ? D(Rd<uint64_t>(lp + 0x30)) : 0;
    uintptr_t pawn = pc ? DP(pc + 0x3F0) : 0;
    uintptr_t ps = pawn ? DP(pawn + 0x390) : 0;
    uintptr_t cam = pc ? DP(pc + 0x408) : 0;
    FILE* f = nullptr; _wfopen_s(&f, L"C:\\Users\\User\\trackA\\dump.bin", L"wb");
    if (!f) return 1;
    char line[512]; wsprintfA(line, "B=%p world=%p level=%p gi=%p lpArr=%p lp=%p pc=%p pawn=%p ps=%p cam=%p\n", (void*)B, (void*)world, (void*)level, (void*)gi, (void*)lpArr, (void*)lp, (void*)pc, (void*)pawn, (void*)ps, (void*)cam);
    fwrite(line, 1, lstrlenA(line), f);
    DumpBlob(f, "WORLD", world, 0x600);
    DumpBlob(f, "LEVEL", level, 0x600);
    DumpBlob(f, "GI", gi, 0x400);
    DumpBlob(f, "PC", pc, 0x800);
    DumpBlob(f, "PAWN", pawn, 0x800);
    DumpBlob(f, "PS", ps, 0x800);
    DumpBlob(f, "CAM", cam, 0x800);
    // ---- scan ----
    FILE* g = nullptr; _wfopen_s(&g, L"C:\\Users\\User\\trackA\\scan.txt", L"w");
    if (g) {
        struct V3 { float x, y, z; };
        V3 ploc{}; uintptr_t r0 = pawn ? DP(pawn + 0x180) : 0;
        if (r0) { ploc = *(V3*)(r0 + 0x210 + 0x10); if (ploc.x*ploc.x + ploc.y*ploc.y + ploc.z*ploc.z < 0.01f) ploc = *(V3*)(r0 + 0x168); }
        fprintf(g, "B=%p pc=%p pawn=%p ps=%p ploc=%.2f,%.2f,%.2f\n", (void*)B, (void*)pc, (void*)pawn, (void*)ps, ploc.x, ploc.y, ploc.z);
        if (pc && Readable(pc + 0x3D8, 12)) { float* cr = (float*)(pc + 0x3D8); fprintf(g, "ctrl=%.2f,%.2f,%.2f\n", cr[0], cr[1], cr[2]); }
        fprintf(g, "== camera offsets in PC ==\n");
        for (DWORD off = 0x3A0; off <= 0x4A0; off += 8) {
            if (!Readable(pc + off, 8)) continue;
            uintptr_t p2 = D(Rd<uint64_t>(pc + off));
            if (!Readable(p2, 0x80)) continue;
            for (DWORD q = 0; q <= 0x300; q += 4) {
                if (!Readable(p2 + q, 0x1C)) break;
                float* v = (float*)(p2 + q);
                float dx = v[0]-ploc.x, dy = v[1]-ploc.y, dz = v[2]-ploc.z;
                float d2 = dx*dx + dy*dy + dz*dz;
                float fov = *(float*)(p2 + q + 0x18);
                float pit = *(float*)(p2 + q + 0xC), yaw = *(float*)(p2 + q + 0x10);
                if (d2 > 4.f && d2 < 4000000.f && fov > 40.f && fov < 130.f && pit > -90.f && pit < 90.f && yaw > -360.f && yaw < 360.f) {
                    fprintf(g, "PC+0x%X -> %p : POV@0x%X loc=%.1f,%.1f,%.1f rot=%.2f,%.2f fov=%.2f dist=%.1f\n",
                        off, (void*)p2, q, v[0], v[1], v[2], pit, yaw, fov, sqrtf(d2));
                }
            }
        }
        fprintf(g, "== name offsets in PS ==\n");
        for (DWORD off = 0; off < 0x800; off += 8) {
            if (!Readable(ps + off + 16, 16)) break;
            uintptr_t sp = D(Rd<uint64_t>(ps + off));
            int32_t len = *(int32_t*)(ps + off + 8);
            if (sp && len >= 2 && len <= 64 && Readable(sp, (size_t)len * 2)) {
                wchar_t w[80]; memcpy(w, (void*)sp, (size_t)len * 2); w[len] = 0;
                bool ok = true; for (int i = 0; i < len; ++i) if (w[i] < 32 || w[i] > 0x2FFF) { ok = false; break; }
                if (ok) { char u[256]; WideCharToMultiByte(CP_UTF8, 0, w, -1, u, sizeof(u), nullptr, nullptr); fprintf(g, "PS+0x%X ptr=%p len=%d str=%s\n", off, (void*)sp, len, u); }
            }
        }
        fprintf(g, "== pointers in PAWN with string or PS-like ==\n");
        for (DWORD off = 0x300; off <= 0x500; off += 8) {
            if (!Readable(pawn + off, 8)) continue;
            uintptr_t p2 = D(Rd<uint64_t>(pawn + off));
            if (!Readable(p2, 0x200)) continue;
            uintptr_t sp = D(Rd<uint64_t>(p2 + 0x478));
            int32_t len = *(int32_t*)(p2 + 0x478 + 8);
            if (sp && len >= 2 && len <= 64 && Readable(sp, (size_t)len * 2)) {
                wchar_t w[80]; memcpy(w, (void*)sp, (size_t)len * 2); w[len] = 0;
                bool ok = true; for (int i = 0; i < len; ++i) if (w[i] < 32 || w[i] > 0x2FFF) { ok = false; break; }
                if (ok) { char u[256]; WideCharToMultiByte(CP_UTF8, 0, w, -1, u, sizeof(u), nullptr, nullptr); fprintf(g, "PAWN+0x%X -> %p : +0x478 str=%s\n", off, (void*)p2, u); }
            }
        }
        fclose(g);
    }
    fclose(f);
    // ---- players + localplayers scan ----
    FILE* q = nullptr; _wfopen_s(&q, L"C:\\Users\\User\\trackA\\players.txt", L"w");
    if (q) {
        struct V3 { float x, y, z; };
        fprintf(q, "gi=%p lpArr=%p cnt=%d\n", (void*)gi, (void*)lpArr, (int)(Readable(gi + 0x40, 4) ? *(int32_t*)(gi + 0x40) : -1));
        int cnt = Readable(gi + 0x40, 4) ? *(int32_t*)(gi + 0x40) : 0;
        for (int i = 0; i < cnt && i < 8; ++i) {
            if (!Readable(lpArr + (uintptr_t)i * 8, 8)) break;
            uintptr_t lpi = D(Rd<uint64_t>(lpArr + (uintptr_t)i * 8));
            if (!lpi) continue;
            uintptr_t pci = Readable(lpi + 0x30, 8) ? D(Rd<uint64_t>(lpi + 0x30)) : 0;
            fprintf(q, "LOCAL[%d] lp=%p pc=%p\n", i, (void*)lpi, (void*)pci);
            if (!pci) continue;
            float* cr = Readable(pci + 0x3D8, 12) ? (float*)(pci + 0x3D8) : nullptr;
            uintptr_t pwn = D(Rd<uint64_t>(pci + 0x3F0));
            uintptr_t cm = D(Rd<uint64_t>(pci + 0x408));
            fprintf(q, "  ctrl=%.2f,%.2f,%.2f pawn=%p cam=%p\n", cr ? cr[0] : 0, cr ? cr[1] : 0, cr ? cr[2] : 0, (void*)pwn, (void*)cm);
            if (pwn) {
                uintptr_t r0 = D(Rd<uint64_t>(pwn + 0x180));
                V3 l1{}, l2{};
                if (r0) { l1 = *(V3*)(r0 + 0x220); l2 = *(V3*)(r0 + 0x168); }
                fprintf(q, "  root=%p loc1=%.1f,%.1f,%.1f loc2=%.1f,%.1f,%.1f\n", (void*)r0, l1.x, l1.y, l1.z, l2.x, l2.y, l2.z);
            }
            if (cm) {
                for (DWORD off = 0x3C0; off <= 0x440; off += 4) {
                    if (!Readable(cm + off, 0x20)) break;
                    float* v = (float*)(cm + off);
                    float fov = *(float*)(cm + off + 0x18);
                    if (fabsf(v[0]) > 1.f && fabsf(v[0]) < 500000.f && fov > 40.f && fov < 130.f)
                        fprintf(q, "  CAM+0x%X loc=%.1f,%.1f,%.1f rot=%.2f,%.2f,%.2f fov=%.2f\n", off, v[0], v[1], v[2], *(float*)(cm + off + 0xC), *(float*)(cm + off + 0x10), *(float*)(cm + off + 0x14), fov);
                }
            }
        }
        uintptr_t arr = Readable(level + 0x98, 8) ? Rd<uintptr_t>(level + 0x98) : 0;
        int acnt = Readable(level + 0xA0, 4) ? *(int32_t*)(level + 0xA0) : 0;
        fprintf(q, "LEVEL arr=%p acnt=%d\n", (void*)arr, acnt);
        int found = 0;
        for (int i = 0; i < acnt && i < 3000 && found < 40; ++i) {
            if (!Readable(arr + (uintptr_t)i * 8, 8)) break;
            uintptr_t a = Rd<uintptr_t>(arr + (uintptr_t)i * 8);
            if (!Readable(a + 0x390, 8)) continue;
            uintptr_t psi = D(Rd<uint64_t>(a + 0x390));
            if (!Readable(psi + 0x480, 0x10)) continue;
            uintptr_t sp = D(Rd<uint64_t>(psi + 0x478));
            int32_t len = *(int32_t*)(psi + 0x478 + 8);
            if (!sp || len < 2 || len > 32 || !Readable(sp, (size_t)len * 2)) continue;
            wchar_t w[40]; memcpy(w, (void*)sp, (size_t)len * 2); w[len] = 0;
            bool ok = true; for (int k = 0; k < len; ++k) if (w[k] < 32 || w[k] > 0x2FFF) { ok = false; break; }
            if (!ok) continue;
            char u[128]; WideCharToMultiByte(CP_UTF8, 0, w, -1, u, sizeof(u), nullptr, nullptr);
            uintptr_t r0 = D(Rd<uint64_t>(a + 0x180));
            V3 l1{}, l2{};
            uint16_t fl = 0xEEEE;
            if (r0) { l1 = *(V3*)(r0 + 0x220); l2 = *(V3*)(r0 + 0x168); if (Readable(r0 + 0x240, 2)) fl = *(uint16_t*)(r0 + 0x240); }
            int camp = Readable(psi + 0x670, 4) ? *(int32_t*)(psi + 0x670) : -1;
            int team = Readable(psi + 0x66C, 4) ? *(int32_t*)(psi + 0x66C) : -1;
            fprintf(q, "PLAYER a=%p ps=%p name=%s camp=%d team=%d root=%p flag=%04X ctw=%.1f,%.1f,%.1f rel=%.1f,%.1f,%.1f\n", (void*)a, (void*)psi, u, camp, team, (void*)r0, fl, l1.x, l1.y, l1.z, l2.x, l2.y, l2.z);
            ++found;
        }
        // decryption function check
        {
            uintptr_t fnAddr = (uintptr_t)0; uint16_t flag = 0;
            uintptr_t g = B + 0x1432F5A8ULL;
            if (Readable(g, 8)) fnAddr = Rd<uint64_t>(g);
            uintptr_t r0 = pawn ? D(Rd<uint64_t>(pawn + 0x180)) : 0;
            if (r0 && Readable(r0 + 0x240, 2)) flag = *(uint16_t*)(r0 + 0x240);
            fprintf(q, "DECFN global=%p fn=%p in_module=%d flag=%04X\n", (void*)g, (void*)fnAddr,
                (fnAddr >= B && fnAddr < B + 0x20000000) ? 1 : 0, flag);
            if (r0 && Readable(r0 + 0x210, 0x40)) {
                float* tr = (float*)(r0 + 0x220);
                fprintf(q, "ROOTX tr=%.2f,%.2f,%.2f\n", tr[0], tr[1], tr[2]);
            }
        }
        // camera debug        // camera debug
        if (pc) {
            uint64_t raw408 = Rd<uint64_t>(pc + 0x408);
            uintptr_t cm = D(raw408);
            fprintf(q, "CAMDBG raw408=%llx cam=%p readonly=%d\n", (unsigned long long)raw408, (void*)cm, Readable(cm, 0x40) ? 1 : 0);
            if (cm) {
                for (DWORD co = 0x3E0; co <= 0x2DEC0; co = (co == 0x3E0 ? 0x2DEC0 : 0x2DEC1)) {
                    if (!Readable(cm + co + 0x10, 0x30)) { fprintf(q, "  CAM+0x%X unreadable\n", co); continue; }
                    float* v = (float*)(cm + co + 0x10);
                    fprintf(q, "  CAM+0x%X loc=%.2f,%.2f,%.2f rot=%.2f,%.2f,%.2f fov=%.2f\n", co, v[0], v[1], v[2], v[3], v[4], v[5], v[6]);
                }
                // scan cam for plausible POV
                for (DWORD q2 = 0; q2 < 0x400000; q2 += 4) {
                    if (!Readable(cm + q2, 0x20)) { q2 += 0xFFC; continue; }
                    float* v = (float*)(cm + q2);
                    float fov = v[6];
                    if (fabsf(v[0]) > 1000.f && fabsf(v[1]) > 1000.f && fov > 40.f && fov < 130.f && fabsf(v[3]) < 90.f && fabsf(v[4]) < 360.f) {
                        fprintf(q, "  SCAN 0x%X loc=%.1f,%.1f,%.1f rot=%.2f,%.2f fov=%.2f\n", q2, v[0], v[1], v[2], v[3], v[4], fov);
                    }
                }
            }
        }
        // mirror world test        // mirror world test
        uint64_t rawm = Rd<uint64_t>(B + 0x1D9E6E78);
        uint8_t bb[8]; memcpy(bb, &rawm, 8);
        uint8_t t0 = bb[0], t7 = bb[7];
        bb[0] = (uint8_t)(t7 ^ 0x36); bb[7] = (uint8_t)(t0 ^ 0x36);
        uint64_t mraw; memcpy(&mraw, bb, 8);
        uintptr_t wm = D(mraw);
        fprintf(q, "MIRROR raw=%llx dec=%p\n", (unsigned long long)rawm, (void*)wm);
        if (Readable(wm, 0x200)) {
            uintptr_t lvm = D(Rd<uint64_t>(wm + 0xF8));
            uintptr_t gim = D(Rd<uint64_t>(wm + 0x190));
            uintptr_t lpa = gim ? Rd<uintptr_t>(gim + 0x38) : 0;
            int lcn = gim ? *(int32_t*)(gim + 0x40) : 0;
            uintptr_t lpi = (lpa && lcn > 0) ? D(Rd<uint64_t>(lpa)) : 0;
            uintptr_t pcm = lpi ? D(Rd<uint64_t>(lpi + 0x30)) : 0;
            uintptr_t pwnm = pcm ? D(Rd<uint64_t>(pcm + 0x3F0)) : 0;
            uintptr_t r0m = pwnm ? D(Rd<uint64_t>(pwnm + 0x180)) : 0;
            float* crm = pcm ? (float*)(pcm + 0x3D8) : nullptr;
            V3 lm{};
            if (r0m) lm = *(V3*)(r0m + 0x220);
            fprintf(q, "MIRROR level=%p gi=%p lpcnt=%d lp=%p pc=%p pawn=%p root=%p ctrl=%.2f,%.2f,%.2f loc=%.1f,%.1f,%.1f\n",
                (void*)lvm, (void*)gim, lcn, (void*)lpi, (void*)pcm, (void*)pwnm, (void*)r0m,
                crm ? crm[0] : 0, crm ? crm[1] : 0, crm ? crm[2] : 0, lm.x, lm.y, lm.z);
        }
        fclose(q);
    }
    return 0;
}
BOOL APIENTRY DllMain(HMODULE, DWORD r, LPVOID) {
    if (r == DLL_PROCESS_ATTACH) { CreateThread(nullptr, 0, Th, nullptr, 0, nullptr); }
    return TRUE;
}
