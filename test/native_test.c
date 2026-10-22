/* Prueba nativa del wrapper: una ROM mínima (código ARM escrito a mano)
 * que activa el modo gráfico 3 y pinta el píxel (0,0) de rojo puro. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

uint8_t *em_alloc_rom(uint32_t);
int em_start(void);
void em_run_frame(void);
uint32_t *em_get_framebuffer(void);
int em_get_audio_frames(void);
void em_set_button(int, int);
int em_save_changed(void);
uint8_t *em_get_save_ptr(void);

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

	int rc = em_start();
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

	rc = em_start();
	if (rc)
		return 3;
	for (int i = 0; i < 10; i++)
		em_run_frame();
	int changed = em_save_changed();
	printf("guardado: changed=%d byte0=0x%02x (esperado 1 y 0x42)\n", changed, em_get_save_ptr()[0]);
	return (changed == 1 && em_get_save_ptr()[0] == 0x42) ? 0 : 4;
}
