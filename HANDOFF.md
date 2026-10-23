# HANDOFF — Delta Force ESP + Aim (Track A)

Updated: **2026-10-23** (core30 / overlay V30)
Target: `DeltaForceClient-Win64-Shipping.exe` **1.14.0.772** (Steam/international)
Repo: private `JinzoU-MwM/deltaforce-tracka`

Status singkat: ESP semua player (box/nama/jarak/skeleton), FOV circle, ADS FOV, prediksi gerak, aim assist via `ROTATION_INPUT`. Anti-cheat membatasi reinject setelah injeksi pertama (details §9).

---

## 0. TL;DR

- **core30.dll** (in-game) publish `SharedState` **V30** tiap **10 ms**: scan **semua level**, class `Character`/`Pawn` (+ RangeTarget/Dummy), `PlayerState` **opsional**, maks 128 player, `FillBones` 80 bone.
- **overlay multi** (`dfimgui_overlay.exe`) di session game: scan shm **V22–V30**, pilih versi core yang hidup (prefer tertinggi); ImGui D3D11 layered; ESP, FOV circle, prediksi `g_draw`, ADS FOV, aim `AimCmd`.
- **Deploy**: inject **SYSTEM** (`dfsecinject.exe <pid> dfcore30.dll`), overlay lewat `StartOverlay.bat` / core `EnsureOverlay` (`start_overlay.flag`).
- **Hotkey**: `INSERT` / `Ctrl+Alt+M` dibaca **core** (`PollKeys` → `keySeq/keyEdges`), overlay toggle menu. Fallback: file `menu.flag` via `ToggleMenu.bat`.
- **Aim**: target dalam FOV circle, head bone 32, `AimCmd` absolute yaw/pitch, core apply ke `ROTATION_INPUT` (bukan `ControlRotation`).

---

## 1. Arsitektur

```
┌──────────────────────────── DeltaForceClient-Win64-Shipping.exe (session user) ─────────────────────────┐
│  core30.dll (injected)                                                                                  │
│    Worker (≈10ms) : Refresh() → players/camera/World → Publish() ke SharedState V30                      │
│    AimLoop (≈8ms): PollKeys() + AimCmd (absolute yaw/pitch) → clamp/jitter → ROTATION_INPUT             │
│    EnsureOverlay(): flag start_overlay.flag → CreateProcess(dfimgui_overlay.exe) di session yang sama     │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────┘
                │ SharedState V30 (named shm, session-local)
                ▼
┌──────────────────────────────── overlay process (session 1, layered topmost) ────────────────────────────┐
│  dfimgui_overlay.exe (multi V22–V30)                                                                     │
│    scan shm → lock versi core yang valid → Snapshot(seqlock) → UpdatePrediction() → g_draw               │
│    DrawESP: FOV circle + box/name/dist + skeleton (bone 66+) + DRAW counter                              │
│    AimTick: target px di dalam circle → AimCmd absolute → core AimLoop                                   │
│    Menu: INSERT/Ctrl+Alt+M (keyEdges) atau menu.flag; slider FOV/step/maxDist/FOV ADS                    │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

**MenuCfg** (`DFC6_menu_v1`) adalah jalur lama untuk `AimCore` core-core lama. Overlay **selalu menulis `aim=0` dan `pad=0`** supaya `AimCore` (direct `ControlRotation`) tidak pernah aktif; aim hanya lewat `AimCmd` → `ROTATION_INPUT`.

---

## 2. Shared memory (`shared.h`)

Nama shm di-obfuscate XOR `0x5A` per versi (`kShmName` di `shared.h`, dipilih `/DSHM_Vxx`). Overlay tidak memakai `kShmName`; dia hardcode `kName22..kName30` dan mencoba semuanya (V22→V30, prefer tertinggi).

```c
struct SPlayer {
    uint64_t pawn, ps;
    char     name[64];
    int32_t  camp, team, bot;
    SVec3    pos;
    float    dist;          // meter (cm * 0.01)
    SVec3    bones[80];     // world, raw mesh bone order
    int32_t  boneCount;
};

