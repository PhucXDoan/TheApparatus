#define F_CPU 16'000'000
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
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

#if 0
	usb_mouse_command
	(
		(struct USBMouseCommand)
		{
			.dest_x   = 255,
			.dest_y   = 255,
			.behavior = USBMouseButtonBehavior_released
		}
	);

	usb_mouse_command
	(
		(struct USBMouseCommand)
		{
			.dest_x   = 0,
			.dest_y   = 0,
			.behavior = USBMouseButtonBehavior_released
		}
	);

	for (;;)
	{
		usb_mouse_command
		(
			(struct USBMouseCommand)
			{
				.dest_x   = 5,
				.dest_y   = 15,
				.behavior = USBMouseButtonBehavior_released
			}
		);
		_delay_ms(1000.0);
		usb_mouse_command
		(
			(struct USBMouseCommand)
			{
				.dest_x   = 35,
				.dest_y   = 5,
				.behavior = USBMouseButtonBehavior_released
			}
		);
		_delay_ms(1000.0);
	}
#else
	u64 i = 0;
	for(;;)
	{
		char c;
		if (debug_rx(&c, 1))
		{
			debug_tx_chars(&c, 1);
			debug_tx_cstr(" : ");
			debug_tx_u64(i);
			debug_tx_cstr(" : The work is mysterious and important.\n");
			i += 1;

			usb_mouse_command
			(
				(struct USBMouseCommand)
				{
					.dest_x   = 141,
					.dest_y   = 120,
					.behavior = USBMouseButtonBehavior_released
				}
			);
			usb_mouse_command
			(
				(struct USBMouseCommand)
				{
					.dest_x   = 90,
					.dest_y   = 20,
					.behavior = USBMouseButtonBehavior_released
				}
			);
			usb_mouse_command
			(
				(struct USBMouseCommand)
				{
					.dest_x   = 34,
					.dest_y   = 20,
					.behavior = USBMouseButtonBehavior_released
				}
			);
		}
	}
#endif
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
