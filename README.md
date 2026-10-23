# Delta Force Track A — ESP + Aim (private)

Riset dan implementasi ESP/aim untuk `DeltaForceClient-Win64-Shipping.exe` **1.14.0.772**.
Repo private. Baca **[HANDOFF.md](HANDOFF.md)** untuk arsitektur, offset, shm, build, deploy, anti-cheat, dan known issues.

## Isi repo

| Path | Isi |
|---|---|
| `core.cpp` | core30 DLL (in-game): scan semua level, publish shm V30 tiap 10ms, AimLoop `ROTATION_INPUT`, PollKeys, EnsureOverlay |
| `shared.h` | SharedState/SPlayer/AimCmd/MenuCfg + nama shm V22–V30 |
| `overlay_imgui.cpp` | overlay multi V22–V30: ESP, skeleton, FOV circle, prediksi `g_draw`, ADS FOV, AimCmd, menu |
| `Offsets.hpp` | UEOffsets (UE5 SDK: FNamePool/UObject) |
| `build30.bat` | build core30 + overlay multi (MSVC) |
| `StartOverlay.bat` | kill overlay → copy `dfimgui_overlay_multi.exe` → start |
| `watch_inject.bat` | watcher SYSTEM: tunggu proses game → inject core30 |
| `injsys.bat`, `inj30.bat` | injector satu kali (SYSTEM) |
| `setup_imgui.ps1` | clone Dear ImGui ke `imgui/` (dibutuhkan build) |
| `docs/HANDOFF-history-core11.md` | handoff lama (core11, riset offset awal) |
| `dist/` | build output (dibuat dari `build30.bat`; tidak selalu di-commit) |

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

Private research. Anti-cheat dapat mendeteksi injeksi/memory write; tidak ada jaminan undetected. Gunakan di lingkungan uji/terotorisasi.