struct AimCmd {
    volatile LONG seq;      // seqlock
    int32_t enable;
    float   yaw, pitch;     // absolute desired control-rotation angles (deg)
    float   maxStep;        // clamp deg per 8ms apply
    uint32_t stamp;         // GetTickCount() overlay; core accepts <150ms
    int32_t mode;           // 0 = ROTATION_INPUT (default), 1 = ControlRotation (unused)
};

struct MenuCfg {            // legacy; overlay menulis aim=0/pad=0
    uint32_t magic, version;
    int32_t  esp, boxes, names, dist, team, body, aim;
    float    aimFov, aimStep, maxDist, camZ, fovAdj, fovAds;
    int32_t  visible, pad;
};

struct SharedState {
    uint32_t magic;         // 0x44464331
    uint32_t version;       // 30 (V22..V30 didukung overlay)
    volatile LONG seq;
    int32_t  state;         // 0 init, 1 ok, 2 no world, 3 level mismatch, 4 no GI
    int32_t  actorCount, localCamp, localTeam, playerCount, camValid;
    SVec3    camLoc;  SRot camRot;  float camFov;
    uint64_t world, level, gi, pc, localPawn;
    SRot     controlRot;  SVec3 selfPos;
    int32_t  camMode;       // 1..4; 2 = fallback controlRot + head, 4 = auto-scan POV
    int32_t  dbgCount;  SDbgActor dbg[64];
    SPlayer  players[128];
    AimCmd   cmd;
    volatile LONG keySeq;  int32_t keyEdges;  int32_t selfDead;
};
```

**Versi penting**

| Versi | Perubahan |
|---|---|
| V22 | `bones[80]`, shm scan multi |
| V23 | `AimCmd` absolute (yaw/pitch) + overlay multi |
| V24 | MenuCfg + PollKeys Ctrl+Alt+M |
| V25/V26 | overlay tanpa RegisterHotKey; keytest |
| V27 | AimCore direct dimatikan; jitter/deadzone AimLoop |
| V28/V29 | FOV circle + prediksi overlay + publish 10ms |
| **V30** | player scan **semua level** + `Character/Pawn` + PS opsional; overlay scan V22–V30 |

---

## 3. Offset game (`namespace off` di `core.cpp`)

```
GWORLD            0x1D9E4E78 (enc 0x1D9E6E78)
GNAMES_BLOCKS     0x1E5A7788
CLASSPRIVATE      0x08
NAMEPRIVATE       0x1C
PERSISTENT_LEVEL  0xF8     // World
OWNING_GI         0x190    // World
LEVEL_ACTORS      0x98     // ULevel.Actors
LOCALPLAYERS      0x38     // GameInstance
LOCAL_PC          0x30     // LocalPlayer
ACK_PAWN          0x3F0    // PlayerController
CAM_MGR           0x408    // PlayerController
ROOT_COMP         0x180    // Actor
REL_LOCATION      0x168    // SceneComponent
COMPONENT_TW      0x210    // mesh quat (x,y,z,w)
PLAYERSTATE       0x390    // Pawn
PLAYERNAME        0x478    // PlayerState FString
CAMP              0x670    // PlayerState team id (0 = local?)
TEAMID            0x66C    // PlayerState squad slot
IS_BOT            0x39E
CAM_CACHE         0x30870  // CameraManager (kandidat POV)
CONTROL_ROT       0x3D8    // PlayerController
ROTATION_INPUT    0x570    // PlayerController
```

**Mesh/bone (verified live)**

```
pawn + 0x3D0                -> USkeletalMeshComponent*
mesh  + 0x760               -> bone array ptr, count +0x768, stride 0x30
bone  + 0x10                -> local position (x,y,z)
mesh  + 0x210 / 0x220       -> component quat / translation (world = quat*local + trans)
mesh  + 0xEB0               -> SkeletalMesh asset
asset + 0x60                -> Skeleton
skeleton + 0x38             -> BoneTree (cnt +0x40 = 75) — **STRIPPED** (FName=0, parent=-1)
mesh bone count             = 80 (5 extra vs skeleton)
```

**Bone index map** (dari posisi world; skeleton di-strip):

| idx | bagian |
|---|---|
| 0 | root |
| 1 | pelvis |
| 2–4 | spine_01..03 |
| 5 | neck |
| 31/32 | head |
| 6–9 | clav/upper/lower/hand **L** |
| 33–36 | clav/upper/lower/hand **R** |
| 58–61 | thigh/calf/foot/ball **L** |
| 62–65 | thigh/calf/foot/ball **R** |
| 66–79 | duplikat/extra mesh (root/arm/foot) |

---

## 4. Core (`core.cpp`)

- **Refresh** (Worker): `GetWorld` → level/`GI` → LocalPlayer/PC/pawn → kamera:
  - mode 1: kandidat POV (`cam+0x2C5E0`, `cam+CAM_CACHE`, `cam+0x2DEC0`, `cam+0x2DEF0`, `cam+0x2F310`)
  - mode 2: fallback `controlRot` + head localPawn (yang dipakai sekarang; camFov=90)
  - mode 4: auto-scan POV (hanya kalau camMode==0; saat ini tidak jalan)
- **Player scan**: persistent level + **semua level** (`World+0x158` Levels): class `Character`/`Pawn`/`RangeTarget`/`TargetDummy`/`Dummy`; `PlayerState` opsional; dedup; max 128; `FillBones`; nama/`camp`/`team` dari PS.
- **Publish**: `SharedState` V30 tiap **Sleep(10)**.
- **AimLoop** (8ms): baca `AimCmd` (absolute), delta terhadap `CONTROL_ROT`, normalize ±180, clamp `maxStep` (+jitter ±0.02, deadzone 0.1°, 1/32 skip) → `ROTATION_INPUT` (mode 0). Tidak menulis `ControlRotation` (mode 1 ada tapi overlay pakai 0).
- **PollKeys** (8ms): INSERT bit1, F8 bit2, HOME bit4, Ctrl+Alt+M bit8 → `keyEdges/keySeq`.
- **KeyTest** (`keytest.flag`): inject INSERT via `keybd_event` untuk tes hotkey.
- **EnsureOverlay** (`start_overlay.flag`): `CreateProcess` overlay di session game (sekali per proses).
- **AimCore legacy**: masih ada di core lama; V27+ tidak dipanggil. Overlay menulis MenuCfg aim=0 supaya tidak aktif.

---

## 5. Overlay (`overlay_imgui.cpp`)

- **shm scan**: `kName22..kName30`, `CreateFileMappingW` + `MapViewOfFile(size 0)`, validasi `magic==0x44464331 && 22<=version<=30 && state<=4 && 0<=playerCount<=128`; lock versi tertinggi; log `shm vXX state=.. players=..` di `ovimg.log`.
- **Snapshot**: seqlock (`g_shm->seq`), copy `SharedState`.
- **UpdatePrediction**: velocity kamera (`camRot/camLoc`) dan per-player dari snapshot sebelumnya; **EMA 0.65/0.35**; clamp rot 1.5°, pos 40 unit, pred maks 40ms → `g_draw`.
- **W2S**: proyeksi pakai `camRot` + offset kalibrasi (`camZ`, `camFwd`, `yawOff`, `pitchOff`, `fovAdj`); **saat RMB (ADS) pakai slider `FOV ADS`** (default 67) supaya box tidak geser/zoom.
- **DrawESP**: FOV circle **selalu**; loop player: `maxDist` (default 1000), team filter (default **off**), box/name/dist, skeleton pakai `bones[66+]` (spine/arms/legs/head), counter `DRAW ply/drawn/team/dist/off`.
- **AimTick**: target head `bones[32]` di dalam FOV circle (`g_aimFov` px); switch delay 120ms; tulis `AimCmd` absolute.
- **Menu**: checkbox ESP/Box/Name/Distance/Team filter/Body/skeleton, Aim assist (RMB), Auto aim, slider `Aim FOV px`, `Max step`, `FOV ADS`, `Max dist`, `Cam Z/FOV adj/Cam Fwd/Yaw/Pitch`, tombol `Calibrate (F9)`.
- **Flags**: `menu.flag` (toggle menu), `aim2.flag` (auto aim test), `spawn_shot.flag` (spawn `shot.exe`), `start_overlay.flag` (core launch), `keytest.flag` (core inject INSERT).
- **Logs**: `ovimg.log` — `shm vXX`, `STATE st/act/ply/cam/camp/team`, `DRAW`, `OPEN/CLOSE`, `hotkey: core-only`, `hook ok`.

---

## 6. Aim assist (stealth path)

1. Overlay pilih target **head bone 32** yang proyeksinya di dalam circle `Aim FOV px` (default 320) — target di luar circle diabaikan.
2. Overlay hitung sudut absolut `yaw = atan2(dy,dx)`, `pitch = atan2(dz,hypot(dx,dy))` dari `camLoc` ke head.
3. `AimCmd {enable,yaw,pitch,maxStep,stamp,mode=0}`.
4. Core `AimLoop`: delta ke `CONTROL_ROT`, clamp `maxStep` (slider `Max step`×10, 0.5–20°), jitter kecil, deadzone, tulis `ROTATION_INPUT`.
5. Tidak ada snap besar (clamp), tidak menulis `ControlRotation` langsung, delay ganti target 120ms.

Catatan: ini tetap memory-write di proses game; bukan jaminan anti-cheat. Tidak ada aim through wall check (LOS) — semua target dalam circle.

---

## 7. Build

Prasyarat: **VS2022 BuildTools** (`vcvars64.bat`), Windows SDK. **Dear ImGui** harus ada di folder `imgui/` (dengan `backends/`). Di build dir `C:\Users\User\trackA`:

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V30 /D_USRDLL /D_WINDLL core.cpp /Fe:dfcore30.dll /link /DLL user32.lib
cl /nologo /std:c++17 /O2 /EHsc /I imgui /I imgui\backends overlay_imgui.cpp imgui\imgui.cpp imgui\imgui_draw.cpp imgui\imgui_tables.cpp imgui\imgui_widgets.cpp imgui\backends\imgui_impl_win32.cpp imgui\backends\imgui_impl_dx11.cpp /Fe:dfimgui_overlay_multi.exe /link d3d11.lib dxgi.lib d3dcompiler.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS
```

