# ESP JITTER/BLINK RESEARCH — Delta Force (UE5 + ACE)

> Snapshot kode: `3fab951` (core30 + overlay v31.2). Fokus: kenapa ESP (box/skeleton/name) bergerak atau blink padahal skeleton dunia diam. Riset + bukti kode + praktik komunitas luar.

---

## 0. TL;DR

**Skeleton dunia diam, tapi ESP bergerak karena kamera bergerak.** ESP memproyeksikan koordinat dunia lewat kamera; kamera punya micro-motion (view bob, sway, recoil, `ViewPunch`, ADS) dan **cache kamera yang dipakai overlay bukan POV render saat ini**. Selisih kamera = selisih proyeksi = ESP terlihat goyang/drift relatif ke model.

Akar utama (ranked):
1. **Sumber kamera salah/terlambat** — `CameraCache`/kandidat 0x30870 bisa **satu frame di belakang**; POV render saat ini ada di `CameraCachePrivate` (UE: `LastFrameCameraCachePrivate` = one frame old). Overlay memakai kandidat/last-frame → ESP lag.
2. **`SanitizeCam` menyamakan `camMode==2`** — `camMode=2` bisa berarti `CameraCache` (ci=1) **atau** fallback `controlRot+selfPos+65`. Fallback tidak mengandung `ViewPunch`/shake/bob → ESP beda dengan model.
3. **`ViewPunch`/`CameraShake`/ADS** — POV berubah tanpa `ControlRotation`; fallback tidak mengikuti → box/skeleton bergeser relatif ke model saat menembak/ADS.
4. **Prediksi** — default `g_predMs=0` (OFF), jadi bukan penyebab sekarang; kalau diaktifkan, prediksi **kamera** menambah overshoot.
5. **Posisi player** — `p.pos` dari translasi mesh (bisa ikut animasi/root motion); box fallback `p.pos+72`.
6. **Blink** — `Snapshot`/state/W2S gagal sesaat + `UpdateLayeredWindow` full-frame; `g_lastValid` 600 ms menutupi sebagian, bukan semua.

---

## 1. Fakta kode (yang relevan)

### Overlay `overlay_imgui.cpp`

| Bagian | Perilaku | Implikasi |
|---|---|---|
| `SanitizeCam()` | `camMode != 2` → reject; rebuild `selfPos.z+65`, `camRot=controlRot`, `fov=90` | `camMode=2` ambigu: `CameraCache` (ci=1, 0x30870) **atau** fallback. Fallback tanpa view punch |
| `W2S()` | client rect `g_cx/g_cw`; `g_camZ/g_camFwd/g_yawOff/g_pitchOff/g_fovAdj/g_fovAds`; `fwd < 1` reject | Offset manual = kalibrasi; sisa error = drift |
| `UpdatePrediction()` | `g_predMs=0` default (OFF); jika ON: kamera + player + semua bone diekstrapolasi | Prediksi kamera = sumber jitter kalau diaktifkan |
| `DrawESP()` | box head `bones[32]`, feet `58..65`, fallback `p.pos+72`; skeleton bones; `h > H` skip; `g_lastValid` 600 ms | Box/skeleton ikut kamera; fallback box ikut `p.pos` |
| `Snapshot()` | spin 64, `YieldProcessor`; last-good; `g_snapMiss` | Blink jika seq terus ganjil / state invalid |
| `PresentLayered()` | full readback 8.3 MB + `UpdateLayeredWindow`; alpha loop sudah dibuang | Flicker/lag DWM; bukan GPU swapchain |

### Core `core.cpp`

| Bagian | Perilaku | Implikasi |
|---|---|---|
| `povs[5]` | ci=0 `0x2C5E0` → mode 1; **ci=1 `0x30870` (`CameraCache`) → mode 2**; ci=2..4 `0x2DEC0/0x2DEF0/0x2F310` → mode 3..5 | `camMode=2` = CameraCache, **bukan** fallback; fallback juga menulis 2 |
| fallback | `if (camMode == 0 && ...)` → `selfPos+65`, `controlRot`, `fov=90`, `camMode=2` | Sama-sama mode 2; overlay tidak bisa membedakan sumber |
| `FillBones` | 1 `ReadMem`/player; `p.bones[i]` index asli; `boneCount` | Data bone benar; bukan penyebab jitter |
| `p.pos` | translasi mesh (`mesh+0x220`) | Bisa ikut animasi mesh; root actor lebih stabil untuk posisi |
| `Publish` | seqlock `state..cmd`, 25 Hz | Update posisi 40 ms; overlay 110–130 fps → butuh interpolasi |

