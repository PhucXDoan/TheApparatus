#define F_CPU 16'000'000
#define PROGRAM_NERD         true
#define BOARD_MEGA_2560_REV3 true
#define PIN_MATRIX_SS        2
#define PIN_SD_SS            3
#define SPI_PRESCALER        SPIPrescaler_2
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdint.h>
#include <string.h>
#include "defs.h"
#include "pin.c"
#include "misc.c"
#include "uart.c"
#include "spi.c"
#include "matrix.c"
#include "sd.c"
#undef  PIN_HALT_SOURCE
#define PIN_HALT_SOURCE HaltSource_nerd

__attribute__((noreturn))
static void
error_(u16 line_number, enum HaltSource source)
{
	cli();

	#if DEBUG
	matrix_set((((u64) line_number) << 32) | source);
	#endif

	pin_output(HALT);
	for (;;)
	{
		pin_high(HALT);
		_delay_ms(25.0);
		pin_low(HALT);
		_delay_ms(25.0);
	}
}

int
main(void)
{
	uart_init();
	spi_init();
	matrix_init();

	sd_init();

	sd_read(0);

	for (u16 i = 0; i < countof(sd_sector); i += 1)
	{
		if (i % 16)
		{
			debug_tx_cstr(" ");
		}
		else
		{
			debug_tx_cstr("\n");
		}
		debug_tx_8H(sd_sector[i]);
	}

	for(;;);
}
