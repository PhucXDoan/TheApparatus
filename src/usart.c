// Defines basic asynchronous serial communication through a single USART.
//
//     - USART_N must be defined to indicate which USART hardware to use.
//       For simplicity, multiple USARTs are not directly supported.
//     - The USART hardware and firmware is also pretty much identical across AVR devices,
//       so the ATmega32U4 datasheet will be primarily referred to within this file.

// See: "USART Register Description" @ Source(1) @ Section(11.81) @ Page(209).
#define UCSRnA concat(concat(UCSR, USART_N), A)
#define UCSRnB concat(concat(UCSR, USART_N), B)
#define UCSRnC concat(concat(UCSR, USART_N), C)
#define U2Xn          concat(U2X , USART_N)
#define RXENn         concat(RXEN, USART_N)
#define TXENn         concat(TXEN, USART_N)
#define UCSZn0 concat(concat(UCSZ, USART_N), 0)
#define UCSZn1 concat(concat(UCSZ, USART_N), 1)
#define UBRRn         concat(UBRR, USART_N)
#define UPMn0  concat(concat(UPM , USART_N), 0)
#define UPMn1  concat(concat(UPM , USART_N), 1)
#define UDREn         concat(UDRE, USART_N)
#define UDRn          concat(UDR , USART_N)
#define RXCn          concat(RXC , USART_N)
#define FEn           concat(FE  , USART_N)
#define DORn          concat(DOR , USART_N)
#define UPEn          concat(UPE , USART_N)

static void
usart_init(void)
{
	UCSRnA = (1 << U2Xn);                 // See: "Double the USART Transmission Speed" @ Source(1) @ Section(18.11.2) @ Page(210).
	UCSRnB = (1 << TXENn) | (1 << RXENn); // Enable transmission and reception of data. See: Source(1) @ Section(18.11.3) @ Page(211).
	UCSRnC =
		(1 << UPMn1 ) | (1 << UPMn0 ) |   // Enable odd parity bit. See: Source(1) @ Table(18-8) @ Page(212).
		(1 << UCSZn1) | (1 << UCSZn0);    // 8-bit data. See: Source(1) @ Table(18-10) @ Page(212).
	UBRRn = 1;                            // Set baud-rate. See: "Examples of UBRRn Settings for Commonly Used Oscillator Frequencies" @ Source(27) @ Table(22-12) @ Page(226).
}

static void
usart_tx(u8 value)
{ // See: C Code Example @ Source(1) @ Section(18.5.1) @ Page(195).

	while (!(UCSRnA & (1 << UDREn))); // Wait for data register to be empty. See: "USART Data Register Empty" @ Source(1) @ Section(18.11.2) @ Page(209).
	UDRn = value;
}

#define usart_rx_available() (!!(UCSRnA & (1 << RXCn)))

static u8
usart_rx(void)
{ // See: C Code Example @ Source(1) @ Section(18.6.1) @ Page(198).

	if (UCSRnA & (1 << DORn)) // See: "Data OverRun" @ Source(1) @ Section(18.11.2) @ Page(210).
	{
		error(); // We missed some data!
	}

	while (!usart_rx_available()); // Wait for reception to be completed. See: "USART Receive Complete" @ Source(1) @ Section(18.11.2) @ Page(209).

	if (UCSRnA & (1 << FEn)) // See: "Frame Error" @ Source(1) @ Section(18.11.2) @ Page(210).
	{
		error(); // We expected a stop-bit!
	}

	if (UCSRnA & (1 << UPEn)) // See: "USART Parity Error" @ Source(1) @ Section(18.11.2) @ Page(210).
	{
		error(); // Parity bit mismatch!
	}

	return UDRn;
}
