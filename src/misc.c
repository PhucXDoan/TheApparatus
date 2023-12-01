#ifdef PROGMEM
	#define pgm_u8(LVALUE)  pgm_read_byte((const u8 *) { &(LVALUE) })
	#define pgm_u16(LVALUE) pgm_read_word((const u16*) { &(LVALUE) })
#endif

#if PROGRAM_MICROSERVICES
	static u16
	_crc16_update(u16 crc, u8 byte) // From avr-gcc's utils.
	{
		crc ^= byte;

		for (u8 i = 0; i < 8; i += 1)
		{
			if (crc & 1)
			{
				crc >>= 1;
				crc  ^= 0b1010'0000'0000'0001;
			}
			else
			{
				crc >>= 1;
			}
		}

		return crc;
	}
#endif

static u8 // Amount written; will never exceed 255 since all serialized u64s will be 20 bytes or shorter.
serialize_u64(char* dst, u16 dst_size, u64 value) // "dst_size" of at least 20 will handle all values.
{
	char aux_buffer[20];
	u8   aux_writer = countof(aux_buffer);
	u64  current    = value;
	do
	{
		aux_writer             -= 1;
		aux_buffer[aux_writer]  = '0' + (current % 10);
		current                /= 10;
	}
	while (current && aux_writer);

	u8 result = countof(aux_buffer) - aux_writer;
	if (result > dst_size)
	{
		result = u8(dst_size); // Safe to cast since "result" is bigger than "dst_size".
	}

	memcpy(dst, aux_buffer + aux_writer, result);

	return result;
}

static u8 // Amount written; will never exceed 255 since all serialized i64s will be 20 bytes or shorter.
serialize_i64(char* dst, u16 dst_size, i64 value) // "dst_size" of at least 20 will handle all values.
{
	u8 result = 0;

	if (value < 0)
	{
		if (dst_size)
		{
			dst[0] = '-';
			result = 1 + serialize_u64(dst + 1, dst_size - 1, ~u64(value) + u64(1));
		}
	}
	else
	{
		result = serialize_u64(dst, dst_size, u64(value));
	}

	return result;
}

#if !PROGRAM_MICROSERVICES
	__attribute__((noreturn))
	static void
	restart(void)
	{
		wdt_enable(WDTO_15MS);
		for(;;);
	}
#endif

