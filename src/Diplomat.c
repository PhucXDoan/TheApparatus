#define F_CPU 16'000'000
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <stdint.h>
#include <string.h>
#include "defs.h"
#include "pin.c"
#include "usb.c"
#define error error_pin(PinErrorSource_diplomat)

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
		char buf[16] = {0};
		usb_out_chars(buf, sizeof(buf) - 1);
		usb_in_cstr(buf);
		usb_in_cstr(__FILE_NAME__ "\n");
		_delay_ms(100.0);
	}
}
