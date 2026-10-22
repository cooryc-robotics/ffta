/*
 * wasm_gba.c
 *
 * Wrapper de mGBA (Game Boy Advance, MPL-2.0) para WebAssembly. Expone al
 * JavaScript funciones simples: cargar ROM, correr un frame, leer pantalla
 * y audio, botones y partida guardada. El orden de inicialización sigue el
 * del frontend libretro de mGBA.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/gba/core.h>
#include <mgba/internal/gba/input.h>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/audio-resampler.h>
#include <mgba-util/vfs.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#define GBA_W 240
#define GBA_H 160
#define OUT_RATE 44100
#define SAVE_SIZE 0x20000 /* 128 KiB: el máximo de un cartucho GBA (Flash 1M) */
#define AUDIO_OUT_FRAMES 2048

static struct mCore *core = NULL;

static uint8_t *rom_buf = NULL;
static size_t rom_size = 0;

static mColor video[GBA_W * GBA_H];
static uint32_t framebuffer[GBA_W * GBA_H]; /* RGBA para <canvas> */

static struct mAudioBuffer audio_dest;
static struct mAudioResampler resampler;
static int16_t audio_out[AUDIO_OUT_FRAMES * 2];
static int audio_out_frames = 0;
static int audio_ready = 0;

static uint8_t savedata[SAVE_SIZE];
static uint8_t save_prev[SAVE_SIZE];

static uint32_t keys = 0;

/* Libera el juego actual (si lo hay). Hay que hacerlo ANTES de liberar el
 * buffer del ROM, porque el núcleo lo tiene mapeado. */
static void teardown(void)
{
	if (!core)
		return;
	if (audio_ready) {
		mAudioResamplerDeinit(&resampler);
		mAudioBufferDeinit(&audio_dest);
		audio_ready = 0;
	}
	core->deinit(core);
	core = NULL;
	keys = 0;
}

/* Reserva el buffer donde JS copia los bytes del .gba. Se puede llamar
 * varias veces (para cambiar de juego): limpia el anterior y deja la
 * partida guardada en blanco para que no se mezcle con la del siguiente. */
EMSCRIPTEN_KEEPALIVE
uint8_t *em_alloc_rom(uint32_t size)
{
	teardown();
	memset(savedata, 0xFF, SAVE_SIZE);
	memset(save_prev, 0xFF, SAVE_SIZE);
	free(rom_buf);
	rom_buf = malloc(size);
	rom_size = size;
	return rom_buf;
}

/* Buffer de la partida guardada (128 KiB, inicializado a 0xFF). Si JS tiene
 * una partida guardada, debe copiarla aquí ANTES de llamar a em_start(). */
EMSCRIPTEN_KEEPALIVE
uint8_t *em_get_save_ptr(void) { return savedata; }

EMSCRIPTEN_KEEPALIVE
uint32_t em_get_save_size(void) { return SAVE_SIZE; }

/* Crea el emulador con el ROM ya copiado. 0 = OK, distinto de 0 = error. */
EMSCRIPTEN_KEEPALIVE
int em_start(void)
{
	teardown();
	if (!rom_buf)
		return 1;

	core = GBACoreCreate();
	if (!core)
		return 2;

	mCoreInitConfig(core, NULL);
	core->init(core);
	core->setVideoBuffer(core, video, GBA_W);

	/* ~549 muestras por frame a 32768 Hz; margen doble. */
	core->setAudioBufferSize(core, 1024);

	mCoreConfigSetDefaultValue(&core->config, "idleOptimization", "remove");
	/* Sin esto el volumen de mGBA vale 0 y todo suena en silencio (los
	 * frontends de mGBA lo fijan siempre; 0x100 = volumen máximo). */
	mCoreConfigSetDefaultIntValue(&core->config, "volume", 0x100);
	mCoreLoadConfig(core);

	struct VFile *vf = VFileFromMemory(rom_buf, rom_size);
	if (!vf || !core->loadROM(core, vf)) {
		return 3;
	}

	struct VFile *save = VFileFromMemory(savedata, SAVE_SIZE);
	if (save && !core->loadSave(core, save)) {
		save->close(save);
	}
	memcpy(save_prev, savedata, SAVE_SIZE);

	core->reset(core);

	/* Resampleo del audio nativo (32768 Hz) a 44100 Hz. */
	mAudioBufferInit(&audio_dest, 4096, 2);
	mAudioResamplerInit(&resampler, mINTERPOLATOR_COSINE);
	mAudioResamplerSetSource(&resampler, core->getAudioBuffer(core),
				 (double)core->audioSampleRate(core), true);
	mAudioResamplerSetDestination(&resampler, &audio_dest, (double)OUT_RATE);
	audio_ready = 1;

	keys = 0;
	return 0;
}

/* Corre un frame de vídeo y deja listo el audio de ese frame. */
EMSCRIPTEN_KEEPALIVE
void em_run_frame(void)
{
	if (!core)
		return;

	core->setKeys(core, keys);
	core->runFrame(core);

	/* Pantalla: mGBA deja el canal alfa a 0, el canvas lo necesita a 255. */
	for (int i = 0; i < GBA_W * GBA_H; i++)
		framebuffer[i] = video[i] | 0xFF000000u;

	/* Audio: resamplear lo que haya salido en este frame. */
	mAudioResamplerProcess(&resampler);
	size_t n = mAudioBufferRead(&audio_dest, audio_out, AUDIO_OUT_FRAMES);
	audio_out_frames = (int)n;
}

/* Pantalla: 240x160 píxeles RGBA (bytes R,G,B,A), listo para ImageData. */
EMSCRIPTEN_KEEPALIVE
uint32_t *em_get_framebuffer(void) { return framebuffer; }

EMSCRIPTEN_KEEPALIVE
int em_fb_width(void) { return GBA_W; }

EMSCRIPTEN_KEEPALIVE
int em_fb_height(void) { return GBA_H; }

/* Audio: int16 estéreo intercalado a 44100 Hz. Devuelve nº de frames
 * (pares L/R) disponibles tras el último em_run_frame(). */
EMSCRIPTEN_KEEPALIVE
int16_t *em_get_audio_buffer(void) { return audio_out; }

EMSCRIPTEN_KEEPALIVE
int em_get_audio_frames(void) { return audio_out_frames; }

/* Botones: 0=A 1=B 2=SELECT 3=START 4=RIGHT 5=LEFT 6=UP 7=DOWN 8=R 9=L */
EMSCRIPTEN_KEEPALIVE
void em_set_button(int button, int pressed)
{
	static const int map[10] = {
		GBA_KEY_A, GBA_KEY_B, GBA_KEY_SELECT, GBA_KEY_START,
		GBA_KEY_RIGHT, GBA_KEY_LEFT, GBA_KEY_UP, GBA_KEY_DOWN,
		GBA_KEY_R, GBA_KEY_L
	};
	if (button < 0 || button > 9)
		return;
	uint32_t bit = 1u << map[button];
	if (pressed)
		keys |= bit;
	else
		keys &= ~bit;
}

/* Devuelve 1 si el juego ha modificado la partida guardada desde la última
 * vez que se preguntó (así JS solo guarda cuando hace falta). */
EMSCRIPTEN_KEEPALIVE
int em_save_changed(void)
{
	if (memcmp(savedata, save_prev, SAVE_SIZE) == 0)
		return 0;
	memcpy(save_prev, savedata, SAVE_SIZE);
	return 1;
}

/* Detiene el juego actual (para volver al menú). */
EMSCRIPTEN_KEEPALIVE
void em_stop(void) { teardown(); }