#if PROGRAM_DIPLOMAT
	#if DEBUG
		// Long pulse followed by pairs of flashes.
		__attribute__((noreturn))
		static void
		debug_halt(u8 amount)
		{
			cli();

			pin_output(HALT);
			for (;;)
			{
				pin_high(HALT);
				_delay_ms(2000.0);
				pin_low(HALT);
				_delay_ms(1000.0);

				for (u8 i = 0; i < amount; i += 1)
				{
					pin_high(HALT);
					_delay_ms(25.0);
					pin_low(HALT);
					_delay_ms(25.0);
					pin_high(HALT);
					_delay_ms(25.0);
					pin_low(HALT);
					_delay_ms(1000.0);
				}
			}
		}

		#ifdef PIN_DEBUG_U16_CLK
		static void
		debug_u16(u16 value)
		{
			pin_output(PIN_DEBUG_U16_CLK);
			pin_output(PIN_DEBUG_U16_DATA);
			pin_low(PIN_DEBUG_U16_CLK);
			pin_low(PIN_DEBUG_U16_DATA);

			for (u8 i = 0; i < sizeof(value) * 8; i += 1)
			{
				if ((value >> i) & 1)
				{
					pin_high(PIN_DEBUG_U16_DATA);
				}
				else
				{
					pin_low(PIN_DEBUG_U16_DATA);
				}

				pin_high(PIN_DEBUG_U16_CLK);
				pin_low(PIN_DEBUG_U16_CLK);
			}
		}
		#endif

		#define debug_unhandled() debug_unhandled_(__LINE__, PIN_HALT_SOURCE)
		__attribute__((noreturn))
		static void
		debug_unhandled_(u16 line_number, enum HaltSource source)
		{
			cli();
			#ifdef PIN_DEBUG_U16_CLK
			debug_u16(line_number);
			#endif
			debug_halt(source);
		}

		#define assert(...) do { if (!(__VA_ARGS__)) { debug_unhandled(); } } while (false)
	#else
		#define assert(...)
	#endif

	// Rapid flashes follwed by pulses indicating the source of error.
	#define error() error_(__LINE__, PIN_HALT_SOURCE)
	__attribute__((noreturn))
	static void
	error_(u16 line_number, enum HaltSource source)
	{
		cli();

		#if DEBUG
		#ifdef PIN_DEBUG_U16_CLK
		debug_u16(line_number);
		#endif
		#endif

		pin_output(HALT);
		for (;;)
		{
			#if BOARD_PRO_MICRO || BOARD_LEONARDO // These boards has the LEDs active low.
				#define ON()  pin_low(HALT)
				#define OFF() pin_high(HALT)
			#endif

			#if BOARD_MEGA2560
				#define ON()  pin_high(HALT)
				#define OFF() pin_low(HALT)
			#endif

			for (u8 i = 0; i < 8; i += 1)
			{
				ON();
				_delay_ms(25.0);
				OFF();
				_delay_ms(25.0);
			}

			_delay_ms(250.0);

			for (u8 i = 0; i < source; i += 1)
			{
				ON();
				_delay_ms(150.0);
				OFF();
				_delay_ms(150.0);
			}

			_delay_ms(250.0);

			#undef ON
			#undef OFF
		}
	}
#endif

