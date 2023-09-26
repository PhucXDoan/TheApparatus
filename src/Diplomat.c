#define F_CPU 16'000'000
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <stdint.h>
#include <string.h>
#include "defs.h"
#include "pins.c"
#include "usb.c"

int
main(void)
{
	UENUM = 1;
	sei();

	usb_in_cstr("aslkdjlasjdlksajd!\n");

	usb_init();

	pin_output(2);
	for (;;)
	{
		pin_high(2);
		usb_in_cstr("meow!\n");
		_delay_ms(100.0);

		pin_low(2);
		usb_in_cstr("bark!\n");
		_delay_ms(100.0);
	}
}
