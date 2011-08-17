/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo prototype firmware
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include <stdint.h>
#include <mcs51/8051.h>

sfr at 0x93 P0DIR;
sfr at 0x94 P1DIR;
sfr at 0x95 P2DIR;
sfr at 0x96 P3DIR;

#define LCD_WIDTH 	128
#define LCD_HEIGHT	128
#define LCD_PIXELS	(LCD_WIDTH * LCD_HEIGHT)
#define LCD_ROW_SHIFT	8

#define ANGLE_90      	64
#define ANGLE_180      	128
#define ANGLE_270      	192
#define ANGLE_360	256

#define CHROMA_KEY	0xF5

#define BUS_PORT	P0
#define BUS_DIR 	P0DIR

#define ADDR_PORT	P1
#define ADDR_DIR	P1DIR

#define CTRL_PORT	P2
#define CTRL_DIR 	P2DIR

#define CTRL_LCD_RDX	(1 << 0)
#define CTRL_LCD_DCX	(1 << 1)
#define CTRL_LCD_CSX	(1 << 2)
#define CTRL_FLASH_WE	(1 << 3)
#define CTRL_FLASH_CE	(1 << 4)
#define CTRL_FLASH_OE	(1 << 5)
#define CTRL_FLASH_LAT1	(1 << 6)
#define CTRL_FLASH_LAT2	(1 << 7)

#define CTRL_IDLE	(CTRL_FLASH_WE | CTRL_FLASH_OE | CTRL_LCD_DCX)
#define CTRL_LCD_CMD	(CTRL_FLASH_WE | CTRL_FLASH_OE)
#define CTRL_FLASH_OUT	(CTRL_FLASH_WE | CTRL_LCD_DCX)

#define LCD_CMD_NOP  	0x00
#define LCD_CMD_CASET	0x2A
#define LCD_CMD_RASET	0x2B
#define LCD_CMD_RAMWR	0x2C

#define TILE(s, x, y)	(tilemap[(x) + ((y)<<(s))])
#define NUM_SPRITES	4

xdata uint8_t tilemap[256];

// For now, a very limited number of 32x32 sprites
near struct {
    uint8_t x, y;
    int8_t xd, yd;
} oam[NUM_SPRITES];


/*
 * Blatantly insufficient for a real chip, but so far this sets up
 * everything the simulator cares about.
 */
void hardware_init(void)
{
    BUS_DIR = 0xFF;

    ADDR_PORT = 0;
    ADDR_DIR = 0;

    CTRL_PORT = CTRL_IDLE;
    CTRL_DIR = 0;
}

/*
 * One-off LCD command byte. Slow but who cares?
 */
void lcd_cmd_byte(uint8_t b)
{
    CTRL_PORT = CTRL_LCD_CMD;
    BUS_DIR = 0;
    BUS_PORT = b;
    ADDR_PORT++;
    ADDR_PORT++;
    BUS_DIR = 0xFF;
    CTRL_PORT = CTRL_IDLE;
}

/*
 * One-off LCD data byte. Really slow, for setup only.
 */
void lcd_data_byte(uint8_t b)
{
    BUS_DIR = 0;
    BUS_PORT = b;
    ADDR_PORT++;
    ADDR_PORT++;
    BUS_DIR = 0xFF;
}

/*
 * Lookup table that can burst up to 32 pixels at a time.
 * We probably spend so much time setting this up that the savings are negated :P
 */
void lcd_addr_burst(uint8_t pixels)
{
    pixels;

__asm
    ; Equivalent to:
    ; a = 0x100 - (pixels << 3)

    clr a
    clr c
    subb a, dpl
    rl a
    rl a
    rl a
    anl a, #0xF8

    mov DPTR, #table
    jmp @a+DPTR

table:
    ; This table holds 128 copies of "inc ADDR_PORT".
    ; It occupies exactly 256 bytes in memory, and the full table represents 32 pixels worth of burst transfer.

    .db 5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT  ; 1 x 8
    .db 5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT  ; 2 x 8
    .db 5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT  ; 3 x 8
    .db 5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT  ; 4 x 8
    .db 5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT  ; 5 x 8
    .db 5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT  ; 6 x 8
    .db 5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT  ; 7 x 8
    .db 5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT  ; 8 x 8
    .db 5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT  ; 9 x 8
    .db 5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT  ; 10 x 8
    .db 5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT  ; 11 x 8
    .db 5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT  ; 12 x 8
    .db 5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT  ; 13 x 8
    .db 5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT  ; 14 x 8
    .db 5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT  ; 15 x 8
    .db 5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT,5,ADDR_PORT  ; 16 x 8
__endasm ;
}