#if DEBUG && !PROGRAM_MICROSERVICES && implies(PROGRAM_DIPLOMAT, USB_CDC_ENABLE)
	static void
	debug_tx_chars(char* value, u16 value_size);

	static void
	debug_tx_8b(u8 value)
	{
		char digits[] =
			{
				'0' + ((value >> 0) & 1),
				'0' + ((value >> 1) & 1),
				'0' + ((value >> 2) & 1),
				'0' + ((value >> 3) & 1),
				'0' + ((value >> 4) & 1),
				'0' + ((value >> 5) & 1),
				'0' + ((value >> 6) & 1),
				'0' + ((value >> 7) & 1),
			};
		debug_tx_chars(digits, countof(digits));
	}

	static void
	debug_tx_16b(u16 value)
	{
		debug_tx_8b((value >> 8) & 0xFF);
		debug_tx_chars(&(char) { '\'' }, 1);
		debug_tx_8b((value >> 0) & 0xFF);
	}

	static void
	debug_tx_32b(u32 value)
	{
		debug_tx_16b((value >> 16) & 0xFFFF);
		debug_tx_chars(&(char) { '\'' }, 1);
		debug_tx_16b((value >>  0) & 0xFFFF);
	}

	static void
	debug_tx_64b(u64 value)
	{
		debug_tx_32b((value >> 32) & 0xFFFF'FFFF);
		debug_tx_chars(&(char) { '\'' }, 1);
		debug_tx_32b((value >>  0) & 0xFFFF'FFFF);
	}

	static void
	debug_tx_8H(u8 value)
	{
		char digits[] =
			{
				((value >> 4) & 0x0F) < 10 ? '0' + ((value >> 4) & 0x0F) : 'A' + ((value >> 4) & 0x0F) - 10,
				((value >> 0) & 0x0F) < 10 ? '0' + ((value >> 0) & 0x0F) : 'A' + ((value >> 0) & 0x0F) - 10,
			};
		debug_tx_chars(digits, countof(digits));
	}

	static void
	debug_tx_16H(u16 value)
	{
		debug_tx_8H((value >> 8) & 0xFF);
		debug_tx_8H((value >> 0) & 0xFF);
	}

	static void
	debug_tx_32H(u32 value)
	{
		debug_tx_16H((value >> 16) & 0xFFFF);
		debug_tx_chars(&(char) { '\'' }, 1);
		debug_tx_16H((value >>  0) & 0xFFFF);
	}

	static void
	debug_tx_64H(u64 value)
	{
		debug_tx_32H((value >> 32) & 0xFFFF'FFFF);
		debug_tx_chars(&(char) { '\'' }, 1);
		debug_tx_32H((value >>  0) & 0xFFFF'FFFF);
	}

	static void
	debug_tx_u64(u64 value)
	{
		char buffer[20];
		u8   length = serialize_u64(buffer, countof(buffer), value);
		debug_tx_chars(buffer, length);
	}

	static void
	debug_tx_i64(i64 value)
	{
		char buffer[20];
		u8   length = serialize_i64(buffer, countof(buffer), value);
		debug_tx_chars(buffer, length);
	}

	static void
	debug_tx_cstr(char* value)
	{
		debug_tx_chars(value, strlen(value));
	}
#endif

#if PROGRAM_MICROSERVICES
	#define error(...) \
		do \
		{ \
			fprintf(stderr, __FILE__ ":%d:%s: ", __LINE__, __func__); \
			fprintf(stderr, __VA_ARGS__); \
			fprintf(stderr, "\n"); \
			__debugbreak(); \
			exit(1); \
		} \
		while (false)

	#if DEBUG
		#define debug_halt() __debugbreak()
		#define assert(...) do { if (!(__VA_ARGS__)) { debug_halt(); exit(1); } } while (false)
	#else
		#define assert(...)
	#endif
#endif

static char
to_lower(char c)
{
	char result = c;
	if ('A' <= result && result <= 'Z')
	{
		result -= 'A';
		result += 'a';
	}
	return result;
}

static char
to_upper(char c)
{
	char result = c;
	if ('a' <= result && result <= 'z')
	{
		result -= 'a';
		result += 'A';
	}
	return result;
}

#if PROGRAM_MICROSERVICES
	#define alloc(DST_PTR, COUNT) \
		do \
		{ \
			i64 _alloc_COUNT = (COUNT); \
			if (_alloc_COUNT) \
			{ \
				*(DST_PTR) = calloc(_alloc_COUNT, sizeof(**(DST_PTR))); \
				if (!*(DST_PTR)) \
				{ \
					error("Failed to allocate."); \
				} \
			} \
		} \
		while (false)
#endif

static b8
is_slot_excluded(enum WordGame wordgame, u8 x, u8 y)
{
	b8 result = false;

	switch (wordgame)
	{
		#define MAKE_TEST(EXCLUDED_SLOT_COORD_X, EXCLUDED_SLOT_COORD_Y) || (x == (EXCLUDED_SLOT_COORD_X) && y == (EXCLUDED_SLOT_COORD_Y))
		#define MAKE_CASE( \
			IDENTIFIER_NAME, \
			PRINT_NAME, \
			SENTINEL_LETTER, \
			POS_X, POS_Y, \
			DIM_SLOTS_X, DIM_SLOTS_Y, \
			SLOT_DIM, \
			UNCOMPRESSED_SLOT_STRIDE, \
			COMPRESSED_SLOT_STRIDE, \
			TEST_REGION_POS_X, TEST_REGION_POS_Y, \
			TEST_REGION_DIM_X, TEST_REGION_DIM_Y, \
			TEST_REGION_R, TEST_REGION_G, TEST_REGION_B, \
			... \
		) \
			case WordGame_##IDENTIFIER_NAME: \
			{ \
				result = false __VA_ARGS__; \
			} break;
		WORDGAME_XMDT(MAKE_CASE, MAKE_TEST)
		#undef MAKE_CASE
		#undef MAKE_TEST

		case WordGame_COUNT: break;
	}

	return result;
}
