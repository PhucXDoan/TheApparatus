// SPI to transmit and receive bytes.
//
//     - SPI_PRESCALER must be defined to specify SPI clock frequency.
//     - The SPI hardware and firmware is also pretty much identical across AVR devices, so the ATmega32U4 datasheet will be primarily referred to within this file.

static void
spi_tx(u8 value)
{ // See: "SPI_MasterTransmit" @ Source(1) @ Section(17) @ Page(181).

	SPDR = value;
	while (!(SPSR & (1 << SPIF))); // Wait so we don't accidentally overwrite SPDR while it's still being sent (by calling spi_rx() right after for example).
}

[[nodiscard]]
static u8
spi_rx(void)
{
	SPDR = 0xFF; // Dummy byte.
	while (!(SPSR & (1 << SPIF)));
	return SPDR;
}

static void
spi_init(void)
{ // See: "SPI_MasterInit" @ Source(1) @ Section(17) @ Page(181).

	pin_output(SPI_SS);   // See: "Master Mode" @ Source(1) @ Section(17.1.2) @ Page(183).
	pin_output(SPI_MOSI);
	pin_output(SPI_CLK);

	SPCR =                                     // See: "SPI Control Register" @ Source(1) @ Section(17.2.1) @ Page(185-186).
		(1 << SPE ) |                          // "SPI Enable".
		(1 << MSTR) |                          // "Master/Slave Select", i.e. set the device as master.
		(0 << CPOL) | (0 << CPHA) |            // Sample on rise, set on fall.
		(((SPI_PRESCALER >> 0) & 1) << SPR0) |
		(((SPI_PRESCALER >> 1) & 1) << SPR1);
	SPSR = (((SPI_PRESCALER >> 2) & 1) << SPI2X);
}