`build30.bat` di repo menjalankan keduanya. `setup_imgui.ps1` meng-clone Dear ImGui ke `imgui/`.

---

## 8. Deploy

**Inject (SYSTEM, early)** — user SSH tidak punya hak `MapViewOfFile` (0xC0000022):
```bat
schtasks /create /tn dfa_inj /tr "C:\Users\User\trackA\injsys.bat" /sc once /st 23:59 /ru SYSTEM /rl HIGHEST /f
schtasks /run /tn dfa_inj
```
`injsys.bat` = `dfsecinject.exe <PID> dfcore30.dll > inj30.log 2>&1`.

**Watcher** (inject begitu game start): `watch_inject.bat` loop `tasklist` cari `DeltaForceClient-Win64-Shipping.exe`, lalu inject; jalankan via schtasks SYSTEM.

**Overlay**:
- `StartOverlay.bat`: kill overlay, copy `dfimgui_overlay_multi.exe` → `dfimgui_overlay.exe`, start.
- atau flag `start_overlay.flag` → core `EnsureOverlay` launch overlay di session game (sekali per proses).
- `sessrun.exe` bisa gagal `WTSQueryUserToken 1314`; cara paling andal: core (in-game) atau user double-click.

**Verifikasi**:
```
ovimg.log : shm v30 state=1 players=N
           STATE st=1 act=... ply=... camp=... team=...
           DRAW ply=.. drawn=.. team=0 dist=0 off=0 maxd=1000 tf=0
inj30.log : map game st=0x00000000 ... loader_status=1
aim.log  : AIM tgt=.. cur=(pitch,yaw) want=(pitch,yaw) d=(0,0)
           APPLY dY=.. dP=.. ctrl=(pitch,yaw)
```

