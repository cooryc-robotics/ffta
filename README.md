# GBA Web — emulador de Game Boy Advance en el navegador

Núcleo: [mGBA](https://mgba.io) (C, MPL-2.0), compilado a WebAssembly con
Emscripten. Frontend en HTML/canvas/JS puro con controles táctiles
multi-touch, botones L/R, guardado automático y modo 2X.

## Estructura

```
gba-emu/
├── CMakeLists.txt     # compila mGBA + el wrapper (nativo o WebAssembly)
├── build.sh           # atajo para compilar a WebAssembly
├── make-roms-list.sh  # genera web/roms.json a partir de web/roms/
├── src/
│   ├── mgba/          # mGBA recortado (MPL-2.0, ver src/mgba/LICENSE)
│   ├── wasm_gba.c     # wrapper: ROM, frames, vídeo, audio, botones, saves
│   └── post.js        # engancha HEAPU8 al objeto Module
├── test/native_test.c # prueba sin navegador (ver abajo)
└── web/
    ├── index.html          # carcasa, pantalla 240x160, controles y menú
    ├── roms/               # tus juegos (.gba)
    ├── roms.json           # lista del menú (la genera make-roms-list.sh)
    ├── app.js              # bucle, input, audio, guardado
    └── gb-audio-worklet.js # audio en su propio hilo
```

## Compilar

```bash
source ~/emsdk/emsdk_env.sh
./build.sh
```

La primera vez tarda un par de minutos (compila unos 100 archivos de mGBA).

## Varios juegos y menú de selección

Al abrir la web aparece un menú con tus juegos; el botón **MENU** (arriba a
la derecha) vuelve a él para cambiar de juego. Cada juego guarda su propia
partida.

1. Mete tus ROMs en `web/roms/` (por ejemplo `web/roms/Pokemon_Esmeralda.gba`).
2. Genera la lista del menú:
   ```bash
   ./make-roms-list.sh
   ```
   Esto crea `web/roms.json` con un título por archivo (`_` se convierte en
   espacio). Si quieres otro título, edita `roms.json` a mano.
3. Repite el paso 2 cada vez que añadas o quites un juego.

Si no hay `roms.json`, la web intenta cargar un único `web/rom.gba`.

Para probarlo en local:

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
- **Audio**: mGBA arranca con el volumen a 0 si nadie lo fija, así que
  `wasm_gba.c` lo establece a `0x100` (máximo). `AudioWorklet` necesita
  `https://` o `localhost`; en el iPhone, el interruptor de silencio
  también calla el audio web, y el modo 2X silencia el sonido a propósito.
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
