#!/bin/sh
# Dear ImGui (https://github.com/ocornut/imgui) - diperlukan untuk menu_imgui.exe
set -e
mkdir -p third_party
[ -d third_party/imgui ] || git clone --depth 1 https://github.com/ocornut/imgui.git third_party/imgui
echo "imgui ready: $(git -C third_party/imgui rev-parse --short HEAD)"
