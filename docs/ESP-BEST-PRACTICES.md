# ESP BEST-PRACTICES — Delta Force (UE5 + ACE)

> Snapshot kode: `2afc9ab` (core30 + overlay ImGui v31, menu 5 tab). Temuan P0 diverifikasi ulang di HEAD ini. `cad3a40` sudah memperbaiki blink ESP (DRAW 100% frame, miss=0) + menu bertab; `a394118` merapikan `0x66C` -> `off::TEAMID`.
>
> Dokumen ini riset penerapan ESP paling optimal: akurasi, performa, stabilitas, stealth. Semua poin menunjuk file/fungsi nyata di repo ini.

---

## 0. TL;DR — 6 hal dengan dampak terbesar

| # | Temuan | Bukti | Dampak | Aksi |
|---|---|---|---|---|
| 1 | **Fallback kamera menimpa POV asli** | `core.cpp` blok `if (ValidPtr(localPawn))` setelah loop `povs[5]` men-set `camMode=2` (head + ControlRotation, FOV 90) | `camMode` 1/3/5/4 tidak pernah dipakai; box/skeleton geser saat ADS/zoom/recoil | `if (camMode == 0 && ...)` + validasi POV + matriks view-projection |
| 2 | **Render overlay full-frame CPU** | `overlay_imgui.cpp` `PresentLayered()`: CopyResource + Map + 8.3 MB memcpy + loop alpha 2M pixel + `UpdateLayeredWindow` tiap frame, `Sleep(16)` | CPU besar, plafon ~60 fps, frame pacing buruk | swapchain/DirectComposition (GPU), atau minimal `UpdateLayeredWindowIndirect` dirty-rect |
| 3 | **Geometri pakai resolusi layar, bukan client game** | `W = GetSystemMetrics(SM_CXSCREEN)`; tidak ada `GetClientRect`/DPI-aware | misalign saat windowed, 125% scaling, monitor non-primary | `GetClientRect` + `ClientToScreen` + `DwmGetWindowAttribute`; DPI-aware |
| 4 | **Bone read per-bone + index kompresi** | `FillBones`: 3× `R<float>` per bone (~240 read/player), `p.bones[n]` pakai `n` bukan `i` | ~180k read/s; index hardcoded bergeser bila ada bone invalid | batch 1 read/player; `p.bones[i]`; kirim 24 bone yang dipakai |
| 5 | **Vektor deteksi ACE** | `WH_KEYBOARD_LL` global hook; `VirtualProtect` tiap write aim; `DFImGuiOverlay`/"DF Overlay"/`DFC6_menu_v1`/log+flag path plaintext; `SetForegroundWindow` menu | risiko deteksi/scan proses & hook | `RegisterHotKey`/polling; write RW langsung; random/off; `WDA` (trade-off) |
| 6 | **Visual belum optimal** | box pakai `p.pos.z + 72` (bukan head bone 32); `p.bot=0` (IsBot 0x39E idle); `p.dist` dari scan bukan posisi prediksi; health/visibility/radar absen | box tidak presisi, info kurang | head/foot bone atau bounds; IsBot; health (verifikasi GAS); radar; visibility = skip |

**Yang sudah bagus:** overlay proses terpisah, core read-only (no D3D hook/no Present hook), seqlock, `SYSTEM+early` (`docs/INJECTION.md`), scan 25 Hz, manual map, skeleton/box/name/distance/team, prediction kamera+player+bone, blink ESP sudah fix (`cad3a40`, DRAW 100% frame, miss=0), menu 5 tab.

---

## 1. Arsitektur saat ini (fakta dari kode)

| Komponen | File | Perilaku |
|---|---|---|
| Core (internal DLL, manual map) | `core.cpp` | scan `Sleep(40)` (25 Hz); `Level.Actors` + fallback level list; `FillBones`; `Publish` seqlock; `AimLoop` `Sleep(8)` |
| Overlay (external, ImGui) | `overlay_imgui.cpp` | layered window + D3D11 texture → staging → DIB → `UpdateLayeredWindow`; `Snapshot`; `UpdatePrediction`; `DrawESP`; `AimTick` |
| Shared memory | `shared.h` | `SharedState` (~140 KB, 128 player × 80 bone), `AimCmd`, `MenuCfg`; nama XOR 0x5A versi 22–30; menu `DFC6_menu_v1` |
| Injeksi | `dfsecinject`/`injsys`/`watch_inject` | manual map; **SYSTEM + early**; `docs/INJECTION.md` |

