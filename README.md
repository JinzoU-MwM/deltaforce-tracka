# Delta Force Track A — ESP + Aim (private)

Riset dan implementasi ESP/aim untuk `DeltaForceClient-Win64-Shipping.exe` **1.14.0.772**.
Repo public. Baca **[HANDOFF.md](HANDOFF.md)** untuk arsitektur, offset, shm, build, deploy, anti-cheat, dan known issues.

## Isi repo

| Path | Isi |
|---|---|
| `core.cpp` | core DLL (in-game): scan semua level, publish shm (V30/V31 via `/DSHM_Vxx`), kamera POV 11..16 / fallback 2, AimLoop `ROTATION_INPUT`, PollKeys, EnsureOverlay |
| `shared.h` | SharedState/SPlayer/AimCmd/MenuCfg + nama shm V22–**V31** (`SHM_VERSION` ikut define) |
| `overlay_imgui.cpp` | overlay multi V22–V31: 5 tab (ESP/AIM/CAMERA/SETTINGS/INFO), ESP+skeleton, FOV circle, target body part, client-rect W2S, `overlay_cfg.txt` |
| `Offsets.hpp` | **single source of truth** offset (UEOffsets + `namespace DF`; core pakai `DF::`) |
| `build30.bat` | build core30 + overlay multi (MSVC) |
| `StartOverlay.bat` | kill overlay → copy `dfimgui_overlay_multi.exe` → start |
| `watch_inject.bat` | watcher SYSTEM: tunggu proses game → inject core30 |
| `injsys.bat` | injector satu kali (SYSTEM) |
| `setup_imgui.ps1` | clone Dear ImGui ke `imgui/` (dibutuhkan build) |
| `docs/ESP-JITTER-BLINK-RESEARCH.md` | riset jitter/blink ESP — camera cache vs render POV, ViewPunch, box/bones, snapshot, render path |
| `docs/ANTI-RECOIL-RESEARCH.md` | riset anti-recoil — actual/visual/spread, RotationInput, pola per-senjata, probe dfrec, stealth |
| `docs/ESP-BEST-PRACTICES.md` | riset ESP optimal — POV/proyeksi, overlay swapchain, data pipeline, anti-cheat |
| `docs/INJECTION.md` | troubleshooting `NtMapViewOfSection 0xC0000022` — token/timing, watcher SYSTEM, checklist |
| `docs/HANDOFF-history-core11.md` | handoff lama (core11, riset offset awal) |
| `dfsecinject.cpp`, `inject.cpp`, `sessrun.cpp` | injector/launcher (SYSTEM + session 1) |
| `shot.cpp`, `restart_helper.cpp`, `gamestart.cpp` | capture gambar, restart helper, start game |
| `dist/` | build output (dibuat dari `build30.bat`; tidak selalu di-commit) |

File test/probe, build lama, log, dan screenshot **tidak di-commit** (lihat `.gitignore`).

## Quickstart

```powershell
# 1. Dependensi
powershell -ExecutionPolicy Bypass -File .\setup_imgui.ps1

# 2. Build (butuh VS2022 BuildTools)
.\build30.bat

# 3. Inject core30 (SYSTEM; lihat HANDOFF §8)
schtasks /create /tn dfa_inj /tr "C:\Users\User\trackA\injsys.bat" /sc once /st 23:59 /ru SYSTEM /rl HIGHEST /f
schtasks /run /tn dfa_inj

# 4. Overlay (session game)
StartOverlay.bat
```

Hotkey: **INSERT** atau **Ctrl+Alt+M** (dibaca core → shm `keyEdges`). Fallback: `menu.flag`/`ToggleMenu.bat`.

## Status

- ESP semua player (box/nama/jarak/skeleton), team filter off, maxDist 1000.
- FOV circle selalu tampil; aim hanya target di dalam circle.
- ADS: W2S pakai slider `FOV ADS` saat RMB.
- Prediksi `g_draw` (EMA + clamp) dipakai di DrawESP.
- Aim stealth: `AimCmd` absolute → core `AimLoop` → `ROTATION_INPUT` (bukan `ControlRotation`).

## Warning

Research. Anti-cheat dapat mendeteksi injeksi/memory write; tidak ada jaminan undetected. Gunakan di lingkungan uji/terotorisasi.
