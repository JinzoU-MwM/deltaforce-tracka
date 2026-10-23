#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "shared.h"
int main() {
    wchar_t name[64]; int i = 0; for (; kShmName[i]; ++i) name[i] = (wchar_t)(kShmName[i] ^ 0x5A); name[i] = 0;
    HANDLE h = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(SharedState), name);
    if (!h) { printf("MAP FAIL %lu\n", GetLastError()); return 1; }
    SharedState* s = (SharedState*)MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedState));
    if (!s) return 2;
    memset(s, 0, sizeof(*s));
    s->magic = SHM_MAGIC; s->version = SHM_VERSION; s->state = 1;
    s->camValid = 1; s->camLoc = { 0,0,0 }; s->camRot = { 0,0,0 }; s->camFov = 90;
    s->playerCount = 1; s->actorCount = 139;
    SPlayer& p = s->players[0];
    strcpy_s(p.name, "TestTarget"); p.camp = 2; p.dist = 25.f;
    p.pos = { 0.f, 1000.f, 100.f };
    printf("wrote fake state, sleeping 15s\n");
    Sleep(15000);
    return 0;
}
