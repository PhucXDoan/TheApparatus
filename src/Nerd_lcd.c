// TODO Old code. Revise.

/*
	Consult "HD44780U (LCD-II) (Dot Matrix Liquid Crystal Display Controller/Driver)" by HITACHI.
	Note that the driver is assuming 1-line/2-line display, but 4-line displays (specifically the 20x4)
	also use the same driver. Display memory that's mapped outside what's visible on the 1-line/2-line
	display (used for scrolling text) is made visible on the 4-line display.
*/

static char _lcd_cache_buffer[LCD_DIMS_Y][LCD_DIMS_X] = {0};

static void
_lcd_raw_u8(u8 value)
{
	// See page 22 in the HITACHI datasheet on data transfer in 4-bit mode.

	// High-nibble is transferred first.
	debug_pin_set(LCD_D4_PIN, (value >> 4) & 1);
	debug_pin_set(LCD_D5_PIN, (value >> 5) & 1);
	debug_pin_set(LCD_D6_PIN, (value >> 6) & 1);
	debug_pin_set(LCD_D7_PIN, (value >> 7) & 1);
	debug_pin_set(LCD_ENABLING_PIN, true);
	_delay_ms(0.01);
	debug_pin_set(LCD_ENABLING_PIN, false);
	_delay_ms(0.01);

	// Low-nibble is transferred second.
	debug_pin_set(LCD_D4_PIN, (value >> 0) & 1);
	debug_pin_set(LCD_D5_PIN, (value >> 1) & 1);
	debug_pin_set(LCD_D6_PIN, (value >> 2) & 1);
	debug_pin_set(LCD_D7_PIN, (value >> 3) & 1);
	debug_pin_set(LCD_ENABLING_PIN, true);
	_delay_ms(0.01);
	debug_pin_set(LCD_ENABLING_PIN, false);
	_delay_ms(0.01);
}

static void
lcd_init(void)
{
	_delay_ms(40.0); // Wait at least 40ms to ensure power as stated in page 46.

	debug_pin_set(LCD_REGISTER_SELECT_PIN, false); // When low, it's essentially saying that data sent is an instruction. When high, it's about the display RAM. See table 1 on page 9.
	debug_pin_set(LCD_D4_PIN, true);               // The DL-bit of the function-set instruction. When high, this sets the data-length to 8-bits. This will be changed to 4-bits later.
	debug_pin_set(LCD_D5_PIN, true);               // The bit for a function-set instruction. Consult page 27 and 28.
	debug_pin_set(LCD_D6_PIN, false);
	debug_pin_set(LCD_D7_PIN, false);
	debug_pin_set(LCD_ENABLING_PIN, false);
	_delay_ms(0.1);

	debug_pin_set(LCD_ENABLING_PIN, true); // Pulse out the instruction.
	_delay_ms(0.1);
	debug_pin_set(LCD_ENABLING_PIN, false);
	_delay_ms(5.0);

	debug_pin_set(LCD_ENABLING_PIN, true); // The documentation on the initialization process states that we repeat the Function-Set instruction again.
	_delay_ms(0.1);
	debug_pin_set(LCD_ENABLING_PIN, false);
	_delay_ms(1.0);

	debug_pin_set(LCD_ENABLING_PIN, true); // ... and once more!
	_delay_ms(0.1);
	debug_pin_set(LCD_ENABLING_PIN, false);
	_delay_ms(0.1);

	debug_pin_set(LCD_D4_PIN, false); // We set the function-set instruction's DL-bit (as in data-length) low to indicate 4-bit mode.
	_delay_ms(0.1);
	debug_pin_set(LCD_ENABLING_PIN, true);
	_delay_ms(0.1);
	debug_pin_set(LCD_ENABLING_PIN, false);
	_delay_ms(0.1);

	_lcd_raw_u8((1 << 5) | (1 << 3)); // Function-Set instruction for 2-line display. Consult page 27 and 28.

	_lcd_raw_u8((1 << 3) | (1 << 2)); // Display-tontrol instruction with D-bit on to activate the LCD screen. See page 23 on item #3 and page 24.

	_lcd_raw_u8(1 << 0);  // Clear-Display instruction.
	_delay_ms(2.0);       // Clearing display apparently takes a while to fully take into effect.

	_lcd_raw_u8((1 << 2) | (1 << 1)); // Entry-Mode-Set instruction with the I/D-bit set to increment the cursor after writing a character. See page 26 and 28.

	_delay_ms(40.0); // TODO Delay?
}

