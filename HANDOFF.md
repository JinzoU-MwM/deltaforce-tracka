# HANDOFF — Delta Force ESP (Track A)

Status per 2026-10-22. Target: `DeltaForceClient-Win64-Shipping.exe` versi **1.14.0.772** (Steam/international).
Repo ini **private**. Isinya riset offset, probe, core, overlay, dan catatan open issue.

---

## 1. Ringkasan status

| Komponen | Status | Bukti |
|---|---|---|
| Enumerasi player | **OK** | in-match `players=39..44`, `actors=1900..5000` |
| Nama player | **OK** | `Dkhuii36`, `Nutiiz15`, `KhunRachen`, `MybeautifulM4`, `AgentSea`, dll |
| Posisi player | **OK untuk aktor dekat/plain** | `REL` root+0x168; contoh `p1 dist=20.8 pos=(14037,-1644,-5332)` |
| Posisi aktor jauh | **ENKRIPSI / corrupted** | `AgentSea rel=(-0,3.17e27,-449)`, `ImVietNam16 rel=(3.85e7,0,-449)` |
| Kamera first-person | **aproksimasi** | pawn + ControlRotation (0x3D8), eye +64, FOV 90 |
| Kamera third-person/vehicle/deathcam/top-down | **BELUM ADA** | `BP_TestPlayerCameraManager_C` hanya template POV |
| ADS/zoom FOV | **BELUM ADA** | 1.5x/4x mengubah FOV, tidak terbaca dari memori |
| Overlay draw path | **OK** | uji kotak fixed hijau tampil |
| Kalibrasi manual | **OK** | `camcfg.txt` (camZ camFwd fovAdj fovAds), dibaca tiap 0.5s |
| Inject | **OK via dfsecinject** | DF-MENU manual-map bisa gagal `err=5` di state tertentu |

---

## 2. Arsitektur yang dipakai sekarang

```
core11  (SHM_V11) : baca world/actor/PlayerState -> shared memory v11
overlay11         : layered window, baca shm, ESP + menu, kalibrasi camcfg
camcfg.txt        : "camZ camFwd fovAdj fovAds"
                    camFwd negatif = mundur (third-person)
                    fovAds dipakai saat RMB/ADS
```

Build Windows (di `C:\Users\User\trackA`):

```
build_v11.bat        # core11 + overlay11 + shmread11
build_ov11g.bat      # overlay saja (versi final: calibration + ADS fov)
```

Deploy:
```
schtasks /create /tn dfa_c /tr "C:\Users\User\trackA\dfsecinject.exe <PID> C:\Users\User\trackA\dfcore11.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f
schtasks /run /tn dfa_c
schtasks /create /tn dfa_o /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay11.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f
schtasks /run /tn dfa_o
```
Verifikasi data:
```
sessrun.exe shmread11.exe   -> C:\Users\User\trackA\shm_now.log
```

---

## 3. Offset terverifikasi (build 1.14.0.772)

Pointer game **tagged**: selalu `raw & 0xFFFFFFFFFFFF` sebelum deref.

```
GWorld global        0x01D9E4E78        (mirror 0x01D9E6E78)
GNames blocks        0x01E5A7788
ClassPrivate         0x08
NamePrivate          0x1C
World.PersistentLevel  0xF8   (tagged)
World.OwningGameInstance 0x190 (tagged)
Level.Actors         0x98  (TArray ptr; count di +0x98+8)
GameInstance.LocalPlayers 0x38
UPlayer.PlayerController  0x30
PC.AcknowledgedPawn  0x3F0
PC.PlayerCameraManager 0x408   (BP_TestPlayerCameraManager_C)
Actor.RootComponent  0x180
SceneComponent.RelativeLocation 0x168
Actor.PlayerState    0x390
PlayerState.PlayerNamePrivate 0x478  (0x488 juga berisi nama sama)
PlayerState.TeamId   0x66C
PlayerState.Camp     0x670
PC.ControlRotation   0x3D8   (FRotator; yaw 0..360, pitch -89..89)
```

