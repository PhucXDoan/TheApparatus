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
	sei(); // Activates global interrupts. @ Source(1) @ Section(4.4) @ Page(11).

	usb_init();

	DDRC   = (1 << DDC7);
	PORTC &= ~(1 << PORTC7);
	_delay_ms(1000.0);

	for (;;)
	{
		PORTC ^= (1 << PORTC7);
		_delay_ms(1000.0);
	}

	error:;
	DDRC = (1 << DDC7);
	for (;;)
	{
		pin_set(13, !pin_read(13));
		_delay_ms(50.0);
	}
}
