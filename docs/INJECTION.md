# INJECTION — kenapa `NtMapViewOfSection` ditolak (`0xC0000022`)

**Gejala:** injector `dfsecinject.exe` membuka proses game (`open pid ... err=0`), tapi manual-map gagal:

```
map game st=0xC0000022
map self st=0xC0000022
EXIT=4
```

`0xC0000022` = `STATUS_ACCESS_DENIED`. Ini **bukan** bug offset/shm — ini masalah **token/privilege + timing + proteksi anti-cheat**.

---

## 1. TL;DR

| Jalur | Token | Hasil |
|---|---|---|
| `dfsecinject.exe` dari shell SSH/user biasa | Medium integrity, tanpa `SeDebugPrivilege` | `map game st=0xC0000022` ❌ |
| `dfsecinject.exe` via `schtasks /ru SYSTEM /rl HIGHEST` (watcher, **saat game baru start**) | `NT AUTHORITY\SYSTEM` + `SeDebugPrivilege` | `map game st=0x00000000`, `loader_status=1` ✅ |
| Injeksi lambat (AC sudah hardening) | SYSTEM pun bisa ditolak | `0xC0000022` |

**Kesimpulan:** bedanya bukan "SSH vs agent", tapi **SYSTEM + early vs user + late**.

---

## 2. Root cause

1. **Handle rights + anti-cheat callback.** Proses game diproteksi; driver AC memasang `ObRegisterCallbacks` dan men-strip hak seperti `PROCESS_VM_OPERATION`/section-map dari handle non-SYSTEM. `OpenProcess` bisa sukses (`err=0`), tapi `NtMapViewOfSection` ke target tetap `STATUS_ACCESS_DENIED`.
2. **Privilege.** `SeDebugPrivilege` (dan `SeTcbPrivilege` untuk `WTSQueryUserToken`) hanya efektif pada token SYSTEM/elevated. Shell agent/user medium tidak punya.
3. **Timing.** Injeksi yang berhasil dilakukan **begitu `DeltaForceClient-Win64-Shipping.exe` muncul** (watcher). Setelah AC selesai inisialisasi/hardening, map ke proses yang sama bisa ditolak walau SYSTEM.
4. **Target proses.** Inject ke `DeltaForceClient-Win64-Shipping.exe` (game), **bukan** launcher `DeltaForceClient.exe`. PID launcher ≠ PID game; `find_game_pid.bat` hanya mencari shipping exe.
5. **Session.** Injector boleh jalan di session 0 (SYSTEM). **Overlay** harus di **session 1** (session game) — beda masalah.

---

## 3. Bukti log

**Gagal — direct dari shell user (inj29/inj30 lama):**
```
section st=0x00000000 size=lu
open pid=33172 h=00000000000000D8 err=0
map game st=0xC0000022 base=0000000000000000 vs=lu
map self st=0xC0000022 base=0000000000000000
EXIT=4
```

**Sukses — watcher SYSTEM saat game start:**
```
GAME_PID=35656
open pid=35656 h=00000000000000DC err=0
map game st=0x00000000 base=0000000004DA0000 vs=lu
map self st=0x00000000 base=0000021119410000
loader_status=1
EXIT=6
```

> `EXIT=6` **bukan** berarti gagal. Patokan sukses: `map game st=0x00000000` dan `loader_status=1`.

---

## 4. Prosedur yang benar (watcher SYSTEM + early)

`watch_inject.bat` (auto-PID via `find_game_pid.bat`, inject **sekali** begitu shipping exe muncul):

```bat
:: 1) arm watcher sebagai SYSTEM — SEBELUM game dibuka
schtasks /create /tn df_watch /tr "C:\Users\User\trackA\watch_inject.bat" /sc once /st 23:59 /ru SYSTEM /rl HIGHEST /f
schtasks /run /tn df_watch

:: 2) buka game lewat launcher
::    D:\Delta Force\Delta Force\DeltaForceClient.exe
```

Sekali jalan saat game sudah hidup:

```bat
schtasks /create /tn dfa_inj /tr "C:\Users\User\trackA\injsys.bat" /sc once /st 23:59 /ru SYSTEM /rl HIGHEST /f
schtasks /run /tn dfa_inj
```

Detail script:

| Script | Fungsi | Log |
|---|---|---|
| `find_game_pid.bat [image]` | cari PID shipping exe → `GAME_PID.txt` (baris1=count, baris2=pids) | stdout |
| `watch_inject.bat [dll] [timeout]` | tunggu game start, inject sekali; default `dfcore30.dll`, timeout 0 | `inj_early.log` |
| `injsys.bat [dll]` | inject ke game yang sudah jalan (auto-PID) | `inj_sys.log` |
| `inj30.bat [dll]` | inject ke game yang sudah jalan; **wajib SYSTEM** | `inj30.log` |

- `TRACKA_DIR` = folder build host (default `%USERPROFILE%\trackA`).
- Args opsional butuh wrapper bila lewat `schtasks` (path tanpa spasi aman):
  `watch_inject.bat dfcore30.dll 0` / `injsys.bat dfcore30.dll`.

---

## 5. Verifikasi

```
inj_early.log / inj_sys.log : map game st=0x00000000 ... loader_status=1
                              FOUND ... pids=... / TOTAL_EXIT=6
ovimg.log                   : shm v30 state=1 players=N
                              STATE st=1 act=... ply=... camp=... team=...
```

`TOTAL_EXIT=6` / `DONE rc=6` **bukan** kegagalan — exit code injector setelah map sukses. Parse log, bukan exit code.

---

## 6. Checklist troubleshooting

- [ ] `whoami /groups` → **System Mandatory Level** (bukan Medium). Medium = map pasti ditolak.
- [ ] Task SYSTEM + Highest: `schtasks /query /tn df_watch /v /fo LIST` → `Run As User: SYSTEM`, `Run Level: Highest`.
- [ ] Inject **awal** (watcher sebelum game). Game sudah lama jalan → restart via launcher, arm watcher lagi.
- [ ] Target = **`DeltaForceClient-Win64-Shipping.exe`**, bukan launcher.
- [ ] `map game st=0x00000000` + `loader_status=1` → sukses; abaikan `EXIT=6`.
- [ ] Masih `0xC0000022` walau SYSTEM+early: AC hardening/blacklist → restart game, rename injector, atau ganti vektor.
- [ ] Overlay **session 1** (game): `StartOverlay.bat` / `start_overlay.flag` → core `EnsureOverlay`. `sessrun` error `1314` = `SeTcbPrivilege`, bukan penyebab map denied.
- [ ] Jangan inject berulang ke proses yang sama (duplikat Worker/shm); satu core per proses.

---

## 7. File terkait

| File | Fungsi |
|---|---|
| `dfsecinject.exe` / `dfsecinject.cpp` | injector manual-map (`NtCreateSection`/`NtMapViewOfSection`) |
| `find_game_pid.bat` | auto-PID shipping exe → `GAME_PID.txt` |
| `injsys.bat`, `inj30.bat` | inject satu kali (panggil via task SYSTEM) |
| `watch_inject.bat` | watcher SYSTEM + early inject |
| `StartOverlay.bat` | start overlay di session game |
| `HANDOFF.md` §9 | ringkasan batasan anti-cheat |
