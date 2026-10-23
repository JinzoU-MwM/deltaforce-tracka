# Track A v3 — manual-map core + native-input aim

## Arsitektur
```
DF-MENU.bat  -> self-elevate -> DF-MENU.exe
DF-MENU.exe  -> cari DeltaForceClient-Win64-Shipping.exe
             -> ManualMap(dfcore.dll)   [utama, tanpa LoadLibrary]
             -> fallback LoadLibrary    [kalau manual map gagal]
             -> start dfoverlay.exe
dfcore.dll   -> read-only GWorld/player/camera -> shared memory (ver 3)
             -> aim worker 8 ms: tulis RotationInput 0x570
dfoverlay.exe-> layered window luar, baca shared memory, ESP + menu
```

## Manual map (map.h)
- Baca PE, alloc MEM_COMMIT|RESERVE, tulis header + section.
- Relokasi via .reloc (RVA -> file offset per section).
- Import resolve via nama (kernel32-only karena dfcore /MT).
- Section protection: RX / R / RW (bukan RWX).
- **RtlAddFunctionTable** dari .pdata (wajib untuk SEH x64).
- Entry dipanggil lewat stub (mov rcx=base, edx=1, r8=0, call entry).
- Header 0x1000 di-zero setelah init.
- TLS directory: tidak ada di dfcore (diverifikasi pefile) -> tidak perlu callback.

## Bukti uji (headless)
```
manualmap pid=... ok=1 err=0
shmread        magic=44464331 ver=3 seq=24 state=2
target alive   True          (SEH menangkap read invalid, tidak crash)
tasklist /m    dfcore.dll TIDAK muncul  (no PEB entry)
DF-MENU --test exit=0, shmread ver=3, no PEB entry
```
String audit tetap bersih (XOR 0x5A).

## Offset
```
GWorld 0x1D9E4E78 / mirror 0x1D9E6E78 / GNames 0x1E5A7788
PersistentLevel 0xF8, OwningGameInstance 0x190, Actors 0x98
LocalPlayers 0x38, PlayerController 0x30, AckPawn 0x3F0, CameraManager 0x408
PlayerState 0x390, PlayerName 0x478, Camp 0x664, CameraCache 0x3E0
ControlRotation 0x3D8, NetConnection 0x568, RotationInput 0x570 (inferensi)
pointer = raw & 0xFFFFFFFFFFFF
```

## Sisa permukaan deteksi
- CreateRemoteThread ke stub di memori unbacked (start address di luar module).
- VAD private-executable untuk image yang di-map manual (ACE kernel bisa scan).
- Named section handle di proses game.
- Statistik server untuk aim; modul/VAD untuk ESP.
- Belum diuji in-match.

## Pakai
1. Game sudah jalan (lobby/match).
2. Double-click `DF-MENU.bat` (UAC Yes) -> MessageBox hasil.
3. `INSERT` menu; masuk match untuk ESP; F8 toggle debug.