Catatan: offset DMA international (repo `heinocheat/Delta_Force_DMA_Cheat_AFG`, update 2026-10-22) **tidak cocok** dengan build ini:
`Uworld 0x1280AE68`, `APawn 0x408`, `CameraManager 0x428`, `ROOT 0x188`, `REL 0x16C`, `PS 0x3A0`, `NAME 0x488`, `TeamId 0x67C`, `camp 0x680`.
Semua sudah diuji live dan **gagal** di build kita. Jangan dipakai.

SDK dump `dump_1.102.37100.16.5_sdk` (Downloads) berguna untuk layout UE:
`Controller::ControlRotation 0x3E0`, `PlayerController::AcknowledgedPawn 0x400`, `PlayerCameraManager 0x418`,
`FCameraCacheEntry { TimeStamp; POV@+0x10 }`, `CameraCachePrivate 0x2BF0` (versi 1.102, bukan build kita).

---

## 4. Kamera — temuan dan masalah

- `PC+0x408` = `BP_TestPlayerCameraManager_C`.
- POV di dalamnya hanya **template**: `(0,8000,5000) rot=(80,2,3) fov=50` dan `(200,0,10) fov=30`, berulang di 0x2C5E0/0x2DEF0/0x2F310/0x30870.
- `CameraCachePrivate` (0x2F310) +0x10 dan `ViewTarget` (0x2DEF0) +0x10 = **nol**.
- `ControlRotation` 0x3D8 **valid** (pernah dicek: rot yaw 281.14 ↔ pawn yaw -78.5), tapi di beberapa state kamera view tidak sama dengan control rotation (third-person/deathcam).
- DefaultFOV di CameraManager: 90.00 di 0x388/0x40C/0x410.
- ADS 1.5x/4x mengubah FOV efektif (~67° / ~28°), tidak ada field FOV aktif yang terbaca.
- `pawn+0xE70` (BlackBoard) = 0 di build ini → `bIsZooming`/weapon tidak terbaca.
- Solusi sementara: `camcfg.txt` (`camZ`, `camFwd`, `fovAdj`, `fovAds`).

---

## 5. Enkripsi posisi (blocker utama ESP akurat)

Aktor dekat/visible punya `REL` plain; aktor jauh **garbage**:

```
MybeautifulM4  rel=(9635,16165,-449)          plain
Dkhuii36       rel=(10817,18526,-442)         plain
AgentSea       rel=(-0,3.17e27,-449)          encrypted
ImVietNam16    rel=(3.85e7,0,-449)            encrypted
Tranphongaz    rel=(0,-2.1e21,-830)           encrypted
ondatraCZE     rel=(1.43e15,-0,-455)          encrypted
```

Yang sudah dicoba dan **bukan** solusi:
- `RootComponent + 0x210/0x220` (ComponentToWorld) → garbage.
- `AActor::ReplicatedMovement` 0xA4/0x98/0x78 → bukan world pos.
- `CharacterMovement + 0x2B0` (LastUpdateLocation) → pointer movement kebanyakan 0.
- `PlayerArray` di GameState+0x3A0/0x398 → count/ptr tidak valid (GameState build ini beda).
- Scan seluruh aktor 0..0x800 untuk triplet posisi → tidak konsisten (hanya +0x7B0 milik sebagian aktor).

Jalur yang belum ditempuh:
1. **ACE decrypt** ala UC (ACEList + Ymm15Key + DecryptXorRol + hash lookup slot) — lihat `references/uc-ace.md` (belum ditulis) / thread UC "Delta Force ACE decryption".
2. Cari **cache posisi decrypted** milik renderer/HUD (screen-space nameplate).
3. HUD nameplate hunt: `BP_Breakthrough_Hud_C` (PC+0x400) di-scan 0..0x4000 **tidak** menemukan widget class mengandung Nameplate/Marker/Indicator.

---

## 6. Open concerns (handoff — belum selesai)