---

## 9. Anti-cheat / batasan (penting)

- Injeksi pertama di proses game berhasil (`map game st=0`, `loader_status=1`). Injeksi berikutnya sering **`MapViewOfFile` 0xC0000022** (manual map diblok). Strategi: **inject awal** via watcher SYSTEM saat game baru start.
- `taskkill` game bisa dilaporkan sukses tapi proses tetap/protected; restart game lewat launcher `D:\Delta Force\Delta Force\DeltaForceClient.exe`.
- Overlay **harus di session 1** (session game); proses dari SSH/session 0 tidak terlihat dan shm-nya beda namespace. Core `EnsureOverlay`/user StartOverlay adalah jalur yang benar.
- `shot.exe` menangkap **frame game** (PrintWindow), **bukan layered overlay**; screenshot overlay butuh desktop capture (`GetDC(NULL)`/BitBlt) di session 1.
- `camMode=2` (controlRot + head) bukan POV kamera sebenarnya; ADS/zoom dan recoil bisa bikin box geser/getar. POV sesungguhnya (CameraCache) belum ketemu; auto-scan mode 4 di-skip saat fallback aktif.
- BoneTree **stripped** (nama/parent -1); mapping dari posisi world.
- PlayerState `camp/team` offset 0x670/0x66C bisa berbeda antar mode; default team filter overlay **off** + maxDist 1000 supaya semua player kelihatan.

