/* app.js — pega el núcleo WASM de mGBA (gb.js/gb.wasm) con el DOM.
 *
 * Versión "un solo juego": el ROM se carga automáticamente desde
 * ROM_URL al arrancar, sin selector de archivo ni menú superior.
 */

(() => {
  const ROM_URL = 'rom.gba'; // pon tu archivo .gba en web/ con este nombre (o cambia esto)
  const W = 240, H = 160;

  const canvas = document.getElementById('gb-canvas');
  const ctx = canvas.getContext('2d');
  const hint = document.getElementById('hint');

  let Module = null;
  let running = false;
  let imageData = ctx.createImageData(W, H);
  let saveKey = null;       // clave localStorage del save actual (o null si el cartucho no tiene batería)
  let lastSavedB64 = null;  // para no reescribir localStorage si no ha cambiado nada

  function showError(msg) {
    hint.textContent = msg;
    hint.classList.add('show');
    console.error(msg);
  }

  Promise.all([
    createGBModule(),
    fetch(ROM_URL).then((r) => {
      if (!r.ok) throw new Error('HTTP ' + r.status + ' al pedir ' + ROM_URL);
      return r.arrayBuffer();
    }),
  ])
    .then(([mod, romBuf]) => {
      Module = mod;
      loadRom(new Uint8Array(romBuf));
    })
    .catch((err) => {
      showError('No se pudo cargar el juego: ' + err);
    });

  function loadRom(buf) {
    const ptr = Module._em_alloc_rom(buf.length);
    Module.HEAPU8.set(buf, ptr);

    // La partida guardada (si hay) tiene que estar en su sitio antes de arrancar.
    setupSaveSlot(ROM_URL, buf.length);

    const err = Module._em_start();
    if (err !== 0) {
      showError('No se pudo cargar el ROM (código ' + err + ')');
      return;
    }

    const led = document.getElementById('led');
    if (led) led.classList.add('on');

    running = true;
    requestAnimationFrame(frame);
  }

  /* ---- Guardado de partidas en localStorage ---- */
  // mGBA usa un bloque de 128 KiB para cualquier tipo de guardado (SRAM,
  // Flash, EEPROM). Solo se escribe en localStorage cuando el juego lo cambia.

  function setupSaveSlot(fileName, romSize) {
    saveKey = 'gbasave::' + fileName + '::' + romSize;
    lastSavedB64 = null;

    const stored = localStorage.getItem(saveKey);
    if (stored) {
      const bytes = base64ToU8(stored);
      if (bytes.length === Module._em_get_save_size()) {
        Module.HEAPU8.set(bytes, Module._em_get_save_ptr());
        lastSavedB64 = stored;
      }
    }
  }

  function saveNow() {
    if (!Module || !saveKey) return;
    if (!Module._em_save_changed()) return; // el juego no ha tocado la partida

    const size = Module._em_get_save_size();
    const ptr = Module._em_get_save_ptr();
    const b64 = u8ToBase64(new Uint8Array(Module.HEAPU8.buffer, ptr, size));
    if (b64 === lastSavedB64) return;
    lastSavedB64 = b64;
    localStorage.setItem(saveKey, b64);
  }

  function u8ToBase64(u8) {
    let binary = '';
    const chunk = 0x8000;
    for (let i = 0; i < u8.length; i += chunk) {
      binary += String.fromCharCode.apply(null, u8.subarray(i, i + chunk));
    }
    return btoa(binary);
  }

  function base64ToU8(b64) {
    const binary = atob(b64);
    const u8 = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i++) u8[i] = binary.charCodeAt(i);
    return u8;
  }

  // Autoguardado periódico (solo escribe si cambió algo) y al perder foco / cerrar.
  setInterval(saveNow, 3000);
  document.addEventListener('visibilitychange', () => {
    if (document.hidden) saveNow();
  });
  window.addEventListener('pagehide', saveNow);

  // Velocidad real de una Game Boy Advance: 59.7275 fps (no 60 en punto). Si atamos
  // el emulador directamente a requestAnimationFrame, la velocidad del
  // juego queda ligada a la frecuencia de refresco real de la pantalla —
  // y muchos móviles la reducen a 30Hz (ahorro de batería, "modo
  // adaptable", etc.), lo que se nota como cámara lenta a la mitad de
  // velocidad. Usamos un reloj real (performance.now) para decidir cuántos
  // frames de GBA toca correr, independientemente de a qué frecuencia
  // vaya la pantalla.
  const GB_FRAME_MS = 1000 / 59.7275;
  const MAX_FRAMES_PER_TICK = 8; // margen suficiente para cuando el modo rapido duplica las que hacen falta por tick
  let lastTime = null;
  let accumulator = 0;
  let fastMode = false;

  function frame(now) {
    if (!running) return;

    if (lastTime === null) lastTime = now;
    let elapsed = now - lastTime;
    lastTime = now;

    // Si ha pasado muchísimo tiempo (pestaña en segundo plano, móvil
    // bloqueado, etc.) no lo intentamos recuperar todo de golpe.
    if (elapsed > 1000) elapsed = GB_FRAME_MS;
    accumulator += elapsed;

    // Modo rápido: al doble de velocidad simplemente exigimos la mitad de
    // tiempo real por cada frame de Game Boy que toca correr.
    const frameMs = fastMode ? GB_FRAME_MS / 2 : GB_FRAME_MS;

    let framesRun = 0;
    while (accumulator >= frameMs && framesRun < MAX_FRAMES_PER_TICK) {
      Module._em_run_frame();
      // En modo rápido el audio se silencia (si no, sale un pitido agudo
      // tipo "ardilla" al acelerarlo al doble) — se recupera solo al
      // desactivar el modo rápido.
      if (!fastMode) pushAudio();
      accumulator -= frameMs;
      framesRun++;
    }
    if (framesRun > 0) blit();

    requestAnimationFrame(frame);
  }

  function blit() {
    const ptr = Module._em_get_framebuffer();
    // Vista fresca cada frame: con ALLOW_MEMORY_GROWTH el buffer puede
    // haberse reemplazado por otro más grande.
    const src = new Uint32Array(Module.HEAPU8.buffer, ptr, W * H);
    new Uint32Array(imageData.data.buffer).set(src);
    ctx.putImageData(imageData, 0, 0);
  }

  /* ---- Audio ---- */
  // AudioWorklet en su propio hilo real de audio: el hilo principal (donde
  // corre el emulador) solo empuja un trozo de muestras por frame.
  let audioCtx = null;
  let gbAudioNode = null;

  function initAudio() {
    if (audioCtx) {
      if (audioCtx.state === 'suspended') audioCtx.resume();
      return;
    }
    try {
      audioCtx = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 44100 });
      if (!audioCtx.audioWorklet) throw new Error('AudioWorklet no disponible (¿http sin localhost?)');
      audioCtx.audioWorklet.addModule('gb-audio-worklet.js').then(() => {
        gbAudioNode = new AudioWorkletNode(audioCtx, 'gb-audio-processor', {
          outputChannelCount: [2],
        });
        gbAudioNode.connect(audioCtx.destination);
      });
    } catch (err) {
      // Sin audio, pero el juego sigue siendo jugable igualmente.
      console.warn('Audio no disponible:', err);
    }
  }

  function pushAudio() {
    if (!gbAudioNode) return;

    const frames = Module._em_get_audio_frames(); // pares L/R a 44100 Hz
    if (frames === 0) return;
    const ptr = Module._em_get_audio_buffer();
    const samples = new Int16Array(Module.HEAPU8.buffer, ptr, frames * 2);

    const left = new Float32Array(frames);
    const right = new Float32Array(frames);
    for (let i = 0; i < frames; i++) {
      left[i] = samples[i * 2] / 32768;
      right[i] = samples[i * 2 + 1] / 32768;
    }

    gbAudioNode.port.postMessage({ left, right }, [left.buffer, right.buffer]);
  }

  /* ---- Controles táctiles ---- */
  // Un botón puede recibir varios punteros (dedo) a la vez; sólo se
  // considera "soltado" cuando el último puntero activo sobre él se va.
  const activePointers = new Map(); // pointerId -> btnElement
  let audioUnlocked = false;

  document.querySelectorAll('[data-btn]').forEach((el) => {
    const btnId = parseInt(el.dataset.btn, 10);

    const press = (ev) => {
      ev.preventDefault();
      if (!audioUnlocked) {
        // El primer toque del usuario en cualquier botón sirve como el
        // "gesto" que los navegadores exigen para permitir reproducir audio.
        audioUnlocked = true;
        initAudio();
      }
      el.setPointerCapture(ev.pointerId);
      activePointers.set(ev.pointerId, el);
      el.classList.add('active');
      if (Module) Module._em_set_button(btnId, 1);
    };

    const release = (ev) => {
      ev.preventDefault();
      if (!activePointers.has(ev.pointerId)) return;
      activePointers.delete(ev.pointerId);
      el.classList.remove('active');
      if (Module) Module._em_set_button(btnId, 0);
    };

    el.addEventListener('pointerdown', press);
    el.addEventListener('pointerup', release);
    el.addEventListener('pointercancel', release);
    el.addEventListener('pointerleave', (ev) => {
      // Si el dedo se arrastra fuera del botón sin soltarlo, lo damos por
      // soltado (evita botones "pegados" al deslizar entre el D-pad).
      release(ev);
    });
  });

  // Evita el gesto de "pull to refresh" / zoom por doble tap en iOS/Android.
  document.addEventListener('touchmove', (e) => e.preventDefault(), { passive: false });

  /* ---- Toggle de modo rápido (x2) ---- */
  const fastToggle = document.getElementById('fast-toggle');
  if (fastToggle) {
    fastToggle.addEventListener('pointerdown', (ev) => {
      ev.preventDefault();
      if (!audioUnlocked) {
        audioUnlocked = true;
        initAudio();
      }
      fastMode = !fastMode;
      fastToggle.classList.toggle('on', fastMode);
    });
  }
})();
