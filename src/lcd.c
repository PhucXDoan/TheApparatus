static void
_lcd_pulse_enable(void)
{
	pin_high(PIN_LCD_ENABLE);
	_delay_us(25.0);
	pin_low (PIN_LCD_ENABLE);
	_delay_us(25.0);
}

static void
_lcd_raw_u8(u8 value) // See: 4-Bit Data Transfering @ Source(25) @ Figure(9) @ Page(22).
{
	//
	// High nibble.
	//

	if ((value >> 4) & 1) { pin_high(PIN_LCD_DATA_4); } else { pin_low(PIN_LCD_DATA_4); }
	if ((value >> 5) & 1) { pin_high(PIN_LCD_DATA_5); } else { pin_low(PIN_LCD_DATA_5); }
	if ((value >> 6) & 1) { pin_high(PIN_LCD_DATA_6); } else { pin_low(PIN_LCD_DATA_6); }
	if ((value >> 7) & 1) { pin_high(PIN_LCD_DATA_7); } else { pin_low(PIN_LCD_DATA_7); }

	_lcd_pulse_enable();

	//
	// Low nibble.
	//

	if ((value >> 0) & 1) { pin_high(PIN_LCD_DATA_4); } else { pin_low(PIN_LCD_DATA_4); }
	if ((value >> 1) & 1) { pin_high(PIN_LCD_DATA_5); } else { pin_low(PIN_LCD_DATA_5); }
	if ((value >> 2) & 1) { pin_high(PIN_LCD_DATA_6); } else { pin_low(PIN_LCD_DATA_6); }
	if ((value >> 3) & 1) { pin_high(PIN_LCD_DATA_7); } else { pin_low(PIN_LCD_DATA_7); }

	_lcd_pulse_enable();
}

static void
lcd_init(void)
{ // See: Initalization via 4-Bit Interface @ Source(25) @ Figure(24) @ Page(46).

	//
	// Set up and wait for power.
	//

	pin_output(PIN_LCD_ENABLE);
	pin_output(PIN_LCD_REGISTER_SELECT); // When low, data is command, else data is for display. Note that we always have the read/write pin (R/W) grounded.
	pin_output(PIN_LCD_DATA_4);
	pin_output(PIN_LCD_DATA_5);
	pin_output(PIN_LCD_DATA_6);
	pin_output(PIN_LCD_DATA_7);

	_delay_ms(15.0);

	//
	// Function-Set command that the datasheet says that we must send multiple times. See: "Function set" @ Source(25) @ Table(6) @ Page(24).
	//

	pin_high(PIN_LCD_DATA_4); // Data-length bit ("DL") active to indicate 8-bit interface.
	pin_high(PIN_LCD_DATA_5);

	_lcd_pulse_enable();
	_delay_ms(4.1);

	_lcd_pulse_enable();
	_delay_us(100);

	_lcd_pulse_enable();

	//
	// Function-Set command that actually sets the interface to 4-bits.
	//

	pin_low(PIN_LCD_DATA_4); // Data-length bit now low.
	_lcd_pulse_enable();

	//
	// Configuring.
	//

	_lcd_raw_u8(0b0010'1000); // Function-Set command for 2-line display. This still applies even for 4x20 displays.
	_lcd_raw_u8(0b0000'1100); // Display-Control command to activate display. See: "Display on/off control" @ Source(25) @ Table(6) @ Page(24).
	_lcd_raw_u8(0b0000'0001); // Clear-Display command. See: "Clear display" @ Source(25) @ Table(6) @ Page(24).
	_delay_ms(2.0);           // Clearing display apparently takes a while to fully take into effect.
	_lcd_raw_u8(0b0000'0110); // Entry-Mode-Set command that automatically increments the write cursor. See: "Entry mode set" @ Source(25) @ Table(6) @ Page(24).
}

static void
lcd_cursor(b8 visible)
{
	pin_low(PIN_LCD_REGISTER_SELECT);
	_lcd_raw_u8(0b0000'1100 | (!!visible << 1)); // Entry-Mode-Set command.
}

static void
lcd_reset(void)
{
	memset(lcd_display, 0, sizeof(lcd_display));
	lcd_cursor_pos = (u8_2) {0};
	lcd_cursor(false);
}

static void
lcd_refresh(void)
{
	for (u8 y = 0; y < LCD_DIM_Y; y += 1)
	{
		for (u8 x = 0; x < LCD_DIM_X; x += 1)
		{
			if (_lcd_cache[y][x] != lcd_display[y][x])
			{
				_lcd_cache[y][x] = lcd_display[y][x];

				static_assert(LCD_DIM_X == 20 && LCD_DIM_Y == 4); // The math done here assumes we have 4x20 display.
				pin_low(PIN_LCD_REGISTER_SELECT);
				_lcd_raw_u8(0b1000'0000 | (x + (y % 2) * 64 + (y / 2) * 20)); // Set-DDRAM-Address command. See: "Set DDRAM Address" @ Source(25) @ Page(29).

				pin_high(PIN_LCD_REGISTER_SELECT);
				_lcd_raw_u8(lcd_display[y][x] ? lcd_display[y][x] : ' '); // Writing null character makes a funny symbol; space is more preferable.
			}
		}
	}
}

static void
lcd_char(char value)
{
	if (lcd_cursor_pos.y < LCD_DIM_Y)
	{
		if (value == '\r')
		{
			// Ignore.
		}
		else if (value == '\n')
		{
			lcd_cursor_pos.x  = 0;
			lcd_cursor_pos.y += 1;
		}
		else if (lcd_cursor_pos.x < LCD_DIM_X)
		{
			lcd_display[lcd_cursor_pos.y][lcd_cursor_pos.x] = value;
			lcd_cursor_pos.x += 1;
		}
	}
}

static void
lcd_cstr(char* value)
{
	for (u16 i = 0; value[i]; i += 1)
	{
		lcd_char(value[i]);
	}
}

static void
lcd_u64(u64 value)
{
	char buffer[21] = {0};
	u8   length = serialize_u64(buffer, countof(buffer), value);
	lcd_cstr(buffer);
}

static void
lcd_i64(u64 value)
{
	char buffer[21] = {0};
	u8   length = serialize_i64(buffer, countof(buffer), value);
	lcd_cstr(buffer);
}

static void
lcd_8H(u8 value)
{
	lcd_char((value >>   4) < 10 ? '0' + (value >>   4) : 'A' + ((value >>   4) - 10));
	lcd_char((value &  0xF) < 10 ? '0' + (value &  0xF) : 'A' + ((value &  0xF) - 10));
}