/*
 * Flexible copy from any flash address to the LCD
 */
void lcd_flash_copy(uint32_t addr, uint16_t pixel_count)
{
    for (;;) {
	uint8_t addr_high = (uint8_t)(addr >> 13) & 0xFE;
	uint8_t addr_mid  = (uint8_t)(addr >> 6) & 0xFE;
	uint8_t addr_low  = addr << 1;

	ADDR_PORT = addr_high;
	CTRL_PORT = CTRL_IDLE | CTRL_FLASH_LAT2;
	ADDR_PORT = addr_mid;
	CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT1;
	ADDR_PORT = addr_low;

	if (pixel_count > 64 && addr_low == 0) {
	    lcd_addr_burst(32);
	    lcd_addr_burst(32);
	    pixel_count -= 64;
	    addr += 128;

	} else {
	    uint8_t burst_count = 32 - (addr_low >> 3);

	    if (burst_count >= pixel_count) {
		lcd_addr_burst(burst_count);
		CTRL_PORT = CTRL_IDLE;
		return;
	    } else {
		lcd_addr_burst(burst_count);
		pixel_count -= burst_count;
		addr += burst_count << 1;
	    }
	}
    }
}

/*
 * Optimized copy, assumes everything is perfectly aligned and always
 * blits a full screen of data.
 */
void lcd_flash_copy_fullscreen(uint32_t addr)
{
    uint8_t addr_high = (uint8_t)(addr >> 13) & 0xFE;
    uint8_t addr_mid  = (uint8_t)(addr >> 6) & 0xFE;
    uint8_t chunk = 0;

    do {
	uint8_t i = 8;  // 64 pixels, as eight 8-pixel chunks

	ADDR_PORT = addr_high;
	CTRL_PORT = CTRL_IDLE | CTRL_FLASH_LAT2;
	ADDR_PORT = addr_mid;
	CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT1;
	ADDR_PORT = 0;

	do {
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++;  // 0
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++;  // 1
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++;  // 2
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++;  // 3
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++;  // 4
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++;  // 5
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++;  // 6
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++;  // 7
	} while (--i);

	if (!(addr_mid += 2)) addr_high += 2;
    } while (--chunk);
}

/*
 * Arbitrary-sized chromakey blit. If we hit a pixel that matches our
 * chromakey, we show data from a background image instead of the
 * foreground.
 */
void lcd_flash_chromakey(uint32_t fgAddr, uint32_t bgAddr, uint16_t pixel_count)
{
    do {
	uint8_t fg_high = (uint8_t)(fgAddr >> 13) & 0xFE;
	uint8_t fg_mid  = (uint8_t)(fgAddr >> 6) & 0xFE;
	uint8_t fg_low  = fgAddr << 1;

	uint8_t bg_high = (uint8_t)(bgAddr >> 13) & 0xFE;
	uint8_t bg_mid  = (uint8_t)(bgAddr >> 6) & 0xFE;
	uint8_t bg_low  = bgAddr << 1;

	// XXX: fg/bg may have different alignment
	uint8_t burst_count = 32 - (fg_low >> 3);
	uint8_t i;
	if (burst_count > pixel_count)
	    burst_count = pixel_count;

	ADDR_PORT = fg_high;
	CTRL_PORT = CTRL_IDLE | CTRL_FLASH_LAT2;
	ADDR_PORT = fg_mid;
	CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT1;
	ADDR_PORT = fg_low;

	fgAddr += burst_count << 1;
	bgAddr += burst_count << 1;
	pixel_count -= burst_count;

	for (i = burst_count; i--;) {
	    if (BUS_PORT == CHROMA_KEY) {
		// Transparent pixel, swap addresses

		ADDR_PORT = bg_high;
		CTRL_PORT = CTRL_IDLE | CTRL_FLASH_LAT2;
		ADDR_PORT = bg_mid;
		CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT1;
		ADDR_PORT = bg_low + ((burst_count - i - 1) << 2);
		
		ADDR_PORT++;
		ADDR_PORT++;
		ADDR_PORT++;
		ADDR_PORT++;

		ADDR_PORT = fg_high;
		CTRL_PORT = CTRL_IDLE | CTRL_FLASH_LAT2;
		ADDR_PORT = fg_mid;
		CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT1;
		ADDR_PORT = fg_low + ((burst_count - i - 1) << 2);

	    } else {
		ADDR_PORT++;
		ADDR_PORT++;
		ADDR_PORT++;
		ADDR_PORT++;
	    }
	}
    } while (pixel_count);
}