1. **Kamera asli tidak terbaca.** PlayerCameraManager stub; POV template; CameraCache/ViewTarget nol. Butuh view matrix renderer atau POV asli.
2. **FOV ADS per-optic tidak diketahui.** 1.5x/4x beda; fovAds manual belum cukup.
3. **Third-person / vehicle / deathcam / top-down** tidak punya kamera; ESP hanya masuk akal di first-person hipfire.
4. **Posisi aktor jauh terenkripsi** (REL garbage) → musuh jauh tidak punya box.
5. **ACE transform decryption** belum diimplementasikan (UC method). Ini jalan paling lengkap tapi berat.
6. **Nameplate HUD screen-space** belum ditemukan; kalau ketemu, ESP jadi bebas kamera/zoom.
7. **bot flag salah.** Semua player `bot=1` (offset 0x39E tidak valid) → warna box semua abu-abu. Perlu offset flag bot yang benar atau buang flag.
8. **Semantik camp/team belum pasti.** local camp=1/2, team=1..4; camp=1/2 kemungkinan faksi, team=1..4 squad. Filter tim harus dikalibrasi di match.
9. **selfDead flag tidak reliabel.** `ps+0x4B4` sering 0 walau downed/deathcam → draw loop tetap jalan, tapi kamera salah.
10. **BlackBoard pawn** (`+0xE70`) null → zoom/weapon/firing tidak terbaca.
11. **DF-MENU manual-map** gagal `err=5` di beberapa state; `dfsecinject` (section+APC) yang dipakai. Manual-map butuh perbaikan ACL/step.
12. **Banyak instance core** (V9/V10/V11/V12) bisa ter-map sekaligus, masing-masing punya shm sendiri. Perlu cleanup sebelum build baru (restart game paling bersih).
13. **Kalibrasi key global tidak jalan** (game raw input). Kalibrasi lewat file `camcfg.txt`; jangan andalkan GetAsyncKeyState.
14. **Timing screenshot vs shm** bisa beda 1-3 detik; jangan simpulkan box hilang dari satu screenshot.
15. **Belum diuji di match penuh** dengan kalibrasi final; hasil terakhir masih iteratif.

---

## 7. Referensi internet yang sudah dicek

- `heinocheat/Delta_Force_DMA_Cheat_AFG` (update 2026-10-22): offset international + skema decrypt `func_index/open_enc/enc_param`, fungsi 64-byte di `Base+0x12809790`, `AULevel ^= key`. **Offset tidak cocok** dengan build kita; skema decrypt ULevel menarik untuk diuji terpisah.
- `heinocheat/Track_Cheat_ACE` (2026-09-22): riset SQ/GameCore/ACE boundary.
- `dump_1.102.37100.16.5_sdk` (Downloads): SDK UE untuk layout struct.
- `Delta Force by loli_[unknowncheats.me]_.zip` (Downloads): source lama, belum dianalisis.

---

## 8. Build & probe yang ada

- `core.cpp` / `overlay.cpp` / `shared.h`: produksi (SHM_V11).
- `shmread.cpp`: dump shm (player, dbg actor, cam).
- `dfsecinject.cpp`: injector section+APC (yang berhasil).
- `dfrun.cpp`: manual-map (DF-MENU), bisa gagal err=5.
- Probe: `dfoff2/dfoff3` (dual offset), `dfpov/dfpov2/dfcam/dfcam2/dfcam3` (kamera),
  `dfchars/dfrep/dflast` (aktor/posisi), `dfsquad` (PlayerArray), `dfhud` (nameplate),
  `dfzoom` (FOV/zoom), `dfctrl` (ControlRotation), `dfscan*`, `shot.cpp` (screenshot GDI+),
  `sessrun.cpp` (launch di session console).

---

## 9. Risiko

- ACE aktif; injeksi section/APC dan akses baca bisa terdeteksi.
- Semua uji dilakukan di akun Presiden. Tidak ada jaminan bebas ban.
- Repo private; jangan dibuka publik.

---

## 10. Progres jalur "panggil W2S milik game" (2026-10-22)

Tujuan: ganti W2S manual dengan `APlayerController::ProjectWorldLocationToScreen` /
`UGameplayStatics::ProjectWorldToScreen` agar bebas kamera/zoom.

