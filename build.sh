#!/usr/bin/env bash
# Compila mGBA + wrapper a WebAssembly -> web/gb.js + web/gb.wasm
# Requiere Emscripten activado:  source ~/emsdk/emsdk_env.sh
set -euo pipefail

cd "$(dirname "$0")"

if ! command -v emcmake >/dev/null 2>&1; then
	echo "No encuentro emcmake. Activa Emscripten primero:"
	echo "  source ~/emsdk/emsdk_env.sh"
	exit 1
fi

emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm --target gb -j 4

echo "OK -> web/gb.js + web/gb.wasm"