/*
 * Optimized chroma-key blit. Assumes perfect alignment, and copies a
 * full screen of data.
 */
void lcd_flash_chromakey_fullscreen(uint32_t fgAddr, uint32_t bgAddr)
{
    uint8_t fg_high = (uint8_t)(fgAddr >> 13) & 0xFE;
    uint8_t fg_mid  = (uint8_t)(fgAddr >> 6) & 0xFE;
    
    uint8_t bg_high = (uint8_t)(bgAddr >> 13) & 0xFE;
    uint8_t bg_mid  = (uint8_t)(bgAddr >> 6) & 0xFE;

    uint8_t chunk = 0;

    ADDR_PORT = fg_high;
    CTRL_PORT = CTRL_IDLE | CTRL_FLASH_LAT2;
    ADDR_PORT = fg_mid;
    CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT1;
    ADDR_PORT = 0;

    do {
	uint8_t cycle = 8;

	do {
	    // Partially unrolled loop, 64 = 8x8

#define CHROMA_TEST 							\
	    if (BUS_PORT == CHROMA_KEY) { 				\
		uint8_t a = ADDR_PORT; 					\
									\
		ADDR_PORT = bg_high; 					\
		CTRL_PORT = CTRL_IDLE | CTRL_FLASH_LAT2;		\
		ADDR_PORT = bg_mid; 					\
		CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT1;		\
		ADDR_PORT = a; 						\
									\
		ADDR_PORT++; ADDR_PORT++; ADDR_PORT++;ADDR_PORT++;	\
									\
		ADDR_PORT = fg_high;					\
		CTRL_PORT = CTRL_IDLE | CTRL_FLASH_LAT2;		\
		ADDR_PORT = fg_mid;					\
		CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT1;		\
		ADDR_PORT = a + 4;					\
	    } else {							\
		ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++;	\
	    }

	    CHROMA_TEST CHROMA_TEST CHROMA_TEST CHROMA_TEST
	    CHROMA_TEST CHROMA_TEST CHROMA_TEST CHROMA_TEST

#undef CHROMA_TEST

	} while (--cycle);

	if (!(fg_mid += 2)) fg_high += 2;
	if (!(bg_mid += 2)) bg_high += 2;

	// Must load the first pixel of the next chunk, for the colorkey test
	ADDR_PORT = fg_high;
	CTRL_PORT = CTRL_IDLE | CTRL_FLASH_LAT2;
	ADDR_PORT = fg_mid;
	CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT1;
	ADDR_PORT = 0;

    } while (--chunk);
}

/*
 * Tile graphics!
 *
 * There are a lot of tile modes we could use. For example, the
 * 7x3=21-bit address bus is really well suited for a layout that has
 * 128 accessible 8x8 tiles at a time, since we then need to change
 * only one of the address bytes per-tile.
 *
 * However, even a relatively simple game like Chroma Shuffle couldn't
 * use that mode. with an 8x8 tile size, it would need at least 640
 * tiles, which is quite a bit beyond this hypothetical mode's
 * 128-tile limit.
 *
 * There are probably tons of uses for a 8x8 mode with a 16-bit tile
 * ID- for example, games that use a lot of text, and need to be able
 * to combine text with graphics.
 *
 * But for this demo, I'm going the other direction and implementing a
 * 16x16 mode with 8-bit tile IDs. Each tile occupies an aligned
 * 512-byte chunk of memory. The tilemap is 8x8, and only occupies 64
 * bytes.
 *
 * For simplicity, we're still requiring that a tile segment begins on
 * a 16kB boundary.
 *
 * This mode isn't especially suited for arbitrary text, but it's good
 * for puzzle games where the game pieces are relatively large.
 */
