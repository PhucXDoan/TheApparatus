#define F_CPU            16'000'000
#define PROGRAM_DIPLOMAT true
#define BOARD_LEONARDO   true
#define PIN_USB_BUSY     2
#define PIN_U16_CLK      4
#define PIN_U16_DATA     5
#define PIN_SD_SS        7
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
#include "Diplomat_usb.c"
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

	while (true)
	{
		char input = {0};
		if (debug_rx(&input, 1))
		{
			switch (input)
			{
				case 'W':
				{
					cli();

					USBCON &= ~(1 << USBE);

					memset(sd_sector, 0, sizeof(sd_sector));
					for (u32 i = 0; i < FAT32_WIPE_SECTOR_COUNT; i += 1)
					{
						debug_u16(i);
						sd_write(i);
					}

					#define MAKE(SECTOR_DATA, SECTOR_ADDRESS) \
						{ \
							static_assert(sizeof(sd_sector) == sizeof(SECTOR_DATA)); \
							memcpy_P(sd_sector, &(SECTOR_DATA), sizeof(SECTOR_DATA)); \
							sd_write(SECTOR_ADDRESS); \
						}
					FAT32_SECTOR_XMDT(MAKE);
					#undef MAKE

					restart();
				} break;

				case ' ':
				{
					for (u8 y = 0; y < pgm_read_byte(&WORDGAME_INFO[usb_ms_ocr_wordgame].board_dim_slots.y); y += 1)
					{
						for (u8 x = 0; x < pgm_read_byte(&WORDGAME_INFO[usb_ms_ocr_wordgame].board_dim_slots.x); x += 1)
						{
							switch (usb_ms_ocr_grid[y][x])
							{
								#define MAKE(NAME) case Letter_##NAME: debug_tx_cstr(#NAME); break;
								LETTER_XMDT(MAKE)
								#undef MAKE
								case Letter_COUNT: debug_tx_cstr("Letter_COUNT"); break;
							}

							if (x == pgm_read_byte(&WORDGAME_INFO[usb_ms_ocr_wordgame].board_dim_slots.x) - 1)
							{
								debug_tx_cstr("\n");
							}
							else
							{
								debug_tx_cstr(" ");
							}
						}
					}
				} break;
			}
		}

	//	click(0, 0);

	//	click(10, 12);
	//	_delay_ms(1500.0);

	//	click(64, 46);

	//	click(0, 0);

	//	_delay_ms(4000.0);
	}
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
