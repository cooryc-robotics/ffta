/* Prueba nativa del wrapper: una ROM mínima (código ARM escrito a mano)
 * que activa el modo gráfico 3 y pinta el píxel (0,0) de rojo puro. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

uint8_t *em_alloc_rom(uint32_t);
int em_start(int kind);
void em_run_frame(void);
uint32_t *em_get_framebuffer(void);
int em_get_audio_frames(void);
void em_set_button(int, int);
int em_save_changed(void);
uint8_t *em_get_save_ptr(void);
int16_t *em_get_audio_buffer(void);
int em_fb_width(void);
int em_fb_height(void);

static void put32(uint8_t *p, uint32_t v)
{
	p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

int main(void)
{
	uint32_t sz = 1 << 20;
	uint8_t *r = em_alloc_rom(sz);
	memset(r, 0, sz);

	const uint32_t code[] = {
		0xE3A00301, /* mov r0, #0x04000000 (REG_DISPCNT) */
		0xE3A01B01, /* mov r1, #0x400                    */
		0xE3811003, /* orr r1, r1, #3  -> modo 3 + BG2   */
		0xE1C010B0, /* strh r1, [r0]                     */
		0xE3A00406, /* mov r0, #0x06000000 (VRAM)        */
		0xE3A0101F, /* mov r1, #0x1F   -> rojo (BGR555)  */
		0xE1C010B0, /* strh r1, [r0]                     */
		0xEAFFFFFE, /* b .                               */
	};
	for (unsigned i = 0; i < sizeof(code) / sizeof(code[0]); i++)
		put32(r + i * 4, code[i]);

	int rc = em_start(0);
	printf("start rc=%d\n", rc);
	if (rc)
		return 1;

	long total = 0;
	for (int i = 0; i < 300; i++) {
		em_set_button(3, i % 2);
		em_run_frame();
		total += em_get_audio_frames();
	}
	uint32_t px = em_get_framebuffer()[0];
	printf("300 frames ok, audio frames=%ld (esperado ~%d)\n", total, 300 * 738);
	printf("pixel(0,0)=%08x (esperado ff0000ff = rojo opaco R,G,B,A)\n", px);
	printf("tamano de pantalla GBA: %dx%d (esperado 240x160)\n", em_fb_width(), em_fb_height());
	if (em_fb_width() != 240 || em_fb_height() != 160)
		return 7;
	printf("guardado cambiado=%d\n", em_save_changed());
	if (px != 0xff0000ffu)
		return 2;

	/* --- Prueba de guardado: ROM con SRAM que escribe 0x42 en 0x0E000000 --- */
	r = em_alloc_rom(sz);
	memset(r, 0, sz);
	const uint32_t code2[] = {
		0xE3A0040E, /* mov r0, #0x0E000000 (SRAM) */
		0xE3A01042, /* mov r1, #0x42              */
		0xE5C01000, /* strb r1, [r0]              */
		0xEAFFFFFE, /* b .                        */
	};
	for (unsigned i = 0; i < sizeof(code2) / sizeof(code2[0]); i++)
		put32(r + i * 4, code2[i]);
	memcpy(r + 0x100, "SRAM_V113", 9); /* así mGBA detecta el tipo de guardado */

	rc = em_start(0);
	if (rc)
		return 3;
	for (int i = 0; i < 10; i++)
		em_run_frame();
	int changed = em_save_changed();
	printf("guardado: changed=%d byte0=0x%02x (esperado 1 y 0x42)\n", changed, em_get_save_ptr()[0]);
	if (!(changed == 1 && em_get_save_ptr()[0] == 0x42))
		return 4;

	/* --- Prueba de audio: ROM que enciende el canal 1 (onda cuadrada) --- */
	r = em_alloc_rom(sz);
	memset(r, 0, sz);
	const uint32_t code3[] = {
		0xE3A00301, /* mov r0, #0x04000000                  */
		0xE3A01080, /* mov r1, #0x80                        */
		0xE1C018B4, /* strh r1,[r0,#0x84] SOUNDCNT_X on     */
		0xE3A01CFF, /* mov r1, #0xFF00                      */
		0xE3811077, /* orr r1, r1, #0x77                    */
		0xE1C018B0, /* strh r1,[r0,#0x80] SOUNDCNT_L        */
		0xE3A01002, /* mov r1, #2                           */
		0xE1C018B2, /* strh r1,[r0,#0x82] SOUNDCNT_H 100%   */
		0xE3A01000, /* mov r1, #0                           */
		0xE1C016B0, /* strh r1,[r0,#0x60] SOUND1CNT_L       */
		0xE3A01CF0, /* mov r1, #0xF000                      */
		0xE3811080, /* orr r1, r1, #0x80                    */
		0xE1C016B2, /* strh r1,[r0,#0x62] SOUND1CNT_H vol15 */
		0xE3A01C80, /* mov r1, #0x8000                      */
		0xE3811B01, /* orr r1, r1, #0x400                   */
		0xE1C016B4, /* strh r1,[r0,#0x64] SOUND1CNT_X trig  */
		0xEAFFFFFE, /* b .                                  */
	};
	for (unsigned i = 0; i < sizeof(code3) / sizeof(code3[0]); i++)
		put32(r + i * 4, code3[i]);
	if (em_start(0))
		return 5;
	int peak = 0;
	for (int i = 0; i < 60; i++) {
		em_run_frame();
		int n = em_get_audio_frames();
		int16_t *a = em_get_audio_buffer();
		for (int j = 0; j < n * 2; j++) {
			int v = a[j] < 0 ? -a[j] : a[j];
			if (v > peak)
				peak = v;
		}
	}
	printf("audio GBA: pico de amplitud=%d (esperado > 1000)\n", peak);
	if (peak <= 1000)
		return 6;

	/* --- Game Boy / Game Boy Color: ROM mínima que enciende el canal 1 --- */
	for (int cgb = 0; cgb <= 1; cgb++) {
		r = em_alloc_rom(32768);
		memset(r, 0, 32768);
		r[0x100] = 0xC3; r[0x101] = 0x50; r[0x102] = 0x01; /* jp 0x0150 */
		static const uint8_t gb_code[] = {
			0x3E, 0x80, 0xE0, 0x26, /* NR52 = 0x80 (sonido on)   */
			0x3E, 0x77, 0xE0, 0x24, /* NR50 = 0x77 (volumen)     */
			0x3E, 0xFF, 0xE0, 0x25, /* NR51 = 0xFF (izq + der)   */
			0x3E, 0x80, 0xE0, 0x11, /* NR11 = duty 50%           */
			0x3E, 0xF0, 0xE0, 0x12, /* NR12 = volumen 15         */
			0x3E, 0x00, 0xE0, 0x13, /* NR13 = frecuencia baja    */
			0x3E, 0x87, 0xE0, 0x14, /* NR14 = disparar + freq    */
			0x18, 0xFE,             /* jr -2                     */
		};
		memcpy(r + 0x150, gb_code, sizeof(gb_code));
		if (cgb)
			r[0x143] = 0x80;
		uint8_t sum = 0;
		for (int i = 0x134; i <= 0x14C; i++)
			sum = sum - r[i] - 1;
		r[0x14D] = sum;

		if (em_start(1))
			return 8;
		int gpeak = 0;
		for (int i = 0; i < 60; i++) {
			em_run_frame();
			int n = em_get_audio_frames();
			int16_t *a = em_get_audio_buffer();
			for (int j = 0; j < n * 2; j++) {
				int v = a[j] < 0 ? -a[j] : a[j];
				if (v > gpeak)
					gpeak = v;
			}
		}
		printf("%s: pantalla %dx%d (esperado 160x144), pico de audio=%d (esperado > 500)\n",
		       cgb ? "GBC" : "GB ", em_fb_width(), em_fb_height(), gpeak);
		if (em_fb_width() != 160 || em_fb_height() != 144 || gpeak <= 500)
			return 9;
	}

	/* --- Volver a un juego de GBA tras haber usado el núcleo GB --- */
	r = em_alloc_rom(sz);
	memset(r, 0, sz);
	for (unsigned i = 0; i < sizeof(code) / sizeof(code[0]); i++)
		put32(r + i * 4, code[i]);
	if (em_start(0))
		return 10;
	for (int i = 0; i < 5; i++)
		em_run_frame();
	printf("GBA otra vez: pantalla %dx%d, pixel(0,0)=%08x\n", em_fb_width(), em_fb_height(), em_get_framebuffer()[0]);
	return (em_fb_width() == 240 && em_get_framebuffer()[0] == 0xff0000ffu) ? 0 : 11;
}