void lcd_render_tiles_16x16_8bit(uint8_t segment)
{
    uint8_t map_index = 0;
    uint8_t y_low = 0;
    uint8_t y_high = 0;

    do {
	// Loop over map columns
	do {
	    uint8_t tile_index = tilemap[map_index++];
		
	    ADDR_PORT = segment + ((tile_index >> 4) & 0x0E);
	    CTRL_PORT = CTRL_IDLE | CTRL_FLASH_LAT2;
	    ADDR_PORT = y_high + (tile_index << 3);
	    CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT1;
	    ADDR_PORT = y_low;

	    // Burst out one row of one tile (16 pixels)

	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; // 1
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; // 2
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; // 3
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; // 4
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; // 5
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; // 6
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; // 7
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; // 8
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; // 9
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; // 10
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; // 11
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; // 12
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; // 13
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; // 14
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; // 15
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; // 16
	    
	} while (map_index & 7);

	/*
	 * We can store four tile rows per address chunk.
	 * After that, roll over y_high. We'll do four such
	 * roll-overs per 16 pixel tile.
	 */

	y_low += 64;
	if (y_low)
	    map_index += 0xF8;  // No map advancement
	else {
	    y_high += 2;
	    if (y_high & 8)
		y_high = 0;
	    else
		map_index += 0xF8;  // No map advancement
	}
    } while (!(map_index & 64));
}

