static u8 // Amount written; will never exceed 20.
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

	u8 length = countof(aux_buffer) - aux_writer;
	if (length > dst_size)
	{
		length = (u8) dst_size;
	}

	memcpy(dst, aux_buffer + aux_writer, length);

	return length;
}

static u8 // Amount written; will never exceed 20.
serialize_i64(char* dst, u16 dst_size, i64 value) // "dst_size" of at least 20 will handle all values.
{
	u8 length = 0;

	if (value < 0)
	{
		if (dst_size)
		{
			dst[0] = '-';
			length = 1 + serialize_u64(dst + 1, dst_size - 1, ~((u64) value) + 1);
		}
	}
	else
	{
		length = serialize_u64(dst, dst_size, (u64) value);
	}

	return length;
}

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
			LANGUAGE, \
			MAX_WORD_LENGTH, \
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

#ifdef PROGMEM
	#define pgm_char(LVALUE)                     pgm_read_byte ((const char       *) { &(LVALUE) })
	#define pgm_u8(LVALUE)                       pgm_read_byte ((const u8         *) { &(LVALUE) })
	#define pgm_u16(LVALUE)                      pgm_read_word ((const u16        *) { &(LVALUE) })
	#define pgm_u32(LVALUE)                      pgm_read_dword((const u32        *) { &(LVALUE) })
	#define pgm_u8_ptr(LVALUE)    ((const u8  *) pgm_read_ptr  ((const u8  * const*) { &(LVALUE) }))
	#define pgm_u16_ptr(LVALUE)   ((const u16 *) pgm_read_ptr  ((const u16 * const*) { &(LVALUE) }))
	#define pgm_u32_ptr(LVALUE)   ((const u32 *) pgm_read_ptr  ((const u32 * const*) { &(LVALUE) }))
	#define pgm_u64_ptr(LVALUE)   ((const u64 *) pgm_read_ptr  ((const u64 * const*) { &(LVALUE) }))
	#define pgm_ptr(TYPE, LVALUE) ((const TYPE*) pgm_read_ptr  ((const TYPE* const*) { &(LVALUE) }))
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

#ifdef WDTO_15MS
	__attribute__((noreturn))
	static void
	restart(void)
	{
		wdt_enable(WDTO_15MS);
		for(;;);
	}
#endif

#if PROGRAM_DIPLOMAT || PROGRAM_NERD
	#if DEBUG
		static void
		debug_dump(char* file_name, u16 line_number);

		__attribute__((noreturn))
		static void
		debug_unhandled(char* file_name, u16 line_number)
		{
			cli();
			debug_dump(file_name, line_number);

			pin_output(HALT);
			for (;;)
			{
				pin_high(HALT);
				_delay_ms(500.0);
				pin_low(HALT);
				_delay_ms(500.0);
			}
		}
		#define debug_unhandled() debug_unhandled(__FILE__, __LINE__)

		static void
		debug_chars(char* data, u16 length);

		static u8 // Amount received and copied into dst.
		debug_rx(char* dst, u16 dst_size);

		static void
		debug_char(char value)
		{
			debug_chars(&value, 1);
		}

		static void
		debug_cstr(char* value)
		{
			debug_chars(value, strlen(value));
		}

		static void
		debug_8b(u8 value)
		{
			for
			(
				u8 i = bitsof(value);
				i--;
			)
			{
				debug_char('0' + ((value >> i) & 1));
			}
		}

		static void
		debug_16b(u16 value)
		{
			debug_8b((value >> 8) & 0xFF);
			debug_char('\'');
			debug_8b((value >> 0) & 0xFF);
		}

		static void
		debug_32b(u32 value)
		{
			debug_16b((value >> 16) & 0xFFFF);
			debug_char('\'');
			debug_16b((value >>  0) & 0xFFFF);
		}

		static void
		debug_64b(u64 value)
		{
			debug_32b((value >> 32) & 0xFFFF'FFFF);
			debug_char('\'');
			debug_32b((value >>  0) & 0xFFFF'FFFF);
		}

		static void
		debug_8H(u8 value)
		{
			debug_char(((value >> 4) & 0x0F) < 10 ? '0' + ((value >> 4) & 0x0F) : 'A' + ((value >> 4) & 0x0F) - 10);
			debug_char(((value >> 0) & 0x0F) < 10 ? '0' + ((value >> 0) & 0x0F) : 'A' + ((value >> 0) & 0x0F) - 10);
		}

		static void
		debug_16H(u16 value)
		{
			debug_8H((value >> 8) & 0xFF);
			debug_8H((value >> 0) & 0xFF);
		}

		static void
		debug_32H(u32 value)
		{
			debug_16H((value >> 16) & 0xFFFF);
			debug_char('\'');
			debug_16H((value >>  0) & 0xFFFF);
		}

		static void
		debug_64H(u64 value)
		{
			debug_32H((value >> 32) & 0xFFFF'FFFF);
			debug_char('\'');
			debug_32H((value >>  0) & 0xFFFF'FFFF);
		}

		static void
		debug_u64(u64 value)
		{
			char buffer[20];
			u8   length = serialize_u64(buffer, countof(buffer), value);
			debug_chars(buffer, length);
		}

		static void
		debug_i64(i64 value)
		{
			char buffer[20];
			u8   length = serialize_i64(buffer, countof(buffer), value);
			debug_chars(buffer, length);
		}
	#endif

	__attribute__((noreturn))
	static void
	error(char* file_name, u16 line_number)
	{
		cli();

		#if DEBUG
			debug_dump(file_name, line_number);
		#endif

		pin_output(HALT);
		for (;;)
		{
			pin_high(HALT);
			_delay_ms(25.0);
			pin_low(HALT);
			_delay_ms(25.0);
		}
	}
	#define error() error(__FILE__, __LINE__)

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

	#if DEBUG
		#define debug_halt() __debugbreak()
		#define assert(...) do { if (!(__VA_ARGS__)) { debug_halt(); exit(1); } } while (false)
	#else
		#define assert(...)
	#endif
#endif
