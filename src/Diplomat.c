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
#include <stdint.h>
#include <string.h>
#include "defs.h"
#include "pin.c"
#include "misc.c"
#include "spi.c"
#include "sd.c"
#include "Diplomat_usb.c"
#undef  PIN_HALT_SOURCE
#define PIN_HALT_SOURCE HaltSource_diplomat

static void
click(u8 x, u8 y)
{
	usb_mouse_command(false, x, y);
	_delay_ms(500.0);
	usb_mouse_command(true , x, y);
	_delay_ms(50.0);
	usb_mouse_command(false, x, y);
	_delay_ms(450.0);
}

int
main(void)
{
	if (USBCON != 0b0010'0000) // [Bootloader-Tampered State].
	{
		wdt_enable(WDTO_15MS);
		for(;;);
	}

	debug_u16(0);

	sei();
	spi_init();
	sd_init();
	usb_init();

	u8 wordgame = 0;

	#define ANAGRAMS_LANGUAGE_COUNT 6
	u8 prev_anagrams_language_index = 0;
	u8 curr_anagrams_language_index = 0;
	b8 anagrams_use_seven_letters   = false;

	#define WORDHUNT_MAP_COUNT 4
	u8 wordhunt_map_index = 0;

	for(;;)
	{
		//
		// Recalibrate.
		//

		usb_mouse_command(false, 0, 0);
		_delay_ms(500.0);

		click(45, 185); // Game Pigeon.
		click(14, 255); // Word Games.

		u8 play_y = 0;

		switch (wordgame)
		{
			case 0: // Anagrams:
			{
				click(30, 238);

				if (prev_anagrams_language_index)
				{
					click(22, 255);
				}
				prev_anagrams_language_index = curr_anagrams_language_index;

				click(19, 255); // Languages.
				click(22 + curr_anagrams_language_index * 17, 255);

				//if (curr_anagrams_language_index == 0)
				//{
				//	if (anagrams_use_seven_letters)
				//	{
				//		click(103, 255);
				//		curr_anagrams_language_index += 1;
				//	}
				//	else
				//	{
				//		click(67, 255);
				//	}

				//	anagrams_use_seven_letters = !anagrams_use_seven_letters;
				//}
				//else
				//{
				//	curr_anagrams_language_index += 1;
				//}

				curr_anagrams_language_index %= ANAGRAMS_LANGUAGE_COUNT;

				play_y = 200;
			} break;

			case 1: // WordHunt.
			{
				click(64, 238);
				click(24 + 27 * wordhunt_map_index, 255);

				wordhunt_map_index += 1;
				wordhunt_map_index %= WORDHUNT_MAP_COUNT;

				play_y = 212;
			} break;

			case 2: // WordBites.
			{
				click(99, 238);
				play_y = 212;
			} break;
		}

		click(121, 168); // Send.
		_delay_ms(2000.0);

		click(64, 120); // Open game.
		click(122, 25); // Close game.
		click(64, 120); // Open game.
		click(64, play_y); // Start.
		_delay_ms(1000.0);

		click(10, 12); // Screenshot.
		_delay_ms(6000.0);

		click(122, 25); // Close game.

//		wordgame += 1;
//		wordgame %= 3;
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
