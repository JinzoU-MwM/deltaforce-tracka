# ANTI-RECOIL RESEARCH — Delta Force (UE5 + ACE)

> Snapshot kode: `5bc33ee` (core30 + overlay ImGui v31; `5bc33ee` = aim tidak selalu nyala + target body part + geometri client-rect). Temuan anti-recoil diverifikasi ulang di HEAD ini. Riset ini fokus pada mekanisme recoil, sumber data/offset, metode kompensasi, dan best practice stealth. Bukan implementasi — semua poin menunjuk file/fungsi/offset nyata atau sumber publik.

---

## 0. TL;DR

1. **Recoil Delta Force = 3 lapis**, jangan dicampur:
   - **Actual recoil** — menggeser `ControlRotation` lewat `RotationInput`/`AddPitchInput`/`AddYawInput`. Ini yang menggeser bullet.
   - **Visual recoil** — `PlayerCameraManager` `ViewPunch`/`CameraShake`/`CameraAnim`; menggeser kamera/tampilan, bukan arah bullet.
   - **Spread** — cone random senjata (`Spread` data); tidak bisa diprediksi, jangan klaim "no spread".
2. **Anti-recoil yang benar** = kompensasi **actual** lewat jalur input game (`RotationInput`), bukan menulis `ControlRotation` langsung, bukan menulis data senjata (`RecoilAmount`/`Spread`).
3. **Pola per-senjata** wajib: `ShotIndex` + `FireRate` + ADS + attachment; recoil deterministik bisa dipetakan di training range; random spread tidak.
4. **External macro = banned** — Garena secara eksplisit melarang AutoHotkey/Python emulation scripts, hardware macro, DMA/KMBox/Arduino, analysis tools, VM, remote/capture tools (RenderDoc/Nsight). Internal read-only + `RotationInput` adalah jalur yang paling tidak terlihat.
5. **Data yang belum ada di repo**: `Weapon`/`CurrentWeapon`/`WeaponData`, `RecoilAmount`/`Speed`/`Recovery`/`Horizontal`/`Vertical`, `Spread`, `ShotIndex`, `FireRate`, `LastFireTime`, `bFiring`, `Ammo`, `ViewPunch`/`CameraShake`. Perlu probe `dfrec`.
6. **Kode sekarang belum punya anti-recoil** — `AimLoop` hanya mengejar head target; recoil menggeser target. Next: pola + kompensasi di `AimLoop`, probe `dfrec` untuk data senjata.

---

## 1. Model recoil Delta Force

### 1.1 Actual vs visual vs spread

| Lapis | Apa yang bergerak | Sumber | Kompensasi |
|---|---|---|---|
| Actual | `ControlRotation` (aim/bullet) | `RotationInput` dari `AddPitchInput`/`AddYawInput` recoil | `RotationInput` inverse (game path) |
| Visual | Kamera/POV | `ViewPunch`, `CameraShake`, `CameraAnim` | Tidak perlu untuk bullet; hanya kosmetik |
| Spread | Arah bullet dalam cone | `Spread`/`SpreadCurve` random | Tidak bisa; pattern hanya untuk deterministik |

Catatan penting: banyak "no recoil" komersial menulis `RecoilAmount=0`/`Spread=0`/`ViewPunch=0`. Itu **game-data write** — paling mudah terdeteksi integrity check. Untuk ESP/aim yang stealth, kompensasi input lebih aman.

### 1.2 Data publik yang relevan (community research)

**Build/attachment (NGA, 9game):**
- "面板后坐力" (stat panel) vs "真实后坐力" (real recoil) — yang penting real recoil.
- Threshold real recoil pertama ~15 poin; di atas itu baru terasa; attachment:
  - Vertikal: 共振人体握把, RK-0 前握把, 沙暴垂直补偿器.
  - Horizontal: 密令斜角握把, K1 精英斜角握把, 堡垒水平补偿器, PBS 俄式消音.
  - Keduanya: X25U 斜侧战斗握把.
  - 骨架 (skeleton stock): 10 poin tapi "negative recoil" besar.
  - 多口径: menambah horizontal.
  - 轻语 (suppressor): +15 real recoil, bukan single-direction.
  - 激光: panel control 53; control semua senjata threshold sama.
