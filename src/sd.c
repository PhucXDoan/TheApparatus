#define SD_MAX_ATTEMPTS         512
#define _sd_R1_IDLE_FLAG        1
#define _sd_DATA_BLOCK_RESPONSE ((u8) ~1)
#define _sd_READ_SECTOR(BUFFER, ADDRESS) \
	do \
	{ \
		u8 READ_SECTOR_response = _sd_read_sector((BUFFER), (ADDRESS)); \
		if (READ_SECTOR_response != _sd_DATA_BLOCK_RESPONSE) \
		{ \
			debug_unhandled; \
		} \
	} \
	while (false)

static u8
_sd_command(u8 command, u32 arg)
{
	// Commands sent to the SD card are in packets of 6 bytes.
	// See "SD Card SPI Data Transfer Protocol" (Chlazza).

	spi_trade((1 << 6) | command); // The first bit sent by SPI is 0 which is then followed by a 1 (SPI transfers most-significant bit first).
	spi_trade((arg >> 24) & 0xFF); // Next four bytes are the argument.
	spi_trade((arg >> 16) & 0xFF);
	spi_trade((arg >>  8) & 0xFF);
	spi_trade((arg >>  0) & 0xFF);
	switch (command)                 // 7-bit CRC which is often ignored. Last bit sent (i.e. least-significant bit) is zero.
	{
		case 0  : spi_trade(0x95); break;
		case 8  : spi_trade(0x87); break;
		default : spi_trade(0xFF); break;
	}

	u8  response;
	u16 attempts = 0;
	do
	{
		response  = spi_trade(0xFF);
		attempts += 1;
	}
	while ((response & (1 << 7)) && attempts < SD_MAX_ATTEMPTS); // Most-significant bit will be set to zero for a response token.

	return response;
}

static u8
_sd_get_data_block(u8* dst_buffer, u16 dst_size)
{
	// See "Data Transfer" (Elm-Chan) for how data-blocks are received.

	u8  response;
	u16 attempts = 0;
	do
	{
		response  = spi_trade(0xFF);
		attempts += 1;
	}
	while (response == 0xFF && attempts < SD_MAX_ATTEMPTS);

	if (response == _sd_DATA_BLOCK_RESPONSE)
	{
		for (u16 i = 0; i < dst_size; i += 1)
		{
			dst_buffer[i] = spi_trade(0xFF);
		}

		spi_trade(0xFF); // 16-bit CRC bytes.
		spi_trade(0xFF);
	}

	return response;
}

static u8
_sd_read_sector(u8* dst_buffer, u32 address)
{
	u8 response = 0xFF;
	for (u16 i = 0; i < SD_MAX_ATTEMPTS && response == 0xFF; i += 1)
	{
		pin_low(PIN_SD_SS);
		_delay_ms(1.0);

		response = _sd_command(17, address); // CMD17 ("READ_SINGLE_BLOCK").
		if (response == 0x00)
		{
			response = _sd_get_data_block(dst_buffer, 512);
			/*
				The R1 response from CMD17 always has the most significant bit zero.
				If there was an error getting the data-block, we can use this bit
				to determine whether or not the error came from sending CMD17 or
				from waiting for the data-block. Of course, this makes it not possible
				to tell what the most significant bit response from waiting on the data
				block was (it could've been a zero or one, but we overwrite it regardless with
				a one), but this is a good enough compromise. `_sd_DATA_BLOCK_RESPONSE` consists
				of only a single zero as the least significant bit, so nothing will be affected
				in the successful case.
			*/
			response |= 1 << 7;
		}

		pin_high(PIN_SD_SS);
		_delay_ms(1.0);

	}
	return response;
}

static b8
_sd_write_sector(u8* dst_response, const u8* buffer, u32 address)
{
	pin_low(PIN_SD_SS);
	_delay_ms(1.0);

	b8 success = false;

	*dst_response = _sd_command(24, address); // CMD24 ("WRITE_BLOCK").
	if (*dst_response == 0x00)
	{
		spi_trade(_sd_DATA_BLOCK_RESPONSE);
		for (u16 i = 0; i < 512; i += 1)
		{
			spi_trade(buffer[i]);
		}

		{
			u16 attempts = 0;
			do
			{
				*dst_response  = spi_trade(0xFF);
				attempts      += 1;
			}
			while (*dst_response == 0xFF && attempts < SD_MAX_ATTEMPTS);
		}

		/*
			"Data Packet and Data Response" (Elm-Chan)
			------------------------------------------
			0x---00101 | Accepted.
			0x---01011 | Rejected due to CRC error.
			0x---01101 | Rejected due to write error.
		*/
		if ((*dst_response & ((1 << 5) - 1)) == 0x05)
		{
			u16 attempts = 0;
			do
			{
				*dst_response  = spi_trade(0xFF); // SD card will send `0` to indicate it is busy writing the data. See "Single Block Write" (Elm-Chan).
				attempts      += 1;
			}
			while (*dst_response == 0x00 && attempts < SD_MAX_ATTEMPTS);

			if (*dst_response == 0xFF)
			{
				success = true;
			}
		}
	}

	pin_high(PIN_SD_SS);
	_delay_ms(1.0);
	return success;
}

