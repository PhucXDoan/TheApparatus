#if PROGRAM_DIPLOMAT
	static void
	spi_tx(u8 value) // See: "SPI_MasterTransmit" @ Source(1) @ Section(17) @ Page(181).
	{
		SPDR = value;
		while (!(SPSR & (1 << SPIF))); // We must wait so we don't accidentally overwrite SPDR while it's still being sent.
	}

	[[nodiscard]]
	static u8
	spi_rx(void) // Dummy 0xFF is sent.
	{
		SPDR = 0xFF;
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
			(((SPI_PRESCALER >> 1) & 1) << SPR1) |
			(0 << DORD);                           // MSb sent first.

		SPSR = (((SPI_PRESCALER >> 2) & 1) << SPI2X);
	}
#endif