Aliran data:
```
game memory --(core: read-only, 25 Hz)--> SharedState (seqlock) --(overlay: Snapshot+prediction)--> ImGui --> layered window
overlay --(AimCmd seqlock)--> core AimLoop --(RotationInput / ControlRotation)--> game
```

---

## 2. Kamera & proyeksi (P0)

### 2.1 Bug fallback menimpa POV asli

Urutan di `core.cpp` (fungsi scan/update kamera):

```cpp
// 1) coba 5 kandidat POV: cam+0x2C5E0, cam+0x30870 (CameraCache), 0x2DEC0, 0x2DEF0, 0x2F310
for (int ci = 0; ci < 5 && camMode == 0; ++ci) { ... camMode = 1 + ci; }

// 2) fallback head + ControlRotation — TIDAK dibatasi camMode == 0
if (ValidPtr(localPawn)) {
    ... st.camLoc = head; st.camRot = st.controlRot; st.camFov = 90.f; camMode = 2;
}

// 3) auto-scan CameraManager hanya jika camMode == 0 — tidak pernah tercapai
if (camMode == 0 && ValidPtr(cam) && ValidPtr(pc)) { ... camMode = 4; }
```

Akibat: kandidat POV yang valid **ditimpa** fallback → `st.camMode` selalu 2 (terlihat di `ovimg.log`), W2S memakai head+controlRot+FOV 90. Itu sebabnya perlu `g_camZ`, `g_camFwd`, `g_fovAdj`, `g_fovAds`, `g_yawOff`, `g_pitchOff` dan tombol Calibrate.

**Fix minimal:**

```cpp
if (camMode == 0 && ValidPtr(localPawn)) { ... fallback ... }
```

**Fix lengkap (urutan yang benar):**

```
1. kandidat POV tetap → validasi ketat
2. auto-scan CameraManager (jika belum ada) → pilih skor terbaik
3. fallback head+ControlRotation hanya jika benar-benar tidak ada POV
```

Validasi POV (bukan hanya "nilai masuk akal"):

```cpp
// POV dianggap live bila:
// 1. TimeStamp CameraCache masih muda (< 0.5 s) — jika offset TimeStamp terverifikasi
// 2. sudut forward POV vs ControlRotation < ~15° (setelah wrap 360)
// 3. jarak camLoc ke head local pawn < ~300 unit
// 4. FOV 60..120, AspectRatio 1.0..3.0, ProjectionMode == perspective
```

Auto-scan sekarang memberi `scoreOut = 0` untuk semua kandidat dan memilih yang pertama ditemukan. Ganti skor:

```cpp
float ang = AngleBetween(Forward(povRot), Forward(controlRot));   // derajat
float dHead = Distance(povLoc, localHead);
score = ang + dHead * 0.01f;   // terkecil = paling mungkin POV aktif
```

### 2.2 W2S: satu sumber kebenaran (matriks)

Saat ini proyeksi dihitung ulang di overlay dari `st.camLoc/camRot/camFov` plus offset manual. Best practice: **core menghitung matriks view-projection dari POV yang sama dengan snapshot**, overlay hanya mengalikan.

```cpp
// core: sekali per publish (bukan per frame overlay)
// pov.Location, pov.Rotation, pov.FOV (horizontal), aspect = W/H client, near/far
FMatrix view = FInverseRotationMatrix(pov.Rotation) * FTranslationMatrix(-pov.Location);
FMatrix proj = FPerspectiveMatrix(pov.FOV * PI / 360.f, aspect, 1.f, 100000.f);
FMatrix vp = view * proj;               // 16 float -> SharedState
```

