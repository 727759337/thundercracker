/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * nRF Radio + Graphics Engine + ??? = Profit!
 *
 * M. Elizabeth Scott <beth@sifteo.com>
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include <stdint.h>
#include "hardware.h"

static __xdata union {
    uint8_t bytes[1024];
    struct {
	uint8_t tilemap[800];
	uint8_t pan_x, pan_y;
    };
} vram;

static void rf_reg_write(uint8_t reg, uint8_t value)
{
    RF_CSN = 0;

    SPIRDAT = RF_CMD_W_REGISTER | reg;
    SPIRDAT = value;
    while (!(SPIRSTAT & SPI_RX_READY));
    SPIRDAT;
    while (!(SPIRSTAT & SPI_RX_READY));
    SPIRDAT;

    RF_CSN = 1;
}

void rf_isr(void) __interrupt(VECTOR_RF)
{
    register uint8_t i;
    register uint8_t __xdata *wptr;
    
    /*
     * Write the STATUS register, to clear the IRQ.
     * We also get a STATUS read for free here.
     */

    RF_CSN = 0;					// Begin SPI transaction
    SPIRDAT = RF_CMD_W_REGISTER |		// Command byte
	RF_REG_STATUS;
    SPIRDAT = 0x70;				// Prefill TX FIFO with STATUS write
    while (!(SPIRSTAT & SPI_RX_READY));		// Wait for Command/STATUS byte
    SPIRDAT;					// Dummy read of STATUS byte
    while (!(SPIRSTAT & SPI_RX_READY));		// Wait for payload byte
    SPIRDAT;					// Dummy read
    RF_CSN = 1;					// End SPI transaction

    /*
     * If we had reason to check for other interrupts, we could use
     * the value of STATUS we read above to do so. But in this case,
     * we only get IRQs for incoming packets, so we're guaranteed to
     * have a packet waiting for us in the FIFO right now. Read it.
     */

    RF_CSN = 0;					// Begin SPI transaction
    SPIRDAT = RF_CMD_R_RX_PAYLOAD;		// Command byte
    SPIRDAT = 0;				// First dummy write, keep the TX FIFO full
    while (!(SPIRSTAT & SPI_RX_READY));		// Wait for Command/STATUS byte
    SPIRDAT;					// Dummy read of STATUS byte

    SPIRDAT = 0;				// Write next dummy byte
    while (!(SPIRSTAT & SPI_RX_READY));		// Wait for payload byte
    i = SPIRDAT;				// Read payload byte (Block address)
    wptr = vram.bytes; 				// Calculate offset to 31-byte block
    wptr += i << 5;
    wptr -= i;

    i = RF_PAYLOAD_MAX - 2;
    do {
	SPIRDAT = 0;				// Write next dummy byte
	while (!(SPIRSTAT & SPI_RX_READY));	// Wait for payload byte
	*(wptr++) = SPIRDAT;			// Read payload byte
    } while (--i);

    while (!(SPIRSTAT & SPI_RX_READY));		// Wait for last payload byte
    *wptr = SPIRDAT;				// Read last payload byte
    RF_CSN = 1;					// End SPI transaction
}

static void lcd_addr_burst(uint8_t pixels)
{
    uint8_t hi = pixels >> 3;
    uint8_t low = pixels & 0x7;

    if (hi)
	// Fast DJNZ loop, 8-pixel bursts
	do {
	    ADDR_INC32();
	} while (--hi);

    if (low)
	// Fast DJNZ loop, single pixels
	do {
	    ADDR_INC4();
	} while (--low);
}

static void lcd_render_tiles_8x8_16bit_20wide(void)
{
    uint8_t pan_x = vram.pan_x;
    uint8_t pan_y = vram.pan_y;
    uint8_t tile_pan_x = pan_x >> 3;
    uint8_t tile_pan_y = pan_y >> 3;
    uint8_t line_addr = pan_y << 5;
    uint8_t first_column_addr = (pan_x << 2) & 0x1C;
    uint8_t last_tile_width = pan_x & 7;
    uint8_t first_tile_width = 8 - last_tile_width;
    __xdata uint8_t *map_line = &vram.tilemap[(tile_pan_y << 5) + (tile_pan_y << 3)];
    uint8_t y = LCD_HEIGHT;

    do {
	uint8_t x;
	uint8_t map_x = tile_pan_x << 1;

	/*
	 * XXX: There is a HUGE optimization opportunity here with regard to map addressing.
	 *      The dptr manipulation code that SDCC generates here is very bad... and it's
	 *      possible we might want to use some of the nRF24LE1's specific features, like
	 *      the PDATA addressing mode.
	 *
	 * XXX: I rather unscrupulously hacked this to add full horizontal and vertical
	 *      wrap-around support. There's also a lot of room for optimization here.
	 */

	// First tile on the line, (1 <= width <= 8)
	ADDR_PORT = map_line[map_x++];
	CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT1;
	ADDR_PORT = map_line[map_x++];
	CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT2;
	ADDR_PORT = line_addr + first_column_addr;
	lcd_addr_burst(first_tile_width);
	if (map_x == 40)
	    map_x = 0;

	// There are always 15 full tiles on-screen
	x = 15;
	do {
	    ADDR_PORT = map_line[map_x++];
	    CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT1;
	    ADDR_PORT = map_line[map_x++];
	    CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT2;
	    ADDR_PORT = line_addr;
	    ADDR_INC32();
	    if (map_x == 40)
		map_x = 0;
	} while (--x);

	// Might be one more partial tile, (0 <= width <= 7)
	if (last_tile_width) {
	    ADDR_PORT = map_line[map_x++];
	    CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT1;
	    ADDR_PORT = map_line[map_x++];
	    CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT2;
	    ADDR_PORT = line_addr;
	    lcd_addr_burst(last_tile_width);
	}

	line_addr += 32;
	if (!line_addr) {
	    map_line += 40;
	    if (map_line > &vram.tilemap[799])
		map_line -= 800;
	}

    } while (--y);
}

void lcd_cmd_byte(uint8_t b)
{
    CTRL_PORT = CTRL_LCD_CMD;
    BUS_DIR = 0;
    BUS_PORT = b;
    ADDR_INC2();
    BUS_DIR = 0xFF;
    CTRL_PORT = CTRL_IDLE;
}

void main(void)
{
    // I/O port init
    BUS_DIR = 0xFF;
    ADDR_PORT = 0;
    ADDR_DIR = 0;
    CTRL_PORT = CTRL_IDLE;
    CTRL_DIR = 0;

    // Radio clock running
    RF_CKEN = 1;

    // Enable RX interrupt
    rf_reg_write(RF_REG_CONFIG, RF_STATUS_RX_DR);
    IEN_EN = 1;
    IEN_RF = 1;

    // Start receiving
    RF_CE = 1;

    while (1) {
	lcd_cmd_byte(LCD_CMD_RAMWR);
	lcd_render_tiles_8x8_16bit_20wide();
    }
}
