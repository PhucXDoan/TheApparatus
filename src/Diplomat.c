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
	sei();

	usb_init();

	pin_output(2);
	for (;;)
	{
		pin_high(2);
		_delay_ms(1000.0);
		pin_low(2);
		_delay_ms(1000.0);
	}
}