```cpp
// overlay
float4 clip = mul(vp, float4(world, 1));
if (clip.w <= 0.01f) return false;      // behind camera
float ndcX = clip.x / clip.w, ndcY = clip.y / clip.w;
sx = (ndcX * 0.5f + 0.5f) * W;
sy = (1.f - (ndcY * 0.5f + 0.5f)) * H;
```

Keuntungan: `g_pitchOff/g_yawOff/g_camZ/g_camFwd/g_fovAdj/g_fovAds` tidak diperlukan; ADS/zoom/recoil otomatis benar karena POV sudah berisi FOV dan rotasi aktual; box dan aim memakai kamera yang sama.

Catatan UE5: `FMinimalViewInfo.FOV` adalah **horizontal**; `AspectRatio` dan `bConstrainAspectRatio` harus dipakai bila game letterbox; `ProjectionMode` ortho tidak relevan untuk mode ini.

### 2.3 Verifikasi

- Proyeksikan head/feet **local pawn** → harus di dekat bawah layar (first person) atau tengah (jika terlihat).
- Bandingkan dengan `ProjectWorldToScreen` milik game (fungsi sudah pernah dicari di `dfpws.cpp`/`dfgs.cpp`) untuk 3–5 titik; error target < 2 px.
- Log `camMode`, `camFov`, `pov.TimeStamp`, skor POV ke `ovimg.log`; pastikan `camMode != 2` saat POV tersedia.
- Uji ADS (RMB), zoom scope, recoil, naik kendaraan: box tidak boleh bergeser relatif ke model.

---

## 3. Overlay render (P0)

### 3.1 Biaya sekarang

`PresentLayered()` tiap frame:
1. `CopyResource(g_stage, g_tex)` — GPU → staging (readback).
2. `Map` staging (sinkron GPU→CPU).
3. `memcpy` 1920×1080×4 = **8.3 MB** per frame.
4. Loop alpha premultiply **2.07 juta pixel** per frame.
5. `UpdateLayeredWindow` full-screen (DWM copy).
6. `Sleep(16)` setelahnya.

Pada 60 fps: ~500 MB/s copy + loop CPU + DWM; ini plafon fps dan sumber stutter. `W/H` juga fixed dari `SM_CXSCREEN`, bukan client game.

### 3.2 Best practice: GPU swapchain / DirectComposition

Opsi A — window transparan + swapchain (paling sederhana):

```cpp
// window: WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE
// (tanpa WS_EX_LAYERED); latar transparan via DwmExtendFrameIntoClientArea(-1)
MARGINS m{ -1, -1, -1, -1 };
DwmExtendFrameIntoClientArea(hwnd, &m);

DXGI_SWAP_CHAIN_DESC1 d{};
d.Width = W; d.Height = H;
d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
d.BufferCount = 2;
d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
d.SampleDesc.Count = 1;
d.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
factory->CreateSwapChainForHwnd(dev, hwnd, &d, nullptr, nullptr, &swap);
swap->SetMaximumFrameLatency(1);
```

Opsi B — transparan penuh (per-pixel alpha) dengan DirectComposition:

```cpp
// window: WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE
d.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
factory->CreateSwapChainForComposition(dev, &d, nullptr, &swap);
DCompositionCreateDevice3(dxgiDev, IID_PPV_ARGS(&dcomp));
dcomp->CreateTargetForHwnd(hwnd, TRUE, &target);
dcomp->CreateVisual(&visual);
visual->SetContent(swap);
target->SetRoot(visual);
dcomp->Commit();
```

Frame loop: `ImGui::Render` → `Present(1, 0)` (vsync) atau `Present(0, 0)` + waitable object; **tanpa** `Sleep(16)`. Latency lebih rendah, CPU turun drastis, tidak ada readback.

Fallback bila swapchain tidak memungkinkan: tetap layered, tapi:
- render hanya **dirty rect** (bounding box ESP + margin), bukan full-screen;
- premultiply alpha di GPU (blend state), bukan loop CPU;
- `UpdateLayeredWindowIndirect` dengan `prcDirty`;
- turunkan update rate overlay ke 30 Hz saat tidak ada gerakan (opsional).

### 3.3 Window & geometri (P0)