void lcd_render_sprites_32x32(uint8_t segment)
{
    uint8_t x, y;

    // We keep the segment constant. Everything has to fit in 16 kB for this mode.
    ADDR_PORT = segment;
    CTRL_PORT = CTRL_IDLE | CTRL_FLASH_LAT2;

    y = LCD_HEIGHT;
    do {
	uint8_t sx0, sx1, sx2, sx3;
	uint8_t sah0, sah1, sah2, sah3;
	uint8_t sal0, sal1, sal2, sal3;

	x = LCD_WIDTH;

	/*
	 * Prepare each row. If a sprite doesn't occur at all on this
	 * row, disable it by interpreting its column position as
	 * off-screen, so that the X test below will hide the sprite.
	 *
	 * We'll also pre-calculate the Y portion of the address for
	 * any visible sprite.
	 */
#define SPRITE_ROW(i)							\
	if (oam[i].y++ & 0xE0)						\
	    sx##i = 0x80;						\
	else {								\
	    sx##i = oam[i].x;						\
	    sah##i = (oam[i].y & 0x1E) | (i << 5);			\
	    sal##i = (oam[i].y & 1) << 7;				\
	}

	SPRITE_ROW(0)
	SPRITE_ROW(1)
	SPRITE_ROW(2)
        SPRITE_ROW(3)

	do {
	    sx0++;
	    sx1++;
	    sx2++;
	    sx3++;

#define SPRITE_TEST(i)    						\
	    if (!(sx##i & 0xE0)) {					\
		CTRL_PORT = CTRL_IDLE;					\
		ADDR_PORT = sah##i;					\
		CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT1;		\
		ADDR_PORT = sal##i | (sx##i << 2);			\
		if (BUS_PORT != CHROMA_KEY) {				\
		    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++;	\
		    continue;						\
		}							\
	    }								\

	    SPRITE_TEST(0)
            SPRITE_TEST(1)
	    SPRITE_TEST(2)
	    SPRITE_TEST(3)

	    /* Nothing found. Draw a dummy background from the end of the segment. */

 	    CTRL_PORT = CTRL_IDLE;
	    ADDR_PORT = 0xFE;
	    CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT1;
	    ADDR_PORT = 0xFE;
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++;

	} while (--x);

    } while (--y);

    oam[0].y += 128;
    oam[1].y += 128;
    oam[2].y += 128;
    oam[3].y += 128;
}

// Signed 8-bit sin()
int8_t sin8(uint8_t angle)
{
    static const code int8_t lut[] = {
	0x00, 0x03, 0x06, 0x09, 0x0c, 0x10, 0x13, 0x16,
	0x19, 0x1c, 0x1f, 0x22, 0x25, 0x28, 0x2b, 0x2e,
	0x31, 0x33, 0x36, 0x39, 0x3c, 0x3f, 0x41, 0x44,
	0x47, 0x49, 0x4c, 0x4e, 0x51, 0x53, 0x55, 0x58,
	0x5a, 0x5c, 0x5e, 0x60, 0x62, 0x64, 0x66, 0x68,
	0x6a, 0x6b, 0x6d, 0x6f, 0x70, 0x71, 0x73, 0x74,
	0x75, 0x76, 0x78, 0x79, 0x7a, 0x7a, 0x7b, 0x7c,
	0x7d, 0x7d, 0x7e, 0x7e, 0x7e, 0x7f, 0x7f, 0x7f,
    };

    if (angle & 0x80) {
	if (angle & 0x40)
	    return -lut[255 - angle];
	else
	    return -lut[angle & 0x3F];
    } else {
	if (angle & 0x40)
	    return lut[127 - angle];
	else
	    return lut[angle];
    }
}

void lcd_render_affine_64x64(uint8_t segment, uint8_t angle, uint8_t scale)
{
    uint8_t y, x;
    int16_t x_acc = 0;
    int16_t y_acc = 0;
    
    int16_t sin_val = (sin8(angle) * (int16_t)scale) >> 6;
    int16_t cos_val = (sin8(ANGLE_90 - angle) * (int16_t)scale) >> 6;

    uint8_t sx, sy, osx = 0xFF, osy = 0xFF;
    uint8_t addr;

    // We keep the segment constant. Everything has to fit in 16 kB for this mode.
    ADDR_PORT = segment;
    CTRL_PORT = CTRL_IDLE | CTRL_FLASH_LAT2;

    y = LCD_HEIGHT;
    do {
	int16_t x_acc_row = x_acc;
	int16_t y_acc_row = y_acc;

	x = LCD_WIDTH;
	do {
	    x_acc_row += cos_val;
	    y_acc_row += sin_val;

	    sx = x_acc_row >> 8;
	    sy = y_acc_row >> 8;

	    /*
	     * XXX: Could speed this up by replacing these explicit comparisons
	     *      with inline asm that uses the flags resulting from updating
	     *      the low byte of x_acc_row/y_acc_row. We already know whether
	     *      we switched pixels, since it was a byte overflow!
	     */

	    if (sy != osy) {
		CTRL_PORT = CTRL_IDLE;
		ADDR_PORT = (sy << 1) & 0x7E;
		CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT1;
		osy = sy;
	    }

	    if (sx != osx) {
		addr = sx << 2;
		osx = sx;
	    }

	    ADDR_PORT = addr;
	    ADDR_PORT++; ADDR_PORT++; ADDR_PORT++; ADDR_PORT++;

	} while (--x);

	x_acc -= sin_val;
	y_acc += cos_val;

    } while (--y);
}

void gems_draw_gem(uint8_t x, uint8_t y, uint8_t index)
{
    // A gem is 32x32, a.k.a. a 2x2 tile grid
    index <<= 2;
    TILE(3, 0 + (x << 1), 0 + (y << 1)) = 0 + index;
    TILE(3, 1 + (x << 1), 0 + (y << 1)) = 1 + index;
    TILE(3, 0 + (x << 1), 1 + (y << 1)) = 2 + index;
    TILE(3, 1 + (x << 1), 1 + (y << 1)) = 3 + index;
}

uint32_t xor128(void)
{
    // Simple PRNG.
    // Reference: http://en.wikipedia.org/wiki/Xorshift

    static uint32_t x = 123456789;
    static uint32_t y = 362436069;
    static uint32_t z = 521288629;
    static uint32_t w = 88675123;
    uint32_t t;
 
    t = x ^ (x << 11);
    x = y; y = z; z = w;
    return w = w ^ (w >> 19) ^ (t ^ (t >> 8));
}

void gems_init()
{
    // Draw a uniform grid of gems, all alike.

    uint8_t x, y;

    for (y = 0; y < 4; y++)
	for (x = 0; x < 4; x++)
	    gems_draw_gem(x, y, 0);
}

void gems_shuffle()
{
    // Mix it up, replace a few pseudo-random gems.

    uint32_t r = xor128();
    uint8_t i = 4;

    do {
	uint8_t gem = (uint8_t)r % 40;
	uint8_t x = (r >> 6) & 3;
	uint8_t y = (r >> 4) & 3;

	gems_draw_gem(x, y, gem);

	r >>= 8;
    } while (--i);
}

void monster_init(uint8_t id)
{
    uint32_t r = xor128();

    oam[id].x = 170 + ((r >> 0) & 63);
    oam[id].y = 170 + ((r >> 8) & 63);
    oam[id].xd = ((r >> 15) & 15) - 8;
    oam[id].yd = ((r >> 23) & 15) - 8;
}

void monster_update(uint8_t id)
{
    oam[id].x = oam[id].x + oam[id].xd;
    oam[id].y = oam[id].y + oam[id].yd;

    // Bounce (Yes, very weird coord system)
    if (oam[id].x < 160)
	oam[id].xd = -oam[id].xd;
    if (oam[id].y < 160)
	oam[id].yd = -oam[id].yd;
}


/*
 * IT IS DEMO TIME.
 */

void main()
{
    hardware_init();

    while (1) {
	uint16_t frame;

	// Background only
	if (1) {
	    for (frame = 0; frame < 256; frame++) {
		uint32_t bg_addr = 0x40000LU + ((uint32_t)(frame & 0xFF) << (LCD_ROW_SHIFT + 1));
		lcd_cmd_byte(LCD_CMD_RAMWR);
		lcd_flash_copy_fullscreen(bg_addr);
	    }
	}

	// Full-screen sprite only
	if (1) {
	    for (frame = 0; frame < 256; frame++) {
		uint8_t spr_f = (frame >> 2) & 7;
		uint32_t spr_addr = (uint32_t)spr_f << 15;
		lcd_cmd_byte(LCD_CMD_RAMWR);
		lcd_flash_copy_fullscreen(spr_addr);
	    }
	}
	
	// Chroma key
	if (1) {
	    for (frame = 0; frame < 256; frame++) {
		uint8_t spr_f = (frame >> 1) & 7;
		uint32_t spr_addr = (uint32_t)spr_f << 15;
		uint32_t bg_addr = 0x40000LU + ((uint32_t)(frame & 0x7F) << (LCD_ROW_SHIFT + 2));
		lcd_cmd_byte(LCD_CMD_RAMWR);
		lcd_flash_chromakey_fullscreen(spr_addr, bg_addr);
	    }
	}

	// Static 16x16 tile graphics (Chroma Extra-lite)
	if (1) {
	    gems_init();
	    for (frame = 0; frame < 256; frame++) {
		lcd_cmd_byte(LCD_CMD_RAMWR);
		lcd_render_tiles_16x16_8bit(0x68000 >> 13);
		gems_shuffle();
	    }
	}

	// Dynamic 32x32 sprites
	if (1) {
	    uint8_t i;
	    for (i = 0; i < NUM_SPRITES; i++)
		monster_init(i);
	    for (frame = 0; frame < 128; frame++) {
		lcd_cmd_byte(LCD_CMD_RAMWR);
		lcd_render_sprites_32x32(0x88000 >> 13);
		for (i = 0; i < NUM_SPRITES; i++)
		    monster_update(i);
	    }
	}

	// Some oldskool rotozooming, why not?
	if (1) {
	    for (frame = 0; frame < 128; frame++) {
		uint8_t frame_l = frame;
		lcd_cmd_byte(LCD_CMD_RAMWR);
		lcd_render_affine_64x64(0x8c000 >> 13, 0xc0 - frame, 0xa0 - frame);
	    }
	}	
    }
}
