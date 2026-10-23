# Clone Dear ImGui ke ./imgui (build30.bat expects imgui\imgui.cpp + imgui\backends)
$ErrorActionPreference = "Stop"
if (Test-Path "imgui\imgui.cpp") { Write-Output "imgui already present"; exit 0 }
git clone --depth 1 https://github.com/ocornut/imgui.git imgui
Write-Output "imgui cloned"