```cpp
SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

HWND game = FindWindowW(gameClass, nullptr);       // atau EnumWindows + PID game
RECT rc; GetClientRect(game, &rc);
POINT org{ 0, 0 }; ClientToScreen(game, &org);
int W = rc.right, H = rc.bottom;                   // <-- dipakai W2S dan swapchain
SetWindowPos(overlay, HWND_TOPMOST, org.x, org.y, W, H, SWP_NOACTIVATE | SWP_SHOWWINDOW);
```

- Tangani `WM_SIZE`, `WM_MOVE`, `WM_DISPLAYCHANGE`, `WM_DPICHANGED`; recreate swapchain/texture bila ukuran berubah.
- Cari window game lewat PID (`GetWindowThreadProcessId`) atau class `UnrealWindow`, bukan `FindWindow` pertama (launcher juga punya window).
- Monitor non-primary: koordinat client tetap benar karena `ClientToScreen`.
- Jangan memakai `SM_CXSCREEN` untuk W/H proyeksi.

### 3.4 Frame pacing

- `timeBeginPeriod(1)` atau `CreateWaitableTimerEx(..., CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, ...)` untuk scan/sleep presisi.
- Present vsync; `DXGI_PRESENT_DO_NOT_WAIT` bila ingin tidak blocking; tangani `DXGI_STATUS_OCCLUDED`.
- `ImGuiIO::DeltaTime` dari `QueryPerformanceCounter`, bukan asumsi 16 ms.

### 3.5 ImGui draw (P1)

- `ImGui::GetBackgroundDrawList()` untuk ESP; jangan buat window per player.
- Skeleton: `AddPolyline` per chain (spine, lengan kiri/kanan, kaki kiri/kanan) → ~6 call/player, bukan 40+ `AddLine`.
- Teks: `AddText` dua kali (shadow/outline) atau background `AddRectFilled`; `CalcTextSize` untuk ukuran; `snprintf` ke stack buffer.
- Joints 24 titik opsional; glow tebal hanya untuk musuh; kurangi alpha.
- `io.IniFilename = nullptr` (jangan tinggalkan `imgui.ini`).
- `io.Fonts` atlas sekali; hindari rebuild font per frame.

---

## 4. Data pipeline (P0/P1)

### 4.1 Player scan

Sekarang: iterasi `ULevel.Actors` (bisa 1000+ actor) + fallback daftar level, filter nama kelas, lalu `PlayerState`.

Best practice:
- `UWorld` → `GameState` → `PlayerArray` (`0x4A8`, kandidat terverifikasi saat riset) → `APlayerState*` (30–60) — jauh lebih sedikit dari seluruh actor.
- `APlayerState.PawnPrivate` (kandidat `0x330`) → pawn; validasi balik `Pawn.PlayerState == ps`.
- Cache `pawn/ps/mesh/boneArray` per player; revalidasi tiap scan (pointer bisa berubah saat respawn).
- Validasi objek: `ClassPrivate`/`NamePrivate` + nama kelas (`Character`/`Pawn`), bukan hanya `ValidPtr`.
- Baca `IsBot` (`0x39E`) dan `TeamId` (`0x66C`) — sekarang `p.bot=0` dan `p.team` tidak dipakai overlay.
- `localCamp`/`camp` konsisten: `CAMP_TRUE 0x660` untuk match team; `CAMP_ALT 0x670` layout lobby.

### 4.2 Bones: batch + index asli

Sekarang (`FillBones`): untuk tiap bone, 3× `R<float>` (x,y,z) → ~240 read/player; 30 player = 7.200 read/scan; 25 Hz = 180k read/s. `p.bones[n]` memakai `n` yang hanya increment untuk bone valid — jika ada satu bone invalid, semua index hardcoded di overlay bergeser.

Best practice:

```cpp
uint8_t raw[80 * 0x30];
if (!ReadMem(raw, boneArray, sizeof(raw))) return;   // 1 read/player
for (int i = 0; i < cnt && i < 80; ++i) {
    float lx, ly, lz;
    memcpy(&lx, raw + i * 0x30 + 0x10, 4);
    memcpy(&ly, raw + i * 0x30 + 0x14, 4);
    memcpy(&lz, raw + i * 0x30 + 0x18, 4);
    if (!FiniteF(lx) || !FiniteF(ly) || !FiniteF(lz)) continue;
    p.bones[i] = WorldFromLocal(q, t, lx, ly, lz);    // index asli i
}
p.boneCount = cnt;
```

