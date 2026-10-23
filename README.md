# Game Boy Web — GBA, Game Boy y Game Boy Color en el navegador

Núcleo: [mGBA](https://mgba.io) (C, MPL-2.0), compilado a WebAssembly con
Emscripten. Reproduce **Game Boy Advance (.gba)**, **Game Boy (.gb)** y
**Game Boy Color (.gbc)**. Frontend en HTML/canvas/JS puro con controles
táctiles multi-touch, botones L/R (solo en GBA), guardado automático, menú de
juegos y modo 2X.

## Estructura

```
gba-emu/
├── CMakeLists.txt     # compila mGBA + el wrapper (nativo o WebAssembly)
├── build.sh           # atajo para compilar a WebAssembly
├── make-roms-list.sh  # genera web/roms.json a partir de web/roms/
├── src/
│   ├── mgba/          # mGBA recortado (MPL-2.0, ver src/mgba/LICENSE)
│   ├── wasm_gba.c     # wrapper: ROM, frames, vídeo, audio, botones, saves (GBA y GB/GBC)
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

Los juegos pueden ser `.gba`, `.gb` o `.gbc`: el núcleo se elige solo por la
extensión. La pantalla cambia de tamaño (240×160 en GBA, 160×144 en GB/GBC) y
los botones L/R se ocultan en Game Boy, que no los tiene.

Si no hay `roms.json`, la web intenta cargar un único `web/rom.gba` (solo
si existe).

### Cargar un juego desde el dispositivo

El botón **"+ Cargar juego del dispositivo"** del menú permite elegir un
`.gba` desde el móvil u ordenador. El archivo **no se sube a ningún sitio**:
se queda en tu navegador (IndexedDB) y aparece en la sección *"En este
dispositivo"* del menú, con un botón ✕ para borrarlo (la partida guardada se
conserva). Así puedes usar la web sin incluir ningún ROM en el repositorio.

Aviso para iPhone/iPad: Safari borra los datos de las webs que no se usan en
7 días, salvo que la añadas a la pantalla de inicio. Si no, tendrás que
volver a cargar el juego (y perderías la partida guardada).

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
- **Game Boy / Color**: los juegos de Super Game Boy se muestran sin marco
  (siempre 160×144). Los juegos clásicos usan la paleta por defecto de mGBA.
  Todas las partidas se guardan igual que en GBA.
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
