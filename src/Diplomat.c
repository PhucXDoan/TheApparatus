#define F_CPU 16'000'000
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "defs.h"
#include "pins.c"
#include "usb.c"

int
main(void)
{
	sei(); // Activates global interrupts. See: Source(1) @ Section(4.4) @ Page(11).

	usb_init();

	while (true)
	{
		debug_u8(0xAA);
		_delay_ms(100.0);
		debug_u8(0x55);
		_delay_ms(100.0);
	}
}
