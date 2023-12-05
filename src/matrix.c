static void
_matrix_command(u8 command, u8 data)
{
	// See: Timing Diagram @ Source(26) @ Figure(1) @ Page(6).
	pin_low(PIN_MATRIX_SS);
	spi_tx(command);
	spi_tx(data);
	pin_high(PIN_MATRIX_SS);
}

static void
matrix_set(u64 bits)
{
	// See: "Register Address Map" @ Source(26) @ Table(2) @ Page(7).
	_matrix_command(0x01, (bits >>  0) & 0xFF);
	_matrix_command(0x02, (bits >>  8) & 0xFF);
	_matrix_command(0x03, (bits >> 16) & 0xFF);
	_matrix_command(0x04, (bits >> 24) & 0xFF);
	_matrix_command(0x05, (bits >> 32) & 0xFF);
	_matrix_command(0x06, (bits >> 40) & 0xFF);
	_matrix_command(0x07, (bits >> 48) & 0xFF);
	_matrix_command(0x08, (bits >> 56) & 0xFF);
}

// Initializes 8x8 LED matrix equipped with MAX7219 driver.
// Requirements:
//     - PIN_MATRIX_SS must be provided.
//     - SPI must be configured to have leading MSb. See: Timing Diagram @ Source(26) @ Figure(1) @ Page(6).
//     - SPI must be below 10MHz. See: "CLK" @ Source(26) @ Page(5).
static void
matrix_init(void)
{
	pin_output(PIN_MATRIX_SS);
	pin_high(PIN_MATRIX_SS);

	_matrix_command(0x0C, 1); // Activate display. See: "Shutdown Register Format" @ Source(26) @ Table(3) @ Page(7).
	_matrix_command(0x09, 0); // Disable BCD decoding. See: "Decode-Mode Register Examples" @ Source(26) @ Table(4) @ Page(7).
	_matrix_command(0x0A, 0); // Set LED intensity to minimum to reduce power consumption (and eye-soreness). See: "Intensity Register Format" @ Source(26) @ Table(7) @ Page(9).
	_matrix_command(0x0B, 7); // Allow displaying of all digits (which are columns in the 8x8 matrix). See "Scan-Limit Register Format" @ Source(26) @ Table(8) @ Page(9).
	_matrix_command(0x0F, 0); // Exit display test mode. See: "Display-Test Register Format" @ Source(26) @ Table(10) @ Page(10).

	matrix_set(0);
}