---

## 10. Known issues / next steps

1. **POV kamera**: cari `CameraCachePrivate.POV` yang benar (loc/rot/FOV) supaya W2S presisi saat ADS/recoil; auto-scan jangan di-skip saat camMode 2.
2. **Mesh chams**: overlay hanya skeleton; body/mesh 100% butuh hook renderer (D3D12) atau menggambar mesh dari vertex buffer.
3. **Player scan**: semua level + Character/Pawn sudah; kalau ada mode dengan class lain, tambah substring.
4. **Injection**: watcher awal vs AC; jangan inject berulang ke proses yang sama (duplikat Worker/shm).
5. **Old cores**: V23–V29 bisa masih hidup di proses yang sama; overlay prefer V30. Restart game untuk bersih.
6. **Screenshot**: tambah desktop capture di overlay (flag) untuk bukti visual ESP.
7. **Aim**: LOS/visibility check, per-weapon FOV ADS, humanization lebih halus.

---

## 11. File map

| File | Isi |
|---|---|
| `core.cpp` | core DLL V30 (Refresh, player scan, AimLoop, PollKeys, EnsureOverlay, legacy AimCore) |
| `shared.h` | SharedState/SPlayer/AimCmd/MenuCfg + nama shm V22–V30 |
| `overlay_imgui.cpp` | overlay multi V22–V30 (scan, W2S, ESP, prediksi, ADS, AimTick, menu) |
| `Offsets.hpp` | UEOffsets (UE5 SDK: FNamePool, UObject, dll) |
| `build30.bat` | build core30 + overlay multi |
| `StartOverlay.bat` | kill overlay → copy multi → start |
| `watch_inject.bat` | watcher SYSTEM: tunggu shipping exe → inject core30 |
| `injsys.bat` / `inj30.bat` | injector satu kali |
| `setup_imgui.ps1` | clone Dear ImGui ke `imgui/` |
| `dist/` | binary siap pakai: `dfcore30.dll`, `dfimgui_overlay_multi.exe`, `dfsecinject.exe`, `sessrun.exe`, `shot.exe`, `restart_helper.exe` |
| `docs/HANDOFF-history-core11.md` | handoff lama (core11, offset awal, riset) |
| `shot.cpp`/`sessrun.cpp`/`restart_helper.cpp`/`gamestart.cpp` | source tool |

---

## 12. Riwayat singkat

- Core11: ESP dasar, player count 39–44, posisi jauh corrupt, kamera aproksimasi.
- V19–V22: ImGui overlay, skeleton 16-bone → semua bone (V22), bone dump (BoneTree stripped).
- V23–V26: AimCmd absolute, MenuCfg, hotkey via core, keytest.
- V27–V29: aim stealth (ROTATION_INPUT), FOV circle, prediksi overlay, publish 10ms.
- V30: fix player scan mode match (semua level + Character/Pawn + PS opsional), overlay multi scan V22–V30, DrawESP pakai `g_draw`, FOV circle selalu, team filter off, maxDist 1000, draw counter.