- Simpan hanya **24 bone yang digambar** (mapping di `HANDOFF.md` §3) di `SPlayer` → shm turun dari ~140 KB ke ~40 KB, copy overlay 3× lebih murah.
- Transform: `ComponentToWorld` (`0x210`) = quat (0x00) + translation (0x10) + scale; pastikan scale dipakai bila ≠ 1.
- Validasi `cnt` 60..100, `BoneArray` aligned, `qn` 0.8..1.2.

### 4.3 Shared memory

- Seqlock sekarang benar (`InterlockedIncrement` sebelum/sesudah `MMove` state..cmd). Tambahkan **double buffer** bila ingin menghilangkan retry `Snapshot`:

```cpp
struct Shm { SharedState buf[2]; volatile LONG idx; };
// writer: salin ke buf[1-idx], MemoryBarrier, InterlockedExchange(&idx, 1-idx)
// reader: i = idx; salin buf[i]; jika idx == i -> valid
```

- `Publish` hanya menyalin `state..cmd`; `cmd` ditulis overlay — pertahankan pemisahan ini.
- `SharedState` besar karena `players[128]×bones[80]` dan `dbg[64]`; kecilkan (24 bone, `dbg` hanya build probe).
- Overlay memakai `OpenFileMapping` (read/write untuk `cmd`), bukan `CreateFileMapping` yang bisa membuat mapping kosong bila core belum jalan.
- Nama shm/menu: acak per build/sesi + ACL; `DFC6_menu_v1` saat ini plaintext.
- `static_assert(sizeof(SharedState))`, `#pragma pack` konsisten, `SHM_VERSION` dicek.

### 4.4 Prediction

Sekarang: kamera + posisi player + **semua bone** diekstrapolasi (`p.bones[b] += delta`), clamp 0.04 s / 40 unit, smoothing 0.65/0.35. Ini sudah bagus; tambahan:
- `p.dist` dihitung ulang dari posisi prediksi (sekarang dari scan).
- Prediksi hanya untuk musuh yang bergerak; matikan saat `selfDead`/menu.
- Jangan prediksi saat snapshot baru saja datang (dt terlalu kecil) — sudah dijaga.

---

## 5. Visual ESP (P1)

### 5.1 Box

Sekarang: `feet = p.pos`, `head = p.pos.z + 72`, `bw = h * 0.45`. Itu tidak mengikuti bone.

Best practice (pilih salah satu):
1. **Bounds 3D** — `FBoxSphereBounds` komponen (`UPrimitiveComponent.Bounds`: origin + box extent), 8 corner → transform `ComponentToWorld` → proyeksikan 8 titik → min/max layar. Box stabil, tidak terpengaruh pose.
2. **Head/foot bone** — `p.bones[32]` (head) dan bone kaki terendah (`58..65`) → tinggi; lebar dari shoulder (`5`/`33`) → `|x5-x33|`. Lebih murah, tetap akurat.
3. Fallback: bone min/max Y/Z semua bone valid.

Hindari `h * 0.45` hardcoded dan `p.pos.z + 72`; keduanya sumber box meleset saat crouch/jump/ADS.

### 5.2 Skeleton

- Mapping indeks sudah ada (HANDOFF); pertahankan.
- Chain: `1-2-3-4-5-31-32` (spine→head), `5-6-7-8-9` (lengan), `5-33-34-35-36`, `1-58..61`, `1-62..65` (kaki).
- `AddPolyline` per chain; glow hanya musuh; joints opsional.
- Warna: lokal cyan, musuh merah, unknown kuning; alpha konsisten; opsi colorblind.

### 5.3 Name / distance / team / bot