---

## 2. Akar masalah (detail)

### A. Camera cache: current vs last frame

UE `APlayerCameraManager` menyimpan:
- `CameraCachePrivate` — cached camera properties (current).
- `LastFrameCameraCachePrivate` — cached camera properties, **one frame old**.
- `GetCameraCachePOV()` → `CameraCachePrivate.POV`; `GetLastFrameCameraCachePOV()` → `LastFrameCameraCachePrivate.POV` (deprecated → `GetCameraCacheView`).

Jika overlay membaca kandidat 0x30870 yang merupakan **last frame**, ESP selalu **1 frame di belakang** model. Saat kamera diam, tidak terlihat; saat kamera bergerak sedikit (bob/ADS/recoil), ESP tertinggal → "gerak-gerak walaupun dikit".

**Best practice komunitas:** pakai POV **paling baru** (`CameraCachePrivate`/`GetCameraCachePOV`), atau interpolasi `LastFrame → Current` dengan `TimeStamp`; jangan memakai last-frame untuk render.

### B. `SanitizeCam` dan ambiguitas `camMode==2`

- Core: `camMode=2` = `CameraCache` (ci=1) **atau** fallback (`controlRot+selfPos+65`, fov 90).
- Overlay: `SanitizeCam` menerima `camMode==2`; hanya reject mode lain / jarak >3000 / fov <60.
- Fallback **tidak** mengandung `ViewPunch`, `CameraShake`, view bob, ADS offset → ESP tidak sama dengan model.
- Jika `CameraCache` valid, ESP memakai POV asli (bagus) tapi last-frame; jika tidak, fallback (lebih jauh dari model).

**Best practice:** bedakan sumber (flag `camSource`: 0 none, 1 cache, 2 fallback); fallback hanya jika tidak ada POV; jangan menimpa POV valid dengan fallback.

### C. `ViewPunch`, `CameraShake`, ADS, recoil

- POV = `ControlRotation` + `ViewPunch` + `CameraShake` + `CameraAnim` + ADS offset + bob.
- Fallback hanya `ControlRotation` → saat firing/ADS, kamera render bergerak, ESP tidak (atau sebaliknya) → drift.
- `ViewPunch`/shake **tidak** menggeser bullet (itu `ControlRotation`), tapi **menggeser proyeksi** — jadi ESP yang memakai POV akan ikut shake; yang memakai fallback tidak. Keduanya harus konsisten dengan **render**.

**Best practice:** gunakan POV render (termasuk punch/shake) untuk ESP; jangan mencampur fallback untuk sebagian frame.

### D. Prediksi

- `g_predMs=0` default; jika di-ON, `UpdatePrediction` memprediksi **kamera** + player + semua bone.
- Prediksi kamera dari snapshot 25 Hz → velocity noise → overshoot → jitter.
- Komunitas: prediksi hanya **posisi player** (translasi), kamera pakai POV terbaru; interpolasi posisi antar snapshot, bukan ekstrapolasi kamera.

### E. Posisi player: mesh vs root

- `p.pos` = translasi mesh; animasi/root motion bisa menggeser mesh sedikit walau actor diam.
- Box fallback `p.pos+72`; skeleton pakai bone (benar).
- **Best practice:** posisi player dari `RootComponent`/actor location; box dari `FBoxSphereBounds` (8 corner) atau head/foot bone; jangan campur mesh translation untuk box.

### F. Box/skeleton

- Box head `bones[32]`, feet `58..65` — sudah benar.
- `FBoxSphereBounds` (UPrimitiveComponent.Bounds) lebih stabil untuk box (tidak ikut pose ekstrem).
- Skeleton: `BoneArray` + `ComponentToWorld`; pastikan scale/quat; bone index asli sudah.

