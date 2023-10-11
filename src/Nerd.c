#define F_CPU 16'000'000
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include <string.h>
#include "defs.h"
#include "misc.c"
#include "pin.c"
#include "Nerd_uart.c"
#include "Nerd_lcd.c"
#undef  PIN_HALT_SOURCE
#define PIN_HALT_SOURCE PinHaltSource_nerd

static u8 spi_packet_byte_index = 0;

ISR(SPI_STC_vect)
{
	if (spi_packet_writer_masked(1) == spi_packet_reader_masked(0))
	{
		debug_halt(1);
	}
	else
	{
		((u8*) &spi_packet_buffer[spi_packet_writer_masked(0)])[spi_packet_byte_index] = SPDR;

		if (spi_packet_byte_index == sizeof(struct SPIPacket) - 1)
		{
			spi_packet_byte_index  = 0;
			spi_packet_writer     += 1;
		}
		else
		{
			spi_packet_byte_index += 1;
		}
	}
}

int
main(void)
{
	sei();

	uart_init();

	pin_output(PIN_SPI_MOSI);
	SPCR = (1 << SPIE) | (1 << SPE) | (1 << SPR1) | (1 << SPR0);

	u64 packet_index = 0;
	for (;;)
	{
		if (spi_packet_reader_masked(0) != spi_packet_writer_masked(0))
		{
			struct SPIPacket packet = spi_packet_buffer[spi_packet_reader_masked(0)];
			spi_packet_reader += 1;

			u8 accumulator = 0;
			for (u8 i = 0; i < sizeof(packet); i += 1)
			{
				accumulator += ((u8*) &packet)[i];
			}
			if (accumulator)
			{
				debug_halt(2);
			}

			if (packet.byte_count > countof(packet.byte_buffer))
			{
				debug_halt(3);
			}

			if (!(packet.source < SPIPacketSource_COUNT))
			{
				debug_halt(4);
			}

			debug_tx_cstr("[Packet ");
			debug_tx_u64(packet_index);
			debug_tx_cstr(" | Line ");
			debug_tx_u64(packet.line_number);
			debug_tx_cstr(" | ");
			debug_tx_u64(packet.byte_count);
			debug_tx_cstr(" Byte(s)] ");
			debug_tx_cstr(SPI_PACKET_SOURCE_NAMES[packet.source]);
			for (u8 i = 0; i < packet.byte_count; i += 1)
			{
				if (i % 16)
				{
					debug_tx_cstr(" ");
				}
				else
				{
					debug_tx_cstr("\n");
				}

				debug_tx_H8(packet.byte_buffer[i]);
			}
			debug_tx_cstr("\n\n");

			packet_index += 1;
		}
	}
}
