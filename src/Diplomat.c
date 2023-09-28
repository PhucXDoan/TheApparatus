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

static const char message[] PROGMEM = "The work is mysterious and important.\n";

int
main(void)
{
	UENUM = 1;
	sei();

	debug_cstr("The work is mysterious and important.\n");

	usb_init();

	pin_output(2);
	for (;;)
	{
		char buf[16] = {0};
		debug_read(buf, sizeof(buf) - 1);
		debug_cstr(buf);
		_delay_ms(100.0);
	}
}