### G. Blink

- `Snapshot` seqlock 140 KB; 64 spin; last-good; `g_snapMiss`.
- `DrawESP` `g_lastValid` 600 ms + `cHold`.
- Player `W2S` gagal (`fwd < 1`, `h > H`, NaN) → player tidak digambar.
- `UpdateLayeredWindow` full-frame + DWM; `DwmFlush` bisa membuat frame pacing tidak rata.
- **Best practice:** double-buffer snapshot; interpolasi; `Present(1,0)`/vsync atau swapchain; jangan toggle `GetAsyncKeyState` tiap frame (edge detection).

### H. W2S/client/DPI

- `W2S` sudah client rect; pastikan overlay window juga mengikuti client (posisi/ukuran), DPI-aware (`SetProcessDpiAwarenessContext`), dan `GetClientRect` diperbarui saat resize/move.
- Jika overlay 1920×1080 tetapi client game beda (windowed/125%), box bergeser saat kamera bergerak.

### I. Player order

- `Level.Actors` order bisa berubah; `g_pp` prediksi per index bisa mismatch → player tertentu "loncat".
- **Best practice:** `GameState.PlayerArray` (stable) + key by `pawn`/`PlayerState`, bukan index.

---

## 3. Praktik komunitas luar (ringkasan sumber)

| Sumber | Poin |
|---|---|
| Epic UE docs — `APlayerCameraManager` | `CameraCachePrivate` = cached current; `LastFrameCameraCachePrivate` = one frame old; `GetCameraCachePOV` → current, `GetLastFrameCameraCachePOV` → last frame |
| GuidedHacking — "Viewmatrix changes value randomly" | Masalah view matrix/ESP; solusi di thread: pastikan sumber matrix stabil/terbaru, bukan offset yang berubah |
| GuidedHacking — "Glitchy ESP" / "ESP Problem" | ESP teleport/resize/lines liar: posisi/float/validasi entity; box dari data yang stabil |
| GuidedHacking — "dx11 flickering imgui menu" | Flicker menu karena `GetAsyncKeyState` toggle tiap frame → pakai edge detection |
| ImGui DX11 example | `Present(1,0)` vs `Present(0,0)`; vsync mempengaruhi flicker/tearing |
| `DwmFlush` (dwmapi) | Blocks until next present; dipakai untuk pacing overlay layered |
| critical-cheats — "ESP With Delay (lag)" | Overlay external bisa lag karena prioritas render; saran vsync/lock FPS |
| cs2-external-esp | External ESP GDI/Discord overlay; box sederhana stabil |
| UE camera notes / TranslatedWorld / interpolated frame | Camera VP interpolation & float precision bisa menambah jitter; pakai POV terbaru & interpolasi posisi |
| Kanxue DWM overlay | DWM `COverlayContext::Present`/swapchain untuk overlay; hindari full-frame layered readback |

**Konsensus:** jitter ESP hampir selalu **kamera/view matrix** (sumber/last-frame/fallback), bukan entity; blink hampir selalu **render path** (`UpdateLayeredWindow` full-frame, toggle, vsync) atau **state invalid sesaat**.

---

## 4. Test plan (A/B, ukur bukan feeling)

Metrik yang sudah ada di kode: `jMax`/`jLocMax` (delta rotasi/lokasi kamera per detik), `cDrawn`/`cHold`/`cOff`, `g_camReject`, `g_snapMiss`, `DRAW`/`JIT`/`STATE` di `ovimg.log`.

