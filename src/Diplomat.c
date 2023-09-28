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
#include "misc.c"
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

	u64 counter = 0;
	for (;;)
	{
		char key_buffer[8];
		u8   key_length = debug_rx(key_buffer, countof(key_buffer));
		for (u8 key_index = 0; key_index < key_length; key_index += 1)
		{
			char serialized_counter_buffer[20];
			u8   serialized_counter_length = serialize_u64(serialized_counter_buffer, countof(serialized_counter_buffer), counter);

			char serialized_key_buffer[20];
			u8   serialized_key_length = serialize_u64(serialized_key_buffer, countof(serialized_key_buffer), key_buffer[key_index]);

			debug_tx_chars(serialized_counter_buffer, serialized_counter_length);
			debug_tx_cstr(" : '");
			debug_tx_chars(serialized_key_buffer, serialized_key_length);
			debug_tx_cstr("'\n");

			counter += 1;
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
