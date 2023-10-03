#define F_CPU 16'000'000
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <stdint.h>
#include <string.h>
#include "defs.h"
#include "misc.c"
#include "pin.c"
#include "usb.c"
#undef  PIN_HALT_SOURCE
#define PIN_HALT_SOURCE PinHaltSource_usb

int
main(void)
{
	if (USBCON != 0b0010'0000) // [Bootloader-Tampered State].
	{
		wdt_enable(WDTO_15MS);
		for(;;);
	}

	sei();

	usb_init();

	debug_pin_set(5, PinState_input);

	for(;;)
	{
		if (!debug_pin_read(5))
		{
			for (u8 i = 0; i < 32; i += 1)
			{
				usb_mouse_command
				(
					(struct USBMouseCommand)
					{
						.dest_x = 25 + (i % 8) * 25,
						.dest_y = 50 + (i / 4) * 15,
						.held   = !!i,
					}
				);
			}

			usb_mouse_command
			(
				(struct USBMouseCommand)
				{
					.dest_x = 18,
					.dest_y = 235,
					.held   = false,
				}
			);
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