- Name: `AddText` shadow/outline + background; `p.name` sudah dibersihkan dari `BP_`/`Default__`.
- Distance: `m` dari posisi prediksi; `snprintf` ke stack; jangan `wsprintfA` per frame tanpa perlu.
- Team: `camp` (0x660) + `team` (0x66C); filter "enemy only" pakai `localCamp`; tag `BOT` dari `IsBot` (0x39E).
- `p.bot` saat ini selalu 0 — isi dari `IsBot`.

### 5.4 Health / visibility / radar (belum ada)

- **Health**: offset belum terverifikasi di repo. Kandidat: `PlayerState`/`Pawn` → `AbilitySystemComponent` (`GAS`) → `AttributeSet` Health/MaxHealth. Verifikasi dengan probe (`dfhud`/`dfchars`) sebelum dipakai; health bar = `AddRectFilled` + border.
- **Visibility**: external read-only tidak bisa `LineOfSightTo` tanpa memanggil fungsi game (risiko). Best practice: **skip**; atau heuristic kasar (jarak + FOV), jangan klaim akurat.
- **Radar**: murah dan informatif — `p.pos` relatif `st.selfPos`, rotasi `camRot.yaw`, range 100–300 m; `AddCircle` + `AddTriangle` lokal + dot musuh; filter team.

### 5.5 FOV circle

`g_aimFov` dalam pixel layar; radius konsisten dengan FOV aim di world:
```
radiusPx = tan(aimFovWorld/2) / tan(camFov/2) * (H/2)
```
Simpan `aimFovWorld` di `MenuCfg`, bukan hanya pixel, supaya resolusi berubah tidak mengubah perilaku.

### 5.6 Text & stream-proof

- Font: satu atlas, ukuran 13–16; `AddText` shadow gelap 1 px.
- `WDA_EXCLUDEFROMCAPTURE` menyembunyikan overlay dari screen capture (ACE screenshot) — **trade-off**: screenshot desktop user juga tidak menampilkan overlay; `shot.exe` (PrintWindow game) memang tidak menangkap overlay.
- `PrintWindow` game tetap untuk frame bersih; desktop BitBlt untuk screenshot overlay.

---

## 6. Aim (P1)

Sekarang: overlay `AimTick` pilih target terdekat crosshair dalam `g_aimFov`, head bone 32, kirim `AimCmd` absolute yaw/pitch; core `AimLoop` hitung delta, clamp `maxStep`, jitter, tulis `RotationInput` (mode 0) atau `ControlRotation` (mode 1).

Best practice:
- **Mode 0 (`RotationInput` 0x570) sebagai default** — jalur input game, paling stealth; mode 1 hanya untuk debugging.
- Target selection: FOV, jarak, tim, `selfDead`, dan **visibilitas** (jika tidak ada, batasi jarak/FOV); hysteresis 100–150 ms (sudah ada 120 ms).
- Smoothing manusiawi: `maxStep` 0.2–0.5°/tick (8 ms), jitter kecil, berhenti saat delta < 0.1° (sudah).
- Jangan menulis saat target mati/pindah; reset `AimCmd.enable=0` (sudah).
- **Hapus `VirtualProtect` per write**: `RotationInput` ada di `.data` yang sudah RW. `WriteMem` sekarang selalu `VirtualProtect` — vektor deteksi `NtProtectVirtualMemory`. Cukup `MMove` langsung; `VirtualProtect` hanya fallback bila page read-only.
- `ControlRotation` write (mode 1) lebih terlihat; hindari di produksi.
- Prediksi target: pakai `g_draw` (sudah prediksi); jangan aim ke posisi scan yang lag.

---

## 7. Anti-cheat / stealth (P0/P1)

### 7.1 Injeksi & core

- `SYSTEM + early` (watcher sebelum game) — sudah; detail `docs/INJECTION.md`.
- Manual map: PE headers dihapus; section RW→RX; tidak ada `LoadLibrary`/`Ldr` entry.
- Thread: satu worker; `NtSetInformationThread(ThreadHideFromDebugger)`; `SetThreadDescription` nama generik; hindari `CreateThread` dari `DllMain` bila bisa (pakai `TpAllocWork`/thread pool — `ThreadFallback` sekarang).
- Read: internal `MMove` + `__try`; validasi `ValidPtr` + nama kelas; tidak ada `ReadProcessMemory` handle eksternal (menghindari `ObRegisterCallbacks`).
- Write: hanya aim (`RotationInput`), bukan ESP; tanpa `VirtualProtect`; frekuensi 8 ms.
- Jangan panggil fungsi game (`LineOfSightTo`, `ProjectWorldToScreen`) dari core — memicu hook/stack walk.

