#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "shared.h"
#include <stdarg.h>
static void Out(const char* fmt, ...) {
    char b[1024]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof(b), fmt, ap); va_end(ap);
    printf("%s", b);
    FILE* f = nullptr; fopen_s(&f, "C:\\Users\\User\\trackA\\shm_now.log", "a");
    if (f) { fputs(b, f); fclose(f); }
}
int main() {
    wchar_t name[64]; int i = 0; for (; kShmName[i]; ++i) name[i] = (wchar_t)(kShmName[i] ^ 0x5A); name[i] = 0;
    HANDLE h = OpenFileMappingW(FILE_MAP_READ, FALSE, name);
    if (!h) { Out("NO SECTION\n"); return 1; }
    SharedState* s = (SharedState*)MapViewOfFile(h, FILE_MAP_READ, 0, 0, sizeof(SharedState));
    if (!s) { Out("NO VIEW\n"); return 2; }
    Out("magic=%08X ver=%u seq=%ld state=%d players=%d actors=%d cam=%d world=%llx level=%llx\n",
        s->magic, s->version, s->seq, s->state, s->playerCount, s->actorCount, s->camValid,
        (unsigned long long)s->world, (unsigned long long)s->level);
    Out("cam mode=%d valid=%d loc=%.0f,%.0f,%.0f rot=%.1f,%.1f,%.1f fov=%.0f self=%.0f,%.0f,%.0f lcamp=%d lteam=%d dead=%d pc=%llx pawn=%llx\n",
        s->camMode, s->camValid, s->camLoc.x, s->camLoc.y, s->camLoc.z, s->camRot.pitch, s->camRot.yaw, s->camRot.roll,
        s->camFov, s->selfPos.x, s->selfPos.y, s->selfPos.z, s->localCamp, s->localTeam, s->selfDead,
        (unsigned long long)s->pc, (unsigned long long)s->localPawn);
    for (int i = 0; i < s->dbgCount && i < 64; ++i) {
        Out("dbg[%d] cls=%s pos=%.0f,%.0f,%.0f dist=%.1f\n", i, s->dbg[i].cls,
            s->dbg[i].pos.x, s->dbg[i].pos.y, s->dbg[i].pos.z, s->dbg[i].dist);
    }
    for (int i = 0; i < s->playerCount && i < 20; ++i) Out("p%d name=%s camp=%d team=%d bot=%d dist=%.1f pos=%.0f,%.0f,%.0f\n",
        i, s->players[i].name, s->players[i].camp, s->players[i].team, s->players[i].bot, s->players[i].dist,
        s->players[i].pos.x, s->players[i].pos.y, s->players[i].pos.z);
    return 0;
}
