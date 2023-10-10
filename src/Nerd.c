#define F_CPU 16'000'000
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include "defs.h"
#include "pin.c"

int
main(void)
{
	for(;;)
	{
		debug_pin_set(13, false);
		_delay_ms(1000.0);
		debug_pin_set(13, true);
		_delay_ms(1000.0);
	}
}
