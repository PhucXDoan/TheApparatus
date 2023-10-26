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

	for(;;)
	{
		char c;
		if (debug_rx(&c, 1))
		{
			switch (c)
			{
				case 'A':
				{
					usb_mouse_command(false, 10, 128);
				} break;

				case 'D':
				{
					usb_mouse_command(false, 120, 120);
				} break;

				case 'S':
				{
					usb_mouse_command(false, 64, 10);
				} break;

				case 'W':
				{
					usb_mouse_command(false, 64, 250);
				} break;

				case ' ':
				{
					//
					// Disconnect.
					//

					cli();
					USBCON &= ~(1 << USBE); // See: Source(1) @ Section(22.2) @ Page(270).

					//
					// Wipe relevant sectors.
					//

					memset(sd_sector, 0, sizeof(sd_sector));
					for (u32 abs_sector_address = 0; abs_sector_address < FAT32_WIPE_SECTOR_COUNT; abs_sector_address += 1)
					{
						sd_write(abs_sector_address);
						debug_u16(abs_sector_address);
					}

					//
					// Write FAT32 file system.
					//

					#define MAKE(SECTOR_DATA, SECTOR_ADDRESS) \
						{ \
							memcpy_P(sd_sector, &(SECTOR_DATA), sizeof(SECTOR_DATA)); \
							sd_write((SECTOR_ADDRESS)); \
						}
					FAT32_SECTOR_XMDT(MAKE)
					#undef MAKE

					//
					// Restart.
					//

					wdt_enable(WDTO_15MS);
					for(;;);
				} break;

				default:
				{
					debug_tx_chars(&c, 1);
				} break;
			}
		}
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