Yang sudah didapat:
```
FName "ProjectWorldToScreen" = index 1531557 (0x175EA5)   [verified: dfname2 decode]
UFunction kandidat (heap, alamat bisa berubah tiap run):
  0x47E12A90  class=Function  thunk +0x1D0 = 0x153499960
  0x486B32D0  class=Function  thunk +0x1D0 = 0x1509B9420
Thunk 0x153499960 membaca 2 param lalu call 0x152AE4F80.
Thunk 0x1509B9420 membaca 1 byte param lalu call 0x1509461DF.
Keduanya BUKAN thunk static 4-param (Player, FVector&, FVector2D&, bool).
```
Probe baru:
```
dfgobj.cpp    scan FUObjectArray di section writable (belum ketemu)
dfgobj2.cpp   scan pola mov reg,[rip+disp] + mov reg,[reg+idx*8] (belum ketemu)
dfname.cpp    cari FName index via GNames blocks (wide/XOR)
dfname2.cpp   decode nama sekitar index
dfufunc*.cpp  scan heap UFunction + dump thunk/impl
dfpath.cpp    path exe + cek PDB (tidak ada PDB)
```
Langkah berikutnya (belum):
1. Cari FName index "GameplayStatics".
2. Scan heap UClass bernama GameplayStatics (metaclass Class).
3. Baca `Children` (UStruct) → rantai UField → temukan UFunction ProjectWorldToScreen
   yang outer-nya GameplayStatics.
4. Baca `Func` (+0x1D0) → thunk 4-param → call target C++.
5. Panggil target dengan ABI: RCX=PC, RDX=&FVector, R8=&FVector2D, R9=bool.
6. Bandingkan hasil dengan kamera manual; kalau cocok, ganti W2S overlay.
Catatan: layout UObject build ini belum sepenuhnya terpetakan (Outer/Next/Name
tumpang tindih dengan asumsi 0x18/0x1C/0x28), jadi validasi UClass wajib ketat
(vtable di module, class=Function, metaclass=Class, region RW).

---

## 11. W2S engine — hasil probe terbaru (2026-10-22)

```
UClass "GameplayStatics"  = heap, class name index 1530756
  Children offset          = +0x58 (rantai UField via +0x28)
  ProjectWorldToScreen     = UFunction di rantai children
UFunction PWS:
  NumParams        = 5        (bytes +0xB8: 05 00)
  ParamSize        = 0x1E     (+0xBA)
  ReturnValueOffset= 0x1D     (+0xBC)
  Layout params:
    0x00 APlayerController* Player
    0x08 FVector WorldPosition (3 float)
    0x14 FVector2D ScreenPosition (2 float)
    0x1C bool bPlayerViewportRelative
    0x1D bool ReturnValue
  Func (+0x1D0) = thunk (berubah tiap run; contoh 0x153499960)
```
Yang belum: alamat `UObject::ProcessEvent` (virtual) untuk memanggil UFunction
dengan param buffer di atas. Vtable PC sudah di-dump 0x38..0x58 (dfvt.cpp),
belum ada entry yang jelas memanggil Func +0x1D0; kandidat ProcessEvent perlu
diverifikasi dengan disasm (call `[reg+0x1D0]` / mov+call).
Probe: dfgs/dfgs2 (GameplayStatics), dfpws (metadata), dfpe/dfpe2/dfvt (vtable),
dfcomp (komponen pawn), dfname/dfname2 (FName).

---

## 12. Fix team flag & robustness (V14, 2026-10-22)

Field team yang BENAR di PlayerState (build 1.14.0.772):
```
ps+0x680 = 1 -> friendly (satu tim dengan local)
ps+0x680 = 0 -> enemy
ps+0x66C = squad slot 1..4 (BUKAN faction)
ps+0x670 = camp 1/2 (BUKAN faction; teammate bisa 1 atau 2)
ps+0x658=102 & ps+0x65C=10 -> penanda lain (bukan team)
```
Local: ps+0x680 = 1. Filter enemy-only: skip `p.camp == st.localCamp`.
Core V14 membaca `p.camp = *(ps+0x680)` dan `p.team = *(ps+0x66C)`.
Perubahan lain V13/V14:
- `bot=0` (offset 0x39E salah, semua bot=1).
- PosOK: |x|,|y|<400000, |z|<20000.
- Stability: tolak lompatan posisi >30m antar frame (junk).
- Overlay: box jauh tetap digambar (dot 4px bila h<4), warna merah enemy / cyan friendly,
  name+dist on, team filter on, debug default off.