- Fire stability (开火稳定性) dan control (操控) mempengaruhi pemulihan, bukan hanya kick awal.
- 屏息 (hold breath) = Shift saat ADS; mengurangi sway, bukan recoil.
- 点射/短连射 (burst) + 滑铲/侧移 (movement) mengurangi akumulasi recoil.

**Pattern & control (9game):**
- Recoil pattern = vertical climb + horizontal drift; arah bisa single-direction atau both.
- "压枪" manual = mouse ke bawah/berlawanan arah pattern; sensitivity/DPI/raw input menentukan hasil.
- Jarak/ADS/hipfire mengubah skala pattern.

**Script publik (untuk memahami teknik, bukan untuk dipakai):**
- `fatinghenji/DFNR` — AutoHotkey: dynamic recoil strength + random offset (humanisasi), fire-rate RPM, hold-breath Shift, kalibrasi "strength 5" di training range sampai pattern jadi titik.
- `LegendaryGaoZheng/Cross-Game-Recoil-Control` — C#/WPF: DXGI Desktop Duplication (GPU capture), GDI fallback, low-level mouse hook, `RegisterHotKey`, image recognition warna target, profil per-senjata, preview弹道, mode fixed/left-right, semi-auto, 31-switch. Klaim "bypass anti-cheat" **tidak terverifikasi**.
- `zeenden729/DeltaForce-RecoilOptimizer`, `ace-trump-tech/DeltaForce-OBS-Locker` — "no recoil"/OBS injection; **hati-hati**, banyak repo scam/malicious; jangan dijalankan.
- **Garena official prohibited list** (16/12/2024): hardware DMA/DMA bridges/KMBox/Fusers/screen splitters/synchronizers/USB boxes/hardware macros/Arduino; analysis software (CheatEngine, YDARK, PCHunter, Ollydbg, WinDbg, x32/64dbg, APIMonitor, ProcessHacker, ProcessMonitor); speed-hack; **emulation scripts (AutoHotkey, Python)**; VM (VMware/KVM/VirtualBox); not recommended: remote desktop/streaming (ToDesk/Sunlogin/TeamViewer/AnyDesk/Parsec/Moonlight), capture tools (RenderDoc/WinPix/Nsight/Intel GPA). Konsekuensi: error/suspensi akun.

