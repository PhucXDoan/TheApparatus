#define F_CPU 16'000'000
#define PROGRAM_NERD 1
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdint.h>
#include <string.h>
#include "defs.h"
#include "misc.c"
#include "pin.c"
#include "Nerd_uart.c"
#undef  PIN_HALT_SOURCE
#define PIN_HALT_SOURCE PinHaltSource_nerd

static volatile char spi_buffer[256] = {0};
static volatile u8   spi_writer      = 0;
static volatile u8   spi_reader      = 0;

ISR(SPI_STC_vect)
{
	if (spi_writer + 1 == spi_reader)
	{
		error;
	}
	else
	{
		pin_high(13);
		spi_buffer[spi_writer] = SPDR;
		spi_writer += 1;
		pin_low(13);
	}
}

int
main(void)
{
	sei();

	uart_init();

	pin_output(13);
	pin_output(PIN_SPI_MISO);
	SPCR = (1 << SPIE) | (1 << SPE) | (1 << SPR1) | (1 << SPR0);

	u8 column = 0;
	for (;;)
	{
		if (spi_reader != spi_writer)
		{
			char byte = spi_buffer[spi_reader];
			spi_reader += 1;

			debug_tx_H8(byte);

			if (column == 15)
			{
				column = 0;
				debug_tx_cstr("\n");
			}
			else if (column == 7)
			{
				column += 1;
				debug_tx_cstr("  ");
			}
			else
			{
				column += 1;
				debug_tx_cstr(" ");
			}
		}
	}
}
