#define SD_CMD8_ARGUMENT                0x00000'1'AA // See: Source(19) @ Table(7-5) @ AbsPage(119).
#define SD_CMD8_CRC7                    0x43         // See: [CRC7 Calculation].
#define SD_MAX_COMMAND_RETRIES          8192
#define SD_MAX_COMMAND_RESPONSE_LATENCY (u16(1) << 15)
#undef  PIN_HALT_SOURCE
#define PIN_HALT_SOURCE HaltSource_sd

[[nodiscard]]
static u8 // First byte of the response. MSb being set means the procedure timed out.
_sd_command(enum SDCommand command, u32 argument)
{
	//
	// See: "Commands" @ Source(19) @ Section(4.7) @ AbsPage(57).
	//

	spi_tx
	(
		(0 << 7) | // "Start bit".
		(1 << 6) | // "Transmission bit".
		command
	);

	spi_tx((argument >> 24) & 0xFF);
	spi_tx((argument >> 16) & 0xFF);
	spi_tx((argument >>  8) & 0xFF);
	spi_tx((argument >>  0) & 0xFF);

	// CRC7 field followed by the end bit; CRC is ignored by default in most commands, but not all. See: Source(19) @ Section(7.2.2) @ AbsPage(107) & [CRC7 Calculation].
	switch (command)
	{
		case SDCommand_GO_IDLE_STATE : spi_tx((0x4A         << 1) | 1); break;
		case SDCommand_SEND_IF_COND  : spi_tx((SD_CMD8_CRC7 << 1) | 1); break;
		default                      : spi_tx(                      1); break;
	}

	//
	// See: "Responses" @ Source(19) @ Section(4.9) @ AbsPage(69).
	//

	u8  response = 0;
	u16 attempts = 0;
	do
	{
		response  = spi_rx();
		attempts += 1;
	}
	while ((response & (1 << 7)) && attempts < SD_MAX_COMMAND_RESPONSE_LATENCY);

	return response;
}