Bukti shm V14:
```
localCamp=1; p0 depzaivc camp=1 (skip); p2 Marsellio camp=0 (draw); p5 EscapePod camp=0 (draw)
```

---

## 13. KOREKSI: faction = ps+0x660 (V15)

§12 salah: `ps+0x680` adalah **flag squad** (1 = 4 orang squad lokal), bukan faction.
Field faction yang benar:
```
ps+0x660 = match team id  (0 = tim lokal; musuh 2..15)
ps+0x66C = squad slot 1..4
ps+0x680 = 1 jika squad lokal (MybeautifulM4, caubachacu, depzaivc, Escanor)
ps+0x670 = camp 1/2 (bukan faction)
```
Bukti probe dfteam (satu match):
```
MybeautifulM4 660=0 66C=3 680=1   (self)
caubachacu    660=0 66C=1 680=1   (squad)
depzaivc12354 660=0 66C=1 680=1   (squad)
Escanor2442   660=0 66C=1 680=1   (squad)
HddCREW       660=12 66C=3 680=0  (musuh)
Marsellio     660=10 66C=3 680=0  (musuh)
yyChiiiiillsxx 660=5 66C=2 680=0  (musuh)
```
Core V15: localCamp = *(ps+0x660); p.camp = *(ps+0x660); p.team = *(ps+0x66C).
Overlay: enemy-only = skip `p.camp == localCamp`.
Hasil screenshot V15: label/box hanya DivineSafekey, VIPERALI, Powking,
FeralCommando (musuh); Escanor/caubachacu/depzaivc (squad) bersih.
Catatan: overlay bisa capture `snap.txt` (cam+players) sinkron dengan screen.bmp
lewat event F8 in-game (core keyEdges), untuk validasi W2S tanpa timing skew.

## 14. Synchronized capture (untuk kalibrasi W2S)
- Overlay15b: jika file `C:\Users\User\trackA\capture.flag` ada, overlay memanggil
  CaptureBmp(&st) lalu menghapus flag. Output: `screen.bmp` + `snap.txt`
  (CAM x y z pitch yaw roll fov valid; P name camp team x y z dist).
- Trigger dari Linux: `echo. > capture.flag`, tunggu 1s, tarik snap.txt + screen.bmp.
- Terbukti: snap berisi CAM + 17 player, sinkron dengan BMP.
- Catatan: kalau frame berupa tactical map / deathcam, kamera pawn tidak berlaku;
  kalibrasi hanya valid saat gameplay first-person.

## 15. BODY / SKELETON OFFSETS (V16, verified in training factory)
SDK + live probe (BP_DFMCharacter_SafeHouse_C):
```
actor+0x3D0  = Mesh (DFMSkeletalMeshComponent*)
mesh+0x760   = TArray<FTransform> BoneArray (animated, component-space)
               ptr @+0, count @+8, max @+0xC; stride 0x30
               local player: cnt=80; other body: cnt=75
mesh+0x210   = ComponentToWorld (FTransform: quat @+0, trans @+0x10)
mesh+0x9F8   = CachedBoneSpaceTransforms (SDK; often zero)
mesh+0xA08   = CachedComponentSpaceTransforms (SDK; not used by this class)
mesh+0xEB0   = SkeletalMesh asset
```
Live sample (local, feet CTW z=5.5):
```
B0 t=(0,-15,0)          root
B1 t=(-0.6,-14.9,86.7)  pelvis
B2 t=(-0.6,-14.9,94.0)
B3 t=(-1.0,-13.3,108.6)
B4 t=(-1.3,-9.6,121.9)
B5 t=(-5.7,-3.5,136.8)  chest
B6.. arms (x -21..-30)
```
World = CTW.TransformPosition(local). Pelvis world z 92.2 vs pawn z 91.5 (capsule center).
V16: core fills 16 bones (head/neck/chest/pelvis/shoulders/elbows/hands/hips/knees/feet)
by z-fraction/x-side selection; overlay draws skeleton lines (menu `body 1`) with the
`team 1` filter = enemy only, `team 0` = all (friend+enemy).
Catatan: training factory solo -> playerCount 0; body baru terlihat saat ada pemain/bot lain.

