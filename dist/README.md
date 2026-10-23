# dist/ — build outputs

Binari **tidak** ter-commit pada sesi push ini karena build host (laptop `<laptop-host>`) unreachable saat push.
Build ulang dengan `..\build30.bat` (butuh VS2022 BuildTools + `imgui/` dari `setup_imgui.ps1`):

- `dfcore30.dll` — core in-game (V30)
- `dfimgui_overlay_multi.exe` — overlay multi V22–V30
- `dfsecinject.exe` — injector manual-map (jalankan sebagai SYSTEM; lihat HANDOFF §8)
- `sessrun.exe`, `shot.exe` — tool sesi/screenshot

Setelah build, taruh di folder ini dan commit: `.gitignore` mengizinkan `dist/*.exe` dan `dist/*.dll`.
