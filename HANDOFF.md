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
