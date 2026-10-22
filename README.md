# GBA Web — emulador de Game Boy Advance en el navegador

Núcleo: [mGBA](https://mgba.io) (C, MPL-2.0), compilado a WebAssembly con
Emscripten. Frontend en HTML/canvas/JS puro con controles táctiles
multi-touch, botones L/R, guardado automático y modo 2X.

## Estructura

```
gba-emu/
├── CMakeLists.txt     # compila mGBA + el wrapper (nativo o WebAssembly)
├── build.sh           # atajo para compilar a WebAssembly
├── src/
│   ├── mgba/          # mGBA recortado (MPL-2.0, ver src/mgba/LICENSE)
│   ├── wasm_gba.c     # wrapper: ROM, frames, vídeo, audio, botones, saves
│   └── post.js        # engancha HEAPU8 al objeto Module
├── test/native_test.c # prueba sin navegador (ver abajo)
└── web/
    ├── index.html          # carcasa, pantalla 240x160 y controles
    ├── app.js              # bucle, input, audio, guardado
    └── gb-audio-worklet.js # audio en su propio hilo
```

## Compilar

```bash
source ~/emsdk/emsdk_env.sh
./build.sh
```

La primera vez tarda un par de minutos (compila unos 100 archivos de mGBA).

## Probar en local

Copia tu ROM como `web/rom.gba` y:

```bash
cd web
python3 -m http.server 8000
```

Abre `http://localhost:8000`.

## Publicar en GitHub Pages

```bash
rm -rf docs
cp -r web docs
git add .
git commit -m "version inicial gba"
git push
```

En GitHub: Settings → Pages → Deploy from a branch → `main` → `/docs`.

## Notas

- **Guardado**: mGBA usa un bloque de 128 KiB para cualquier tipo de
  partida (SRAM, Flash, EEPROM). Se guarda en `localStorage` solo cuando el
  juego lo modifica, y se restaura al abrir la web.
- **Sin hilos**: mGBA se compila sin hilos para no necesitar
  `SharedArrayBuffer` (GitHub Pages no puede activarlo). Hay un parche de una
  línea en `src/mgba/CMakeLists.txt`, marcado con `[gba-web]`.
- **BIOS**: no hace falta; mGBA incluye una BIOS de sustitución.
- **Rendimiento**: GBA es bastante más exigente que GB/GBC. Si va a cámara
  lenta en un móvil modesto, lo primero que se puede quitar es el audio
  (`pushAudio()` en el bucle de `app.js`).
- **Prueba nativa** (opcional, sin navegador, necesita cmake):
  ```bash
  cmake -S . -B build-native -DNO_THREADS=ON && cmake --build build-native --target native_test
  ./build-native/native_test
  ```
  Comprueba arranque, vídeo (orden de colores), audio y guardado con ROMs
  mínimas escritas a mano.
