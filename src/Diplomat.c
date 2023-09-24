#define F_CPU 16'000'000
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <string.h>
#include "defs.h"
#include "pins.c"
#include "usb.c"

int
main(void)
{
	sei(); // Activates global interrupts. See: Source(1) @ Section(4.4) @ Page(11).

	usb_init();

	pin_output(2);
	while (true)
	{
		pin_high(2);
		_delay_ms(1000.0);
		pin_low(2);
		_delay_ms(1000.0);
	}
}