### 7.2 Overlay

| Sekarang | Risiko | Ganti |
|---|---|---|
| `WH_KEYBOARD_LL` (F9) | global hook, deteksi ACE | `RegisterHotKey(hwnd, id, MOD_NOREPEAT, VK_F9)` + `WM_HOTKEY`, atau polling `GetAsyncKeyState` edge |
| `DFImGuiOverlay` / `"DF Overlay"` | string plaintext | class/title acak (`kOverlayCls` sudah ada tapi tidak dipakai) |
| `DFC6_menu_v1` | nama mapping plaintext | XOR/acak, `OpenFileMapping` |
| `ovimg.log`, `aim.log`, `body.txt`, flag `aim2.flag`/`menu.flag`/`spawn_shot.flag` | artefak forensik | matikan di release; path acak |
| `SetForegroundWindow(g_hwnd)` saat menu | steal focus, game input | `WS_EX_NOACTIVATE` + mouse capture; keyboard via polling |
| `CreateFileMapping` di overlay | membuat mapping kosong | `OpenFileMapping(FILE_MAP_READ|FILE_MAP_WRITE)` |
| `imgui.ini` default | file artefak | `io.IniFilename = nullptr` |
| overlay topmost/layered terlihat | ACE window scan | `WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE`; `WDA_EXCLUDEFROMCAPTURE` (trade-off) |

### 7.3 Checklist rilis

- [ ] `WH_KEYBOARD_LL` dihapus.
- [ ] `VirtualProtect` aim dihapus.
- [ ] Class/title/menu shm/log/flag random atau off.
- [ ] `imgui.ini` off.
- [ ] `OpenFileMapping` + ACL + nama acak.
- [ ] `IsBot`/`team`/health/radar (fitur) tidak menambah write.
- [ ] `SYSTEM+early` + `docs/INJECTION.md` dipatuhi.
- [ ] Uji 1 sesi penuh: tidak ada kick/crash; `ovimg.log` bersih.

---

## 8. Performance budget

| Metrik | Sekarang (estimasi) | Target |
|---|---|---|
| Scan core | 25 Hz, ~180k read/s (per-bone) | 25–30 Hz, ~750 read/s (batch), `scanUs` < 1000 µs |
| Publish | ~140 KB × 25 Hz | ~40 KB × 25 Hz |
| Overlay | 8.3 MB copy + 2M px alpha/frame; `Sleep(16)` | GPU present; 60–144 fps; CPU < 3% |
| Draw | ~70 call/player + 24 joint | ~6–10 call/player (`AddPolyline`) |
| Aim | 8 ms tick | 8 ms; write hanya saat target |
| Latency | scan 40 ms + present | scan 33 ms + present 1 frame |

Ukur: `QueryPerformanceCounter` di core (`scanUs`, `readCount`, `publishUs`) dan overlay (`fps`, `drawUs`) → kirim di `SharedState`/log; `PresentMon`, `ImGui` metrics, Task Manager/Process Explorer.

---

## 9. Stabilitas

- `FillBones`: index asli `p.bones[i]`, `cnt` 60..100, `ReadMem` batch return false → `boneCount=0`.
- `W2S`: `isfinite` pada semua input; `fovUse` 30..140; `tan > 0`; `fwd > 1`; `H/W > 0`; clamp box ke layar.
- `PosOK`/`FiniteF` sudah ada — pakai juga untuk `p.pos.z + 72` dan bone.
- `ReadFStringToBuf`: batas 64, null-terminate, sanitasi nama; fallback class name.
- `Snapshot`: cek `magic`/`version`/`state`/`playerCount`; `seq` retry; `g_haveState` false → overlay tidak menggambar.
- `AimLoop`: `g_pcShared` valid, `cmd` timeout 150 ms, `maxStep` clamp, `Sleep(8)`.
- `DllMain`: `ThreadFallback`/`Tp`; jangan alokasi berat di loader lock.
- Exception: `SetUnhandledExceptionFilter` + minidump di build debug; log versi/offset.