Kesimpulan: **macro eksternal (AHK/Python/C# input) dan hardware macro berada di daftar larangan**; internal memory read + `RotationInput` adalah pendekatan yang berbeda dan lebih stealth, tapi tetap harus humanis.

---

## 2. UE5 internals (dasar kompensasi)

### 2.1 `APlayerController`

- `RotationInput` — `FRotator` input axis yang **diakumulasi tiap tick** (offset repo `0x570`).
- `ControlRotation` — `FRotator` rotasi kontrol aktual (offset repo `0x3D8`).
- `AddPitchInput(float Val)` / `AddYawInput(float Val)` — menambah `RotationInput.Pitch += Val * InputPitchScale`, `RotationInput.Yaw += Val * InputYawScale`.
- `UpdateRotation(DeltaTime)` — `ControlRotation += RotationInput`, lalu `RotationInput = FRotator::ZeroRotator`, `SetControlRotation` (detail persis tergantung versi UE; intinya input diakumulasi lalu diterapkan).
- `AimAt(FVector WorldLocation)` — menghitung rotasi dan `SetControlRotation` (jalur fungsi game; risiko return-address/stack jika dipanggil dari DLL).
- `PlayerCameraManager` (offset repo `0x408`).
- `AcknowledgedPawn` (`0x3F0`), `PlayerState` (`0x390`).

### 2.2 `APawn`/`ACharacter`

- `AddControllerPitchInput`/`AddControllerYawInput` → diteruskan ke `APlayerController`.
- `bUseControllerRotationPitch/Yaw` — apakah pawn mengikuti `ControlRotation`.
- `GetControlRotation`, `GetBaseAimRotation`, `GetActorEyesViewPoint` — sumber rotasi untuk animasi/aim.
- `Mesh` (`0x3D0`), `RootComponent` (`0x180`).

### 2.3 `APlayerCameraManager`

- `CameraCache`/`CameraCachePrivate` — `FCameraCacheEntry { FMinimalViewInfo POV; float TimeStamp; }`; kandidat offset repo `0x30870` (POV), `0x2C5E0`/`0x2DEC0`/`0x2DEF0`/`0x2F310` (kandidat lain).
- `ViewPunch` — rotasi punch kamera (biasanya `FRotator`), diolah `ProcessViewPunch`/`AddViewPunch`; **visual**, bukan `ControlRotation`.
- `CameraShake`/`CameraAnim` — shake visual; `StartCameraShake`/`PlayCameraAnim`.
- `UpdateCamera`/`UpdateViewTarget` — menghasilkan `POV` final yang dipakai render.

Implikasi: untuk anti-recoil yang menggeser bullet, baca/tulis `ControlRotation`/`RotationInput`. `ViewPunch`/`CameraShake` hanya mempengaruhi tampilan; jika ingin crosshair "diam" secara visual, itu masalah render, bukan aim.

### 2.4 Weapon/recoil (game-specific, belum terverifikasi)

Kandidat field yang perlu dicari lewat probe:
- `APawn`/`ACharacter`: `Inventory`, `CurrentWeapon`, `Weapon` (offset belum ada).
- `AWeapon`/`UWeaponComponent`: `WeaponData`, `FireRate`, `ShotIndex`, `LastFireTime`, `bFiring`, `Ammo`, `bAiming`.
- `UWeaponData`/`FWeaponData`: `RecoilAmount`, `RecoilSpeed`, `RecoilRecovery`, `RecoilHorizontalRange`, `RecoilVerticalRange`, `RecoilCurve`, `Spread`, `SpreadCurve`, `AimPunch`, `RecoilModifier`.
- `RecoilComponent`/`WeaponRecoilComponent` (jika ada): state recoil berjalan, `RecoilOffset`, `RecoilRecoveryTime`.
- `ViewPunch`/`CameraShake` di `PlayerCameraManager` (visual).

Nama kelas yang dicari di `dfscan`/`dfdump`: `Weapon`, `WeaponData`, `Recoil`, `Spread`, `Fire`, `Ammo`, `RecoilComponent`, `WeaponRecoil`, `WeaponSpread`, `DFWeapon`, `BP_Weapon`.

---

## 3. Kode yang ada sekarang (aim, belum anti-recoil)

`core.cpp` — `AimLoop()`:
- Baca `cmd` (seqlock) → `targetYaw/targetPitch`, `maxStep`, `mode`.
- Baca `ControlRotation` (`pc + 0x3D8`).
- `dY/dP` = target − current; clamp `maxStep` (0.02..6°); jitter ±0.0256°; skip 1/32 tick.
- `mode 0` (default): tulis `{dP+jp, dY+jy, 0}` ke `RotationInput` (`0x570`).
- `mode 1`: `ctrl.yaw += dY; ctrl.pitch += dP` lalu tulis `ControlRotation` (lebih terlihat).
- `WriteMem` selalu `VirtualProtect(PAGE_READWRITE)` → `MMove` → restore. **Vektor deteksi `NtProtectVirtualMemory`.**
- `Sleep(8)` per tick.

`overlay_imgui.cpp` — `AimTick()`:
- Target = player terdekat crosshair dalam `g_aimFov` (px), body part (5bc33ee; sebelumnya head bone 32).
- `AimBone`: 0=Auto, 1=Head(32), 2=Neck(5), 3=Chest(3), 4=Pelvis(1), 5=Legs(58/62).
- `g_fileTrig` default OFF (fix `aim2.flag` nyangkut yang membuat aim selalu nyala); `SanitizeCam()` menolak `camMode!=2`/jarak >3000/fov<60 dan rebuild dari `controlRot+selfPos`; FOV circle/aim center pakai client rect `g_cx/g_cy/g_cw/g_ch` (`UpdateGeometry`: `GetClientRect`+`ClientToScreen`).
- Filter team/maxDist; `lastSwitch` 120 ms; tulis `AimCmd` absolute yaw/pitch; `mode=0`.
- **Tidak ada kompensasi recoil** — saat firing, `ControlRotation` naik, crosshair/target bergeser; aim hanya mengejar head.

**Yang sudah benar:** `RotationInput` sebagai jalur default, `maxStep` clamp, jitter, timeout `cmd` 150 ms, `selfDead` dari scan.
**Yang perlu diperbaiki untuk anti-recoil:** pola per-senjata, kompensasi vertikal/horizontal, sinkron dengan `ShotIndex`/`FireRate`, tulis hanya saat firing, tanpa `VirtualProtect`, humanisasi yang tidak "perfect".

---

## 4. Metode anti-recoil (dari paling stealth ke paling berisiko)

### 4.1 Input-level compensation (`RotationInput`) — recommended
- Baca `ControlRotation` + `RotationInput` + `POV`/`CameraCache` per tick.
- Hitung delta recoil actual: `ControlRotation` sekarang vs posisi aim yang diinginkan (target/pattern).
- Tulis **inverse** ke `RotationInput` (jalur input game), bukan `ControlRotation`.
- Kelebihan: bullets mengikuti `ControlRotation`; tidak menulis game data; tidak butuh `AimAt`.
- Kekurangan: tetap `WriteProcessMemory` internal (`MMove`); harus presisi timing tick; `InputPitchScale`/`InputYawScale` mempengaruhi besaran.

### 4.2 Pattern per-senjata
- Rekam `ControlRotation` per shot di range: `shotIndex`, `dPitch`, `dYaw`, `timeSinceShot`, `bAiming`, attachment, stance, movement.
- Buat tabel pattern (vertikal/horizontal) per senjata/attachment/ADS.
- Saat firing: tambahkan inverse pattern ke `RotationInput` (di atas aim target).
- Untuk spread random: jangan klaim no-spread; pattern hanya mengurangi komponen deterministik.

### 4.3 `ControlRotation` direct — riskan
- Tulis `ControlRotation` langsung (`ctrl.yaw/pitch`) → instan, tapi lebih mudah terdeteksi (game state berubah di luar input) dan bisa dilawan `UpdateRotation`/server correction.
- Hanya untuk debug/benchmark; produksi pakai `RotationInput`.

### 4.4 Game-data write — paling terdeteksi
- `RecoilAmount=0`, `RecoilModifier=0`, `Spread=0`, `ViewPunch=0`, `bUseRecoil=false`.
- Efek "no recoil/no spread/no punch" instan, tapi integrity check/ACE bisa mendeteksi perubahan nilai.
- Untuk riset boleh; untuk produksi **hindari**.

### 4.5 External macro (AHK/Python/C#/hardware) — banned
- AHK/Python emulation scripts, hardware macro, DMA/KMBox/Arduino ada di daftar larangan Garena.
- Low-level mouse hook + `SendInput`/`mouse_event` = input injection; ACE bisa scan proses/hook.
- `RegisterHotKey`/`GetAsyncKeyState` (read-only) lebih aman daripada hook; tapi macro tetap macro.

### 4.6 Visual-only (`ViewPunch`/`CameraShake`)
- Jika tujuan hanya crosshair tidak goyang di layar, kompensasi `ViewPunch`/`CameraShake` (visual) bisa dipertimbangkan; tetapi bullet tetap mengikuti `ControlRotation`.
- Jangan campur: mengompensasi `ViewPunch` tanpa `ControlRotation` membuat crosshair dan bullet tidak sinkron.

**Rekomendasi:** 4.1 + 4.2 (input-level + pattern per-senjata), read-only measurement dulu, tanpa game-data write, tanpa macro eksternal.

---

## 5. Probe `dfrec` (yang perlu dibangun/dijalankan di laptop)

Tujuan: menemukan offset dan memetakan recoil.

### 5.1 Scan awal
- `GWorld` → `OwningGameInstance` → `LocalPlayers` → `PlayerController` (`pc`) → `AcknowledgedPawn` (`pawn`).
- Dari `pawn`: cari pointer ke `Weapon`/`CurrentWeapon`/`Inventory` (scan `pawn+0x0..0x2000` untuk pointer ke objek dengan nama kelas mengandung `Weapon`).
- Dari `Weapon`: `WeaponData`, `FireRate`, `ShotIndex`, `LastFireTime`, `bFiring`, `Ammo`.
- Dari `WeaponData`: `Recoil*`, `Spread*`, `AimPunch`.
- Dari `PlayerCameraManager`: `CameraCache` (`POV`+`TimeStamp`), `ViewPunch`, `CameraShake`.

### 5.2 Log per shot (`rec.log`)
Format yang disarankan:
```
t=... shot=... rpm=... ads=... stance=... move=...
ctrlPitch=... ctrlYaw=... rotInputPitch=... rotInputYaw=...
povPitch=... povYaw=... povFov=...
viewPunch=... recoil=... spread=... ammo=... firing=...
weapon=... data=...
```
- Catat `ControlRotation` sebelum/sesudah shot; `ViewPunch`; `POV`.
- Ulangi 20–30 shot per senjata: hipfire, ADS, attachment berbeda, stance (stand/crouch/prone), movement (still/strafe).
- Hitung pattern: `dPitch/dYaw` per shot; vertical/horizontal; recovery time; first-shot multiplier.

### 5.3 Existing probes yang bisa dipakai
`dfctrl`/`dfctrl2` (control rotation), `dfcam`/`dfcam2`/`dfcam3` (camera), `dfpov`/`dfpov2` (POV), `dfzoom` (FOV/zoom), `dfchars`/`dfhud` (HUD), `dfcomp` (components), `dfscan`/`dfdump`/`dfvt` (scan/vtable), `dfoff` (offset), `dfname` (class names), `dfgs`/`dfgobj` (objects), `dfufunc`/`dfcall` (functions). `dfrec` tinggal menggabungkan semuanya + log per shot.

---

## 6. Best practices anti-recoil

### 6.1 Akurasi
- Kompensasi harus berdasarkan `ControlRotation` actual (bukan `camRot`/POV yang kena `ViewPunch`).
- Sinkronkan dengan `ShotIndex`/`FireRate`: recoil terjadi saat shot, recovery setelahnya; jangan kompensasi terus-menerus.
- Per-senjata, per-attachment, per-ADS; pattern yang sama tidak berlaku untuk semua.
- Hitung `InputPitchScale`/`InputYawScale` — besaran `RotationInput` yang ditulis harus dikalikan skala game; kalibrasi di range.
- Jangan aim ke target saat firing jika target bergerak; prioritaskan kompensasi recoil lalu aim.

### 6.2 Stealth
- Tulis hanya `RotationInput` (game path), bukan `ControlRotation`, bukan game data.
- Tulis hanya saat firing (dari `bFiring`/`ShotIndex`/`LastFireTime`), bukan terus-menerus.
- Tanpa `VirtualProtect` per write: `RotationInput` di `.data` RW; `MMove` langsung; `VirtualProtect` hanya fallback.
- Tanpa macro eksternal (AHK/Python/C# input), tanpa hardware macro/DMA/KMBox.
- Tanpa `WH_KEYBOARD_LL`; pakai `RegisterHotKey`/polling.
- String/log/flag off di release; `DFC6_menu_v1`/window class/title random; `imgui.ini` off.
- Humanisasi: `maxStep` kecil, jitter, jeda, tidak "perfect zero"; pattern tidak 100% sama tiap shot.

### 6.3 Performa
- Baca `ControlRotation`/`RotationInput`/`POV` 1–2 read per tick (8 ms); jangan per-frame overlay.
- Pattern table di memori; tidak ada alokasi/`std::string` di loop aim.
- `Sleep(8)`/timer high-resolution; satu thread aim; prioritas normal/below normal.
- `dfrec` hanya di build probe; release tanpa log.

### 6.4 Verifikasi
- Range: wall/target, 30 shot, pattern; bandingkan `ControlRotation` delta vs pattern table.
- A/B: kompensasi on/off; error vertikal/horizontal; first-shot.
- Cek `RotationInput` yang ditulis vs `ControlRotation` yang terjadi (skala).
- Uji ADS/hipfire/attachment/stance/movement; uji senjata berbeda.
- Sesi penuh: tidak ada kick; `ACE` tidak bereaksi; `aim.log`/`rec.log` bersih.

---

## 7. Prioritas

### P0
1. `dfrec` probe: weapon/current weapon/data, recoil/spread, shot index/fire rate/ammo/firing, viewpunch/camerashake, control/rotation/POV.
2. Pattern table per-senjata (ADS/attachment/fire rate) dari range.
3. Kompensasi `RotationInput` di `AimLoop` (inverse pattern + aim target), tulis hanya firing.
4. Hapus `VirtualProtect` per write; `RotationInput` RW langsung.
5. Stealth: no macro/hardware, no game-data write, `RegisterHotKey`, strings/log off.

### P1
1. Spread/aim punch: read-only; jangan tulis game data.
2. ViewPunch/CameraShake: visual only; jangan campur dengan aim.
3. Per-weapon profile + operator/attachment; pattern table.
4. Fire-rate sync: `ShotIndex`/`LastFireTime`/`bFiring`.
5. A/B test + `dfrec` logging.

### P2
1. Game-data "no recoil" (RecoilAmount/Spread/ViewPunch) — riset saja, bukan produksi.
2. External macro/hardware — **no** (banned).
3. `AimAt`/`SetControlRotation` game function call — **no** (return address/stack).
4. Machine-learning pattern (opsional; butuh dataset besar).

---

## 8. Sumber

**Game/komunitas:**
- Garena Delta Force — prohibited software/hardware: https://deltaforce.garena.com/th/news/announcement/QZVAYD
- NGA — 大众化公式化改枪: https://ngabbs.com/read.php?tid=47599648
- NGA — 改枪思路分享: https://ngabbs.com/read.php?tid=42657035
- 9game — 三角洲行动怎么压枪: https://a.9game.cn/sjzxd/11672363.html
- Dotesports — SG552 build: https://dotesports.com/delta-force/news/best-sg552-build-in-delta-force
- GameRant — MP5 build: https://gamerant.com/delta-force-best-mp5-build-attachments-loadout/

**Script/cheat publik (referensi teknik; jangan dijalankan):**
- DFNR (AutoHotkey): https://github.com/fatinghenji/DFNR
- Cross-Game-Recoil-Control (C#/WPF): https://github.com/LegendaryGaoZheng/Cross-Game-Recoil-Control
- DeltaForce-RecoilOptimizer: https://github.com/zeenden729/DeltaForce-RecoilOptimizer
- DeltaForce-OBS-Locker: https://github.com/ace-trump-tech/DeltaForce-OBS-Locker

**UE5:**
- `APlayerController` — `RotationInput`, `ControlRotation`, `PlayerCameraManager`, `AddPitchInput`/`AddYawInput`, `UpdateRotation`, `AimAt`.
- `APlayerCameraManager` — `CameraCache`, `ViewPunch`, `CameraShake`, `UpdateCamera`.
- `APawn`/`ACharacter` — `AddControllerPitchInput`/`AddControllerYawInput`, `GetControlRotation`, `GetBaseAimRotation`.
- UE5.7 controller/pawn/camera notes: https://gitcode.csdn.net/69e6f7c054b52172bc6b28c4.html
- `APlayerController` property list (RotationInput): https://blog.csdn.net/ttod/article/details/134391885

**Repo:**
- `core.cpp` — `AimLoop`, `ControlRotation` 0x3D8, `RotationInput` 0x570, `WriteMem`/`VirtualProtect`.
- `overlay_imgui.cpp` — `AimTick`, `g_aimFov`, `g_aimStep`, `AimCmd`.
- `shared.h` — `AimCmd`, `SPlayer`, `SharedState`.
- `Offsets.hpp` — `PlayerController`, `PlayerCameraManager`, `APlayerState`.
- `docs/ESP-BEST-PRACTICES.md` §6 (aim), `docs/INJECTION.md`.

---

## 9. Status implementasi (2026-10-24, overlay v31.3 + core31)

| Butir | Status | Detail / bukti |
|---|---|---|
| §4.1 kompensasi via `RotationInput` | **done** | Overlay hitung `want = controlRot − pull×8ms`, kirim lewat `AimCmd`; core `AimLoop` (mode 0) menulis `RotationInput`. Tidak menulis `ControlRotation`, tidak menulis data senjata. |
| §6.1 dasar pada `ControlRotation` actual | **done** | `g_draw.controlRot` (bukan `camRot`/POV yang kena ViewPunch). Log: `REC ctrl=(359.62,265.95) want=(359.57,265.95) d=(-0.05,-0.00)` → 6 deg/s × 8 ms = 0.048°. |
| §6.1 tulis hanya saat firing | **done** | State machine `firing` dengan hysteresis 120 ms; opsi "Hanya saat menembak (LMB)" default ON. |
| §6.1 humanisasi | **done** | ramp 1.15× untuk 200 ms pertama (first-shot kick) lalu 1.0; clamp v ≤ 30 deg/s, h ≤ ±15 deg/s; jitter ±0.0256° tetap dari core. |
| §6.1 per-senjata / pattern | **belum** | Butuh `dfrec` (P0 #1) untuk `CurrentWeapon`/`ShotIndex`/`FireRate`. Untuk sementara: preset laju SMG 5/0.5, AR 7/1.5, LMG 9/2, DMR 4/0.5 (kalibrasi di range). |
| §6.1 InputPitchScale/YawScale | **diserap** | Slider deg/s berfungsi sebagai kalibrasi skala; tidak perlu offset baru. |
| §6.2 tanpa `WH_KEYBOARD_LL` | **done** | Hook dihapus; hotkey dibaca `GetAsyncKeyState` (INSERT / Ctrl+Alt+M) + jalur `keyEdges` dari core. `strings` binary: 0 kemunculan `LLKeyboard`. |
| §6.2 tanpa `VirtualProtect` per write | **done** | `WriteMem` kini fast-path `MMove` di dalam SEH; `VirtualProtect` hanya fallback bila halaman read-only. |
| §6.2 tulis hanya `RotationInput` | **done** | mode 0 default; mode 1 (`ControlRotation`) tidak dipakai overlay. |
| §6.2 log/flag off di rilis | **sebagian** | Toggle "Tulis log debug" (SETTINGS) → `OLog` jadi no-op. Nama shm/window/`DFC6_menu_v1` masih plaintext. |
| §6.3 performa | **done** | 1 snapshot + 1 tulis cmd per frame; tanpa alokasi di loop; `Sleep`/`DwmFlush` pacing. |
| §6.4 verifikasi | **sebagian** | Matematika kompensasi terverifikasi dari log; A/B recoil on/off di range **belum** (butuh sesi menembak). |

Guard robust yang dipasang (mencegah "asal jalan"):
`sstate==1 && camValid && !selfDead && !menu_open && game_foreground && (!fireOnly || firing)`, clamp laju, proteksi NaN, dan reset state saat trigger dilepas.

---

## 10. REVERT (2026-10-24) — anti-recoil dilepas

Atas keputusan operator, implementasi anti-recoil **dibuang dari overlay** karena **aim assist sudah menutupi kebutuhan**:

- Dihapus: blok recoil di `AimTick`, state `g_rec*` (enable/fireOnly/V/H/auto/learned), section "No recoil" di tab AIM, dan key `rec*` di `overlay_cfg.txt`.
- Bukti bersih: `grep -c "recOn\|recDP\|g_rec" overlay_imgui.cpp` → **0**; baris `JIT` baru tidak lagi memuat `rec=`.
- Tetap dipertahankan (tidak terkait fitur): `WriteMem` fast-path tanpa `VirtualProtect`, hotkey tanpa `WH_KEYBOARD_LL`, dan ramp/jitter aim yang sudah ada.

Yang **masih berguna** dari riset ini dan tetap ada di repo:

| Artefak | Fungsi |
|---|---|
| `dfrec.cpp` + `build_rec.bat` | probe read-only: delta `ControlRotation` per shot, `SHOT n`, scan kandidat weapon (`Weapon/Recoil/Spread/Ammo/Fire/Inventory`) |
| `scripts/analyze_rec.py` | ranking otomatis kandidat offset: AMMO (turun 1 + reset reload), SHOT INDEX, FIRE TIMER, RECOIL |

Temuan probe yang sudah tercatat (build 1.14.0.772, live):

```
pawn+0xD30  -> BP_WeaponModular_C
pawn+0x1088 -> WeaponAppearanceProxy  (0x10D8 saat dump)
pawn+0x1788 -> BP_WeaponModular_C     (objek senjata aktif)
WFLT objek aktif: 0xD8=1.0  0xDC≈879.8 (rpm?)  0x150=100  0x154=2  0x158=1
```

Watcher `CHG` pada rentang `0x0..0x600` menghasilkan **0 perubahan** saat menembak → stat recoil/ammo **tidak** berada di rentang itu pada objek tersebut (kemungkinan di komponen/asset terpisah, atau objek senjata runtime berbeda). Kalau fitur ini dihidupkan lagi, langkah pertama = perluas watcher (`0x0..0x2000`, dua level pointer) dan jalankan `analyze_rec.py`.

Kesimpulan teknis untuk masa depan: kompensasi paling aman tetap **input-level** (`RotationInput`), bukan tulis game-data; dan **jangan** memakai macro eksternal (daftar larangan Garena, §1.2).