1. **Prediksi OFF vs ON** (`g_predMs=0` vs 20/40 ms): bandingkan `jMax`/`jLocMax` dan gerak box relatif model.
2. **Sumber kamera**: log `camMode`, `camLoc`, `camRot`, `controlRot`, `camSource`; bandingkan `CameraCache` vs `CameraCachePrivate` vs fallback (tambahkan probe `dfcam4` jika perlu). Cari offset `CameraCachePrivate` di sekitar 0x30870; cek `TimeStamp`.
3. **ViewPunch**: saat firing/ADS, bandingkan POV vs `ViewPunch`; pastikan ESP memakai POV render.
4. **Box**: `bones[32]/58..65` vs `FBoxSphereBounds`; ukur box height/posisi per frame.
5. **Player pos**: `p.pos` mesh vs `RootComponent`; ukur delta saat idle/animasi.
6. **Blink**: hitung `g_snapMiss`, `cOff`, `cHold`, `DRAW` miss; A/B `UpdateLayeredWindow` vs swapchain/`Present`; A/B `DwmFlush`.
7. **W2S/client**: windowed 1600×900, 125% DPI, monitor kedua; cek box tetap di model.
8. **Order**: log urutan player; uji `GameState.PlayerArray` vs `Level.Actors`.

---

## 5. Prioritas perbaikan

### P0
1. **Sumber kamera**: baca POV **terbaru** (`CameraCachePrivate`/`GetCameraCachePOV`), bukan last-frame; tambah `camSource` (cache/fallback) dan `TimeStamp`; fallback hanya jika tidak ada POV; jangan menimpa POV valid.
2. **Sinkronkan dengan `ViewPunch`/shake**: ESP memakai POV render; fallback tidak dipakai saat POV ada.
3. **Box/posisi**: root actor + `FBoxSphereBounds`/head-foot bone; hapus fallback `p.pos+72` bila bone ada.
4. **Blink**: double-buffer/interpolasi posisi; snapshot state; `Present`/vsync/swapchain; edge detection hotkey.

### P1
1. Interpolasi posisi player antar snapshot (25 Hz → 110 fps); kamera tetap POV terbaru.
2. `GameState.PlayerArray` (stable) + key by pawn; hilangkan index-based prediction.
3. `SetProcessDpiAwarenessContext` + update geometry saat resize/move.
4. Skeleton `AddPolyline` per chain; kurangi draw call.

### P2
1. Swapchain/DirectComposition untuk overlay (hilangkan readback full-frame).
2. `WDA_EXCLUDEFROMCAPTURE` (trade-off screenshot).
3. External read-only (tanpa DLL) untuk ESP-only.

---

## 6. Sumber

- Epic UE docs — `APlayerCameraManager` (CameraCachePrivate/LastFrameCameraCachePrivate): https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/APlayerCameraManager
- Epic UE docs — `GetCameraCachePOV`: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/Camera/APlayerCameraManager/GetCameraCachePOV
- Epic UE docs — `GetLastFrameCameraCachePOV`: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/Camera/APlayerCameraManager/GetLastFrameCameraCachePOV
- GuidedHacking — Viewmatrix changes value randomly: https://guidedhacking.com/threads/viewmatrix-changes-value-randomly.16380/
- GuidedHacking — Glitchy ESP: https://guidedhacking.com/threads/glitchy-esp.9010/
- GuidedHacking — ESP Problem: https://guidedhacking.com/threads/esp-problem.15442/
- GuidedHacking — dx11 flickering imgui menu: https://guidedhacking.com/threads/dx11-flickering-imgui-menu.14979/
- ImGui DX11 example — Present(1,0)/vsync: https://github.com/ocornut/imgui/blob/master/examples/example_win32_directx11/main.cpp
- Microsoft — DwmFlush: https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmflush
- critical-cheats — ESP With Delay (lag): https://critical-cheats.gitbook.io/critical/loader-errors/esp-with-delay-lag
- cs2-external-esp (GDI/Discord overlay): https://github.com/IMXNOOBX/cs2-external-esp
- UE camera notes: https://tizengyan.github.io/2025/03/23/ue_note_camera/
- TranslatedWorld/float precision: https://blog.csdn.net/qqQQqsadfj/article/details/161398650
- Interpolated frame/camera VP jitter: https://banming.github.io/GameEngine/Unreal/optimize/interpolated-frame-high-fps.html
- Repo: `overlay_imgui.cpp` (`SanitizeCam`, `W2S`, `UpdatePrediction`, `DrawESP`, `PresentLayered`), `core.cpp` (`povs[5]`, fallback, `FillBones`, `p.pos`), `docs/ESP-BEST-PRACTICES.md`, `docs/ANTI-RECOIL-RESEARCH.md`.