---

## 10. Testing / verifikasi

1. **Kamera**: `camMode` harus 1/4 (bukan 2) saat POV tersedia; bandingkan W2S overlay vs `ProjectWorldToScreen` game pada 5 titik; error < 2 px; uji ADS/zoom/recoil.
2. **Render**: `PresentMon`/`ImGui` fps; CPU overlay; tidak ada `UpdateLayeredWindow` full-frame di profil.
3. **Data**: `scanUs`, `readCount`, `playerCount`, `boneCount` di log; bandingkan sebelum/sesudah batch.
4. **Aim**: `aim.log` delta; tidak ada snap; target pindah > 120 ms; mode 0.
5. **Stealth**: `inj_early.log` (`map game st=0`, `loader_status=1`), `ovimg.log` (`STATE`, `DRAW`), tidak ada kick; cek hook/string/log.
6. **Visual**: screenshot `shot.exe` (game) + desktop capture (overlay) untuk box/skeleton/name.

---

## 11. Prioritas implementasi

### P0 (hari ini)
1. `core.cpp`: `if (camMode == 0 && ValidPtr(localPawn))` — fallback tidak menimpa POV.
2. `core.cpp`: validasi/skor POV + urutan kandidat → auto-scan → fallback; log `camMode`.
3. `core.cpp` + `overlay`: matriks view-projection di `SharedState`; W2S pakai matriks; hapus offset manual.
4. `overlay_imgui.cpp`: `GetClientRect`/`ClientToScreen`/DPI-aware; W/H client.
5. `overlay_imgui.cpp`: swapchain/DirectComposition (atau dirty-rect) — hapus full-frame readback + `Sleep(16)`.
6. `overlay_imgui.cpp`: `RegisterHotKey`/polling; hapus `WH_KEYBOARD_LL`.
7. `core.cpp`: `WriteMem` aim tanpa `VirtualProtect` (page RW).
8. `overlay_imgui.cpp`: random class/title; `io.IniFilename=nullptr`; `OpenFileMapping`.

### P1
1. `FillBones` batch + index asli + 24 bone; `SPlayer` kecil.
2. `GameState.PlayerArray` + `PawnPrivate`; cache/validasi; `IsBot`/`team`.
3. Box dari head/foot bone atau bounds; hapus `p.pos.z+72`.
4. `p.dist` dari posisi prediksi; name shadow/outline.
5. Seqlock double buffer; `scanUs`/`fps` metrics.
6. Aim: target selection + humanisasi + `RotationInput`; radar; health (verifikasi).

### P2
1. Visibility (skip/limitasi) — jangan panggil game.
2. `WDA_EXCLUDEFROMCAPTURE` (trade-off).
3. External read-only (tanpa DLL) untuk ESP-only; bandingkan deteksi/perf.
4. Menu `WS_EX_NOACTIVATE` tanpa steal focus.

---

## 12. Referensi

- UE5: `FMinimalViewInfo`, `FSceneView::ProjectWorldToScreen`, `FBoxSphereBounds`, `FTransform`, `FNamePool`, `USkeletalMeshComponent`, `APlayerState`, `AGameStateBase`.
- ImGui: `ImDrawList::AddPolyline`, `ImGui_ImplDX11`, `GetBackgroundDrawList`.
- DXGI/DWM: `CreateSwapChainForComposition`, `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT`, `DwmExtendFrameIntoClientArea`, `DwmGetWindowAttribute`, `SetWindowDisplayAffinity`.
- Anti-cheat: `docs/INJECTION.md`; `ObRegisterCallbacks`; `SeDebugPrivilege`; `SYSTEM+early`.
- Repo: `core.cpp`, `overlay_imgui.cpp`, `shared.h`, `Offsets.hpp`, `HANDOFF.md` §3/§9.