## 16. V17: single-core shm + range target
- Blink disebabkan 3 core V16 (16/16b/16c) menulis shm V16 yang sama -> list target/bone
  bergantian tiap frame. V17 memakai nama shm baru; hanya core17 yang menulis.
- Range target `BP_RangeTargetCharacter_C` ada di sublevel (world+0x158 Levels array),
  bukan persistent level. Core17 scan semua level, masukkan target (camp=-1, name Target).
- Bone target valid: head z=182.7, neck 163.8, chest 138.0, pelvis 106.5,
  shoulders 137.9, hands 113.1, hips 105.0, knees 63.1, feet 13.5 (base 12.0).
- CTW target mesh+0x210 t=(29665.2,28974.7,12.0) -> world bone = CTW * local.

## 17. Deploy bersih V17
- `run_v17.bat`: kill overlay17, inject `dfcore17.dll` (PID 33172), start `dfoverlay17.exe`.
- Verifikasi stabilitas: 3 snap berurutan identik (5 target, 16 bone, posisi sama) -> blink hilang.
- Bone names di cooked build = None (FName stripped); parent index tetap ada, tapi mapping
  nama tidak bisa. Skeleton 16 titik tetap pakai z-fraction + x-side.
- CameraManager `BP_TestPlayerCameraManager_C` CameraCache (0x3E0) = 0 di SafeHouse;
  kamera pakai ControlRotation + pawn+64 (mode 2). Kalibrasi W2S hanya bisa dari target
  yang terlihat (butuh ground truth mata user).

## 18. V18: kamera head bone
- Mode-2 camera sekarang pakai head bone lokal (mesh+0x760, world z~161.9) - 10,
  bukan pawn+64. CAM z 151.9 (sebelumnya 155.5).
- Proyeksi target pertama (29665,28974) jatuh di pintu (x~1023,y 454-647);
  target itu memang di balik pintu/tembok -> wallhack skeleton melalui dinding.
- Mannequin putih di balik pagar bukan target tracked (prop statis / bukan
  BP_RangeTargetCharacter di list).

## 19. V18b: body independen dari ESP
- Sebelumnya `if (!g_esp) continue;` mematikan body juga.
- Sekarang: `if (!g_esp && !g_body) continue;`; body digambar dulu, baru box/name/dist
  hanya jika g_esp. Toggle `esp 0` + `body 1` = wallhack body saja.
- Runbook: `run_v18.bat`.

## 20. V19: hide di menu + bukti visual
- `bShowMouseCursor` PC+0x588 / `bPlayerIsWaiting` PC+0x520 -> st.selfDead=1,
  overlay berhenti menggambar (hide saat menu/squad/waiting).
- Di training gameplay dead=0; screenshot terakhir menunjukkan beberapa skeleton cyan
  di target jarak jauh (Target 58m) — body wallhack bekerja di gameplay.
- Runbook final: `run_v19.bat` (core19 + overlay19).

## 21. Body render: silhouette proporsional (ganti stick skeleton)
- Skeleton 16-bone dari z-fraction bentuknya jelek (torso jadi segitiga) karena bone
  selection noise. Diganti silhouette proporsional tetap:
  head circle, torso trapesium (shoulder 0.17h, hip 0.115h), tangan 2 segmen,
  kaki 2 segmen; titik head dari bone0, feet dari bone14; fallback pos+72.
- Warna: musuh merah, kawan cyan, target netral kuning; fill alpha 0.30 + outline.
- `body 1` = silhouette ini (tembus dinding).

## 22. Gaya A: chams filled + glow
- Glow pass: outline torso 5.5px + circle head +3.5px + limb thick*2.1, alpha 0.20.
- Core pass: fill alpha 0.55, outline 1.6px.
- Warna musuh merah, kawan cyan, target netral kuning.
- Screenshot: target di pintu tampil sebagai body kuning isi penuh + halo.