static void
sd_init(void) // Depends on the SPI being already initialized.
{
	pin_output(PIN_SD_SS);
	pin_high(PIN_SD_SS);

	_delay_ms(1.0);
	for (u16 i = 0; i < 512; i += 1) // Ready SPI communication with SD module.
	{
		spi_trade(0xFF);
	}
	_delay_ms(1.0);

	{ // CMD0 ("GO_IDLE_STATE"): Resets the card.
		u8  response;
		u16 attempts = 0;
		do
		{
			pin_low(PIN_SD_SS);
			_delay_ms(1.0);
			response  = _sd_command(0, 0);
			attempts += 1;
			pin_high(PIN_SD_SS);
			_delay_ms(1.0);
		}
		while (response != _sd_R1_IDLE_FLAG && attempts < 1024);

		if (response != _sd_R1_IDLE_FLAG)
		{
			debug_unhandled;
		}
	}
	pin_low(PIN_SD_SS);
	_delay_ms(1.0);
	{ // CMD8 ("SEND_IF_COND"): Checks the integrity of the SD card. This command exists only in newer specficiations (>= 2.00). See page 13 of datasheet.
		/*
			The CMD8 command must be executed before calling the following ACMD41 command.
			CMD8 takes an 8-bit check pattern in the least-significant byte of the argument.
			Following that is a nibble describing the voltage supply that the SD card is designed for (see table on page 40 of datasheet).
		*/
		u16 argument = 0x1AA;
		u8  response = _sd_command(8, argument);
		if (response != _sd_R1_IDLE_FLAG)
		{
			debug_unhandled;
		}

		// CM8 returns an R7 response. See page 111 of datasheet.

		spi_trade(0xFF); // Command version mixed
		spi_trade(0xFF); // with some reserved bits.

		u16 echoed = 0;
		echoed |= (u16) (spi_trade(0xFF) & 0xF) << 8;
		echoed |= spi_trade(0xFF);

		if (echoed != argument)
		{
			debug_unhandled;
		}
	}
	pin_high(PIN_SD_SS);
	_delay_ms(1.0);

	{ // ACMD41 ("SD_SEND_OP_COND"): Activates initialization process.
		u8  response;
		u16 attempts = 0;
		do
		{
			pin_low(PIN_SD_SS);
			_delay_ms(1.0);
			{ // CMD55 ("APP_CMD") to prepend before any application specific commands.
				response = _sd_command(55, 0);
				if (response & ~_sd_R1_IDLE_FLAG)
				{
					debug_unhandled;
				}
			}
			{ // ACMD41 itself. See page 107 of datasheet.
				response = _sd_command(41, ((u32) 1) << 30); // HCS-bit set to allow support for high-capacity cards.
				if (response & ~_sd_R1_IDLE_FLAG)
				{
					debug_unhandled;
				}
			}
			pin_high(PIN_SD_SS);
			_delay_ms(1.0);

			attempts += 1;
		}
		while (response == _sd_R1_IDLE_FLAG && attempts < SD_MAX_ATTEMPTS);

		if (response == _sd_R1_IDLE_FLAG)
		{
			debug_unhandled;
		}
	}

	{ // CMD9 ("SEND_CSD"): Request for card specific data. See page 103 of datasheet.
		pin_low(PIN_SD_SS);
		_delay_ms(1.0);
		{
			u8 response = _sd_command(9, 0);
			if (response)
			{
				debug_unhandled;
			}
		}
		pin_high(PIN_SD_SS);
		_delay_ms(1.0);

		pin_low(PIN_SD_SS);
		_delay_ms(1.0);
		{ // Under "Reading CSD and CID" (Elm-Chan), the CSD is sent as a 16-byte data-block.
			u8 csd[16]; // See page 86 of datasheet for the layout of the CSD register 2.0.
			u8 response = _sd_get_data_block(csd, countof(csd));
			if (response != _sd_DATA_BLOCK_RESPONSE)
			{
				debug_unhandled;
			}

			if
			(
				(csd[0] >> 6) != 1 ||
				(1ULL << (csd[5] & 0xF))                                       != 512 || // Data blocks from reads should be 2^9 = 512 bytes.
				(1ULL << (((csd[12] & ((1 << 2) - 1)) << 2) | (csd[13] >> 6))) != 512    // Data blocks for writes should be 2^9 = 512 bytes.
			)
			{
				debug_unhandled;
			}
		}
		pin_high(PIN_SD_SS);
		_delay_ms(1.0);
	}
}
