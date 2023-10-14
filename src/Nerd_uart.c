// TODO Old code stuff. Gotta redocument this.

// Initializes USART0 in asynchronous mode for transmitting only in BN1 format.
// Receiving inputs is not supported.
static void
uart_init(void)
{
#if 0
	/*
		The baud rate is calculated as follows for asynchronous mode without 2x-speed
		(see table on page 203 in the datasheet):

			BAUD = F_CPU / (16 * (UBRRn + 1))

		With some algebra, or using the table that also happens to nicely provide the formula,
		the appropiate UBRRn value at some desired baud rate:

			UBRRn = F_CPU / (16 * BAUD) - 1

		With a desired baud rate of 9600 and ATmega2560's frequency of 16MHz:

			UBRRn
				= 16,000,000 / (16 * 9600) - 1
				= 103.16666...

		Using `UBRR0 = 103`, we can achieve a baud rate of 9600 at ~0.2 percent error.
		This agrees with the table on page 226.
	*/

	UBRR0  = 103;        // 12-bit USART-Baud-Rate register (consult page 222).
	UCSR0B = 1 << TXEN0; // USART-MSPIM-Control-Status-B register with Transmitter-Enable bit (consult page 234).

	// UCSRnC (USART-Control-Status-C register) controls the mode of the USART operation.
	// By default, this is asynchronous USART with no parity bit and one stop bit (BN1 format).
#endif

	UBRR0  = 12;
	UCSR0B = 1 << TXEN0;
}

#if DEBUG
static void
debug_tx_chars(char* value, u16 length)
{
	for (u16 i = 0; i < length; i += 1)
	{
		// USART-MSPIM-Control-Status-A register has a USART-Data-Register-Empty bit that indicates
		// whether or not the transmission is ready to recieve new data. See page 233.
		while (!(UCSR0A & (1 << UDRE0)));
		UDR0 = value[i]; // See page 218 about the USART-IO-Data register.
	}
}
#endif

#if DEBUG
static void
debug_tx_cstr(char* value)
{
	debug_tx_chars(value, strlen(value));
}
#endif

#if DEBUG
static void
debug_tx_u64(u64 value)
{
	char buffer[20];
	u8   length = serialize_u64(buffer, countof(buffer), value);
	debug_tx_chars(buffer, length);
}
#endif

#if DEBUG
static void
debug_tx_i64(i64 value)
{
	char buffer[20];
	u8   length = serialize_i64(buffer, countof(buffer), value);
	debug_tx_chars(buffer, length);
}
#endif

static void
debug_tx_H8(u8 value)
{
	char digits[2] = {0};
	digits[0] = ((value >> 4) & 0x0F) < 10 ? '0' + ((value >> 4) & 0x0F) : 'A' + (((value >> 4) & 0x0F) - 10);
	digits[1] = ((value >> 0) & 0x0F) < 10 ? '0' + ((value >> 0) & 0x0F) : 'A' + (((value >> 0) & 0x0F) - 10);
	debug_tx_chars(digits, countof(digits));
}