[[nodiscard]]
static b8 // Success?
_sd_get_data_block(u8* dst_buffer, u16 dst_size) // See: "Data Transfer" @ Source(20).
{
	b8 success = false;

	for (u16 i = 0; i < SD_MAX_COMMAND_RESPONSE_LATENCY; i += 1)
	{
		if (spi_rx() == 0b1111'1110) // Starting token. See: Source(19) @ Section(7.3.3.2) @ AbsPage(122-123).
		{
			for (u16 i = 0; i < dst_size; i += 1)
			{
				dst_buffer[i] = spi_rx();
			}

			// 16-bit CRC that we ignore.
			(void) spi_rx();
			(void) spi_rx();

			success = true;
			break;
		}
	}

	return success;
}

static void
sd_read(u8* dst, u32 abs_sector_address) // dst must be at least FAT32_SECTOR_SIZE bytes.
{ // See: Source(19) @ Section(7.2.3) @ AbsPage(107).

	pin_low(PIN_SD_SS);

	// "READ_SINGLE_BLOCK" uses the R1 format for the response.
	// Any bit being set will indicate an error.
	// If the MSb is set, then _sd_command timed out.
	// See: Source(20) @ Section(7.3.2.1) @ AbsPage(120).
	// See: Source(20) @ Table(4-19) @ AbsPage(61).

	if (_sd_command(SDCommand_READ_SINGLE_BLOCK, abs_sector_address))
	{
		error; // Failed to command.
	}
	else if (!_sd_get_data_block(dst, FAT32_SECTOR_SIZE))
	{
		error; // Failed to receive data.
	}

	pin_high(PIN_SD_SS);
}

static void
sd_write(u8* src, u32 abs_sector_address) // src must be at least FAT32_SECTOR_SIZE bytes.
{ // See: Source(19) @ Section(7.2.4) @ AbsPage(108).

	pin_low(PIN_SD_SS);

	// "WRITE_BLOCK" responds with R1 format, similar to "READ_SINGLE_BLOCK". See: Source(19) @ Table(4-20) @ AbsPage(62).
	if (_sd_command(SDCommand_WRITE_BLOCK, abs_sector_address))
	{
		error; // Failed to command.
	}
	else
	{
		//
		// Send data block.
		//

		spi_tx(0b1111'1110); // Starting token. See: Source(19) @ Section(7.3.3.2) @ AbsPage(122-123).
		for (u16 i = 0; i < FAT32_SECTOR_SIZE; i += 1)
		{
			spi_tx(src[i]);
		}

		//
		// Wait for data response.
		//

		u16 attempts = 0;
		while (true)
		{
			u8 response = spi_rx();

			if (response == 0xFF) // SD still busy with parsing command.
			{
				if (attempts < SD_MAX_COMMAND_RESPONSE_LATENCY)
				{
					attempts += 1;
				}
				else
				{
					error; // Timed out waiting for SD's response to the data that we sent.
				}
			}
			else if ((response & 0b000'11111) == 0b000'0'010'1) // See: "Data accepted" @ Source(19) @ Section(7.3.3.1) @ AbsPage(122).
			{
				break;
			}
			else
			{
				error; // Unknown response received.
			}
		}

		//
		// Wait for SD to be finished with writing.
		//

		attempts = 0;
		while (true)
		{
			u8 response = spi_rx();

			if (response == 0xFF) // When the SD is finished, the data line will be released from the low state.
			{
				break;
			}
			else if (response)
			{
				error; // Unknown response received.
			}
			else if (attempts < SD_MAX_COMMAND_RESPONSE_LATENCY)
			{
				attempts += 1;
			}
			else
			{
				error; // Timed out waiting for the SD to finish writing.
			}
		}
	}

	pin_high(PIN_SD_SS);
}

static u32 // Sector count.
sd_init(void) // Depends on SPI being MSb sent first and samples taken on rise.
{ // See: Source(19) @ Section(4.2.2) @ AbsPage(24).

	pin_output(PIN_SD_SS);
	pin_high(PIN_SD_SS);

	for (u8 i = 0; i < 255; i += 1) // Doing this improves reliability after powering up.
	{
		spi_tx(0xFF);
	}

	//
	// Perform software reset.
	//

	u16 attempts = 0;
	while (true)
	{
		pin_low(PIN_SD_SS);
		u8 response = _sd_command(SDCommand_GO_IDLE_STATE, 0);
		pin_high(PIN_SD_SS);

		if (response == SDR1ResponseFlag_in_idle_state) // See: "In idle state" @ Source(19) @ Section(7.3.2.1) @ AbsPage(120).
		{
			break;
		}
		else if (attempts < SD_MAX_COMMAND_RETRIES)
		{
			attempts += 1;
		}
		else
		{
			error; // Timed out waiting for the SD to restart; SD card potentially not inserted.
		}
	}

	//
	// Mandatory command before initialization can begin. See: "CMD8" @ Source(19) @ Table(7-5) @ AbsPage(119).
	//

	pin_low(PIN_SD_SS);
	{
		// The command uses the 5-byte R7 response format where the first byte identical to R1. See: Source(19) @ Section(7.3.2.6) @ AbsPage(122).
		if (_sd_command(SDCommand_SEND_IF_COND, SD_CMD8_ARGUMENT) != SDR1ResponseFlag_in_idle_state)
		{
			error; // SD went out of idle mode, or the command timed-out.
		}

		(void) spi_rx(); // Irrelevant command version in high-nibble, rest are reserved.
		(void) spi_rx(); // Reserved bits.

		u16 echo_back = 0;
		echo_back   = spi_rx() << 8;  // See: "voltage accepted" @ Source(19) @ Table(4-34) @ AbsPage(71).
		echo_back  |= spi_rx();       // Echo-back pattern provided by SD_CMD8_ARGUMENT.
		echo_back  &= 0b0000'1111'1111'1111;

		if (echo_back != SD_CMD8_ARGUMENT)
		{
			error; // Echoed values do not match.
		}
	}
	pin_high(PIN_SD_SS);

	//
	// Initialize SD.
	//

	attempts = 0;
	while (true)
	{
		pin_low(PIN_SD_SS);

		// The application-specific command "SD_SEND_OP_COND" must be sent with a prefixing command.
		if (_sd_command(SDCommand_APP_CMD, 0) & ~SDR1ResponseFlag_in_idle_state)
		{
			error; // Error signaled in R1 response, or the command timed-out.
		}

		// Attempt to initialize SD and let it know we support high-capacity cards (HCS bit). Responds with R1.
		u8 response = _sd_command(SDCommand_SD_SEND_OP_COND, ((u32) 1) << 30);

		pin_high(PIN_SD_SS);

		if (!response) // Finished initialization.
		{
			break;
		}
		else if (response & ~SDR1ResponseFlag_in_idle_state)
		{
			error; // Error signaled in R1 response, or the command timed out.
		}
		else if (attempts < SD_MAX_COMMAND_RETRIES)
		{
			attempts += 1;
		}
		else
		{
			error; // Timed out waiting for the SD to initialize.
		}
	}

	//
	// Query SD for "card-specific data" (CSD).
	//

	pin_low(PIN_SD_SS);
	if (_sd_command(SDCommand_SEND_CSD, 0))
	{
		error; // Error signaled in R1 response, or the command timed out.
	}
	pin_high(PIN_SD_SS);

	//
	// Receive the card-specific data. See: Source(19) @ Table(5-16) @ AbsPage(97).
	//

	pin_low(PIN_SD_SS);
	u8 csd[16];
	if (!_sd_get_data_block(csd, countof(csd)))
	{
		error; // Failed to get CSD data-block.
	}
	pin_high(PIN_SD_SS);

	u8  csd_READ_BL_LEN  = csd[5] & 0xF;
	u8  csd_WRITE_BL_LEN = ((csd[12] & 0b11) << 2) | (csd[13] >> 6);
	u32 csd_C_SIZE       = (((u32) (csd[7] & 0b00'111111)) << 16) | (((u32) csd[8]) << 8) | csd[9];

	if
	(
		!(
			(((u64) 1) << csd_READ_BL_LEN ) == FAT32_SECTOR_SIZE &&
			(((u64) 1) << csd_WRITE_BL_LEN) == FAT32_SECTOR_SIZE
		)
	)
	{
		error; // Card has unexpected/unsupported parameters.
	}

	static_assert(FAT32_SECTOR_SIZE == 512);
	return (csd_C_SIZE + 1) * 1024; // See: "C_SIZE" @ Source(19) @ Section(5.3.3) @ AbsPage(98).
}

//
// Documentation.
//

/* [Overview].
	This file just defines a thin interface to be able to read and write sectors of SD cards.
	It does not handle file systems itself, nor handle all SD cards in existence.
	Your milage may vary!
*/

/* [CRC7 Calculation].
	// The following JS carries out CRC7, which is used for error detection on some commands.
	// The input stream begins at the start bit and terminates right before the end bit.
	// Example : iterate(bits_of("Start(0) Transmission(1) Command(000000) Argument(00000000000000000000000000000000) CRC(1001010)"))
	// Expect  : [0, 0, 0, 0, 0, 0, 0]
	// See: Source(19) @ Section(4.5) @ AbsPage(54).

	let bits_of = str =>
		str
			.split("")
			.filter(x => x == "0" || x == "1")
			.map(x => parseInt(x))

	let crc = bits_of(((1 << 7) | (1 << 3) | (1 << 0)).toString(2))

	let iterate = curr_stream =>
		crc.length > curr_stream.length
			? curr_stream
			: curr_stream[0]
				? iterate(crc.map((x, i) => x ^ curr_stream[i]).concat(curr_stream.slice(crc.length)), crc)
				: iterate(curr_stream.slice(1), crc)
*/
