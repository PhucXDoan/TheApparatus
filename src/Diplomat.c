#define F_CPU                   16'000'000
#define PROGRAM_DIPLOMAT        true
#define BOARD_PRO_MICRO         true
#define PIN_DEBUG_U16_DATA      2
#define PIN_DEBUG_U16_CLK       3
#define PIN_LCD_ENABLE          4
#define PIN_LCD_REGISTER_SELECT 5
#define PIN_LCD_DATA_4          6
#define PIN_LCD_DATA_5          7
#define PIN_LCD_DATA_6          8
#define PIN_LCD_DATA_7          9
#define PIN_ROTARY_BTN          10
#define PIN_ROTARY_CLK          A1
#define PIN_ROTARY_DAT          A3
#define PIN_USB_BUSY            A2
#define PIN_SD_SS               A0
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <util/crc16.h>
#include <stdint.h>
#include <string.h>
#include "defs.h"
#include "Diplomat_masks.h"
#include "pin.c"
#include "misc.c"
#include "spi.c"
#include "sd.c"
#include "usb.c"
#include "lcd.c"
#undef  PIN_HALT_SOURCE
#define PIN_HALT_SOURCE HaltSource_diplomat

//static void
//click(u8 x, u8 y)
//{
//	usb_mouse_command(false, x, y);
//	_delay_ms(500.0);
//	usb_mouse_command(true , x, y);
//	_delay_ms(50.0);
//	usb_mouse_command(false, x, y);
//	_delay_ms(450.0);
//}

int
main(void)
{
	if (USBCON != 0b0010'0000) // [Bootloader-Tampered State].
	{
		restart();
	}

	debug_u16(0);

	sei();
	spi_init();
	sd_init();
	usb_init();
	lcd_init();

	pin_input(PIN_ROTARY_BTN);
	pin_input(PIN_ROTARY_CLK);
	pin_input(PIN_ROTARY_DAT);

	b8 rotary_prev_clk  = pin_read(PIN_ROTARY_CLK);
	b8 rotary_btn       = false;
	u8 rotary_btn_delay = 0;

	i64 counter = 0;
	while (true)
	{
		b8 rotary_curr_clk = pin_read(PIN_ROTARY_CLK);

			debug_u16(0x00'00);

		if (rotary_curr_clk && !rotary_prev_clk)
		{
			if (pin_read(PIN_ROTARY_DAT))
			{
				counter -= 1;
			}
			else
			{
				counter += 1;
			}
		}

		if (pin_read(PIN_ROTARY_BTN)) // We read that the rotary button is released?
		{
			if (rotary_btn && rotary_btn_delay) // We currently think the rotary should still be pressed?
			{
				rotary_btn_delay -= 1;
				if (!rotary_btn_delay) // We're confident that the rotary is truly released now.
				{
					rotary_btn = false;
					counter -= 1;
				}
			}
			else
			{
				rotary_btn_delay = -1;
			}
		}
		else // We read that the rotary button is pressed?
		{
			if (!rotary_btn && rotary_btn_delay) // We currently think that the rotary should still released?
			{
				rotary_btn_delay -= 1;
				if (!rotary_btn_delay) // We're confident that the rotary is truly pressed now.
				{
					rotary_btn = true;
					counter += 1;
				}
			}
			else
			{
				rotary_btn_delay = -1;
			}
		}

		rotary_prev_clk = rotary_curr_clk;

		lcd_reset();
		lcd_i64(counter);
		lcd_refresh();
	}

//	while (true)
//	{
//		char input = {0};
//		if (debug_rx(&input, 1))
//		{
//			switch (input)
//			{
//				case 'W':
//				{
//					cli();
//
//					USBCON &= ~(1 << USBE);
//
//					memset(sd_sector, 0, sizeof(sd_sector));
//					for (u32 i = 0; i < FAT32_WIPE_SECTOR_COUNT; i += 1)
//					{
//						debug_u16(i);
//						sd_write(i);
//					}
//
//					#define MAKE(SECTOR_DATA, SECTOR_ADDRESS) \
//						{ \
//							static_assert(sizeof(sd_sector) == sizeof(SECTOR_DATA)); \
//							memcpy_P(sd_sector, &(SECTOR_DATA), sizeof(SECTOR_DATA)); \
//							sd_write(SECTOR_ADDRESS); \
//						}
//					FAT32_SECTOR_XMDT(MAKE);
//					#undef MAKE
//
//					restart();
//				} break;
//
//				case ' ':
//				{
//					for (u8 y = 0; y < pgm_read_byte(&WORDGAME_INFO[usb_ms_ocr_wordgame].board_dim_slots.y); y += 1)
//					{
//						for (u8 x = 0; x < pgm_read_byte(&WORDGAME_INFO[usb_ms_ocr_wordgame].board_dim_slots.x); x += 1)
//						{
//							switch (usb_ms_ocr_grid[y][x])
//							{
//								#define MAKE(NAME) case Letter_##NAME: debug_tx_cstr(#NAME); break;
//								LETTER_XMDT(MAKE)
//								#undef MAKE
//								case Letter_COUNT: debug_tx_cstr("Letter_COUNT"); break;
//							}
//
//							if (x == pgm_read_byte(&WORDGAME_INFO[usb_ms_ocr_wordgame].board_dim_slots.x) - 1)
//							{
//								debug_tx_cstr("\n");
//							}
//							else
//							{
//								debug_tx_cstr(" ");
//							}
//						}
//					}
//				} break;
//			}
//		}
//
//	//	click(0, 0);
//
//	//	click(10, 12);
//	//	_delay_ms(1500.0);
//
//	//	click(64, 46);
//
//	//	click(0, 0);
//
//	//	_delay_ms(4000.0);
//	}
}
//
// Documentation.
//

/* [Bootloader-Tampered State].
	For the ATmega32U4 that I have, the bootloader is executed whenever the MCU reinitializes from
	an external reset (RST was pulled low); other sources of resets do not. When the bootloader
	hands execution to the main program, it seems to leaves the USB-related registers (e.g. USBCON)
	not with their default values. This is pretty obviously inconvenient for us to set up USB
	correctly, and who knows what other registers are also tampered with by the bootloader!

	To resolve this, we just simply enable the watchdog timer and wait for it to trigger. This
	induces a hardware reset that won't have the MCU start with the bootloader messing around with
	our registers. AVR-GCC pretty much handles all for us, so don't have to worry about the minute
	details like the timed-writing sequences.

	See: "Watchdog Reset" @ Source(1) @ Section(8.6) @ Page(53-54).
	See: "Watchdog Timer" @ Source(1) @ Section(8.9) @ Page(55-56).
*/