static void
lcd_cursor(b8 visible)
{
	debug_pin_set(LCD_REGISTER_SELECT_PIN, false);
	_lcd_raw_u8((1 << 3) | (1 << 2) | (!!visible << 1)); // Entry-mode-set instruction. See page 26 and 28.
}

static void
lcd_renew(void)
{
	memset(lcd_backbuffer, 0, sizeof(lcd_backbuffer));
	lcd_cursor_pos_x = 0;
	lcd_cursor_pos_y = 0;
	lcd_cursor(false);
}

static void
lcd_swap(void)
{
	for (u8 y = 0; y < LCD_DIMS_Y; y += 1)
	{
		for (u8 x = 0; x < LCD_DIMS_X; x += 1)
		{
			if (_lcd_cache_buffer[y][x] != lcd_backbuffer[y][x])
			{
				_lcd_cache_buffer[y][x] = lcd_backbuffer[y][x];
				debug_pin_set(LCD_REGISTER_SELECT_PIN, false);
				_lcd_raw_u8((1 << 7) | (x + (y % 2) * 64 + (y / 2) * 20)); // Set-DDRAM-address instruction. Consult page 24 and 29. Note that this is for a 20x4 display.
				debug_pin_set(LCD_REGISTER_SELECT_PIN, true);
				_lcd_raw_u8(lcd_backbuffer[y][x] ? lcd_backbuffer[y][x] : ' ');
			}
		}
	}
}

static void
lcd_char(char value)
{
	if (lcd_cursor_pos_y < LCD_DIMS_Y)
	{
		if (value == '\r')
		{
			// Ignore.
		}
		else if (value == '\n')
		{
			lcd_cursor_pos_x  = 0;
			lcd_cursor_pos_y += 1;
		}
		else if (lcd_cursor_pos_x < LCD_DIMS_X)
		{
			lcd_backbuffer[lcd_cursor_pos_y][lcd_cursor_pos_x] = value;
			lcd_cursor_pos_x += 1;
		}
	}
}

static void
lcd_chars(char* value, u16 length)
{
	for (u16 i = 0; i < length && lcd_cursor_pos_y < LCD_DIMS_Y; i += 1)
	{
		lcd_char(value[i]);
	}
}

static void
lcd_cstr(char* value)
{
	lcd_chars(value, strlen(value));
}

static void
lcd_u64(u64 value)
{
	if (lcd_cursor_pos_x < LCD_DIMS_X && lcd_cursor_pos_y < LCD_DIMS_Y)
	{
		lcd_cursor_pos_x +=
			serialize_u64
			(
				&lcd_backbuffer[lcd_cursor_pos_y][lcd_cursor_pos_x],
				LCD_DIMS_X - lcd_cursor_pos_x,
				value
			);
	}
}

static void
lcd_i64(u64 value)
{
	if (lcd_cursor_pos_x < LCD_DIMS_X && lcd_cursor_pos_y < LCD_DIMS_Y)
	{
		lcd_cursor_pos_x +=
			serialize_i64
			(
				&lcd_backbuffer[lcd_cursor_pos_y][lcd_cursor_pos_x],
				LCD_DIMS_X - lcd_cursor_pos_x,
				value
			);
	}
}

static void
lcd_h8(u8 value)
{
	lcd_char((value >>   4) < 10 ? '0' + (value >>   4) : 'A' + ((value >>   4) - 10));
	lcd_char((value &  0xF) < 10 ? '0' + (value &  0xF) : 'A' + ((value &  0xF) - 10));
}
