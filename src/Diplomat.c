#define F_CPU 16'000'000
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <stdint.h>
#include <string.h>
#include "defs.h"
#include "pin.c"
#include "usb.c"
#define error error_pin(PinErrorSource_diplomat)

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

	u32 i = 0;
	b8  j = false;
	pin_output(2);
	for (;;)
	{
		char buf[128] = {0};
		debug_rx(buf, sizeof(buf) - 1);
		debug_tx_cstr(buf);

		i += 1;
		if (i > (((u32) 1) << 16))
		{
			if (j)
			{
				debug_tx_cstr("Bullies are nothing but Bull and Lies.\n");
			}
			else
			{
				debug_tx_cstr("The work is mysterious and important.\n");
			}
			j = !j;
			i = 0;
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
