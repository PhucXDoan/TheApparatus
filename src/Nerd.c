#define F_CPU                16'000'000
#define PROGRAM_NERD         true
#define BOARD_MEGA_2560_REV3 true
#define PIN_MATRIX_SS        2
#define PIN_SD_SS            3
#define SPI_PRESCALER        SPIPrescaler_2
#define USART_N              3
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdint.h>
#include <string.h>
#include "defs.h"
#include "pin.c"
#include "misc.c"
#include "usart.c"
#include "spi.c"
#include "sd.c"
#if DEBUG
	#include "matrix.c"
#endif

#if DEBUG
	static void
	debug_chars(char* data, u16 length)
	{
		for (u16 i = 0; i < length; i += 1)
		{
			while (!(UCSR0A & (1 << UDRE0))); // Wait for USART transmission buffer to free up.
			UDR0 = data[i];
		}
	}

	static void
	debug_dump(char* file_path, u16 line_number)
	{
		u64 bits            = ((u64) line_number) << 48;
		u8  file_name_index = 0;
		for (u8 i = 0; file_path[i]; i += 1)
		{
			if (file_path[i] == '/' || file_path[i] == '\\')
			{
				file_name_index = i + 1;
			}
		}
		for
		(
			u8 i = 0;
			i < 6 && file_path[file_name_index + i];
			i += 1
		)
		{
			bits |= ((u64) file_path[file_name_index + i]) << (i * 8);
		}
		matrix_init(); // Reinitialize matrix in case the dump occurred mid communication or we haven't had it initialized yet.
		matrix_set(bits);
		debug_cstr("\nDUMPED @ File(");
		debug_cstr(file_path);
		debug_cstr(") @ Line(");
		debug_u64(line_number);
		debug_cstr(").\n");
	}
#endif

int
main(void)
{
	#if DEBUG // Configure USART0 to have 2Mbps. See: Source(27) @ Table(22-12) @ Page(225).
		UCSR0A = 1 << U2X0;
		UBRR0  = 0;
		UCSR0B = 1 << TXEN0;
		debug_char('\n'); // If terminal is opened early, lots of junk could appear due to noise.
	#endif

	usart_init();
	spi_init();
	sd_init();

	#if DEBUG // 8x8 LED dot matrix to have visual debug info.
		matrix_init();
	#endif

	//
	// Wait for packet from Diplomat.
	//

	matrix_set
	(
		(((u64) 0b00000000) << (8 * 0)) |
		(((u64) 0b00111110) << (8 * 1)) |
		(((u64) 0b01000000) << (8 * 2)) |
		(((u64) 0b01111100) << (8 * 3)) |
		(((u64) 0b01000000) << (8 * 4)) |
		(((u64) 0b00111110) << (8 * 5)) |
		(((u64) 0b00000000) << (8 * 6)) |
		(((u64) 0b00000000) << (8 * 7))
	);

	struct DiplomatPacket diplomat_packet = {0};
//	for (u8 i = 0; i < sizeof(diplomat_packet); i += 1)
//	{
//		((u8*) &diplomat_packet)[i] = usart_rx();
//	}

	//
	//
	//

	matrix_set
	(
		(((u64) 0b00000000) << (8 * 0)) |
		(((u64) 0b00000000) << (8 * 1)) |
		(((u64) 0b01111110) << (8 * 2)) |
		(((u64) 0b00010010) << (8 * 3)) |
		(((u64) 0b00010010) << (8 * 4)) |
		(((u64) 0b00001100) << (8 * 5)) |
		(((u64) 0b00000000) << (8 * 6)) |
		(((u64) 0b00000000) << (8 * 7))
	);

	debug_cstr("WordGame: ");
	debug_u64(diplomat_packet.wordgame);
	debug_cstr("\n");

	for
	(
		i8 y = WORDGAME_MAX_DIM_SLOTS_Y - 1;
		y >= 0;
		y -= 1
	)
	{
		for (u8 x = 0; x < WORDGAME_MAX_DIM_SLOTS_X; x += 1)
		{
			debug_u64 (diplomat_packet.board[y][x]);
			debug_char(' ');
		}
		debug_char('\n');
	}

	u8 counter = 0;
	while (true)
	{
		if (usart_rx_available()) // Diplomat signaled that it's ready for next command?
		{
			counter += 1;

			usart_rx(); // Eat dummy signal byte.
			usart_tx(counter);
			matrix_set
			(
				(
					(((u64) 0b00000000) << (8 * 0)) |
					(((u64) 0b00000000) << (8 * 1)) |
					(((u64) 0b00100100) << (8 * 2)) |
					(((u64) 0b01001010) << (8 * 3)) |
					(((u64) 0b01010010) << (8 * 4)) |
					(((u64) 0b00100100) << (8 * 5)) |
					(((u64) 0b00000000) << (8 * 6)) |
					(((u64) 0b00000000) << (8 * 7))
				) + counter
			);
		}
	}

	for(;;);
}
