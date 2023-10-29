static u8  u8_min (u8  x, u8  y) { return x <= y ? x : y; }
static u16 u16_min(u16 x, u16 y) { return x <= y ? x : y; }
static u32 u32_min(u32 x, u32 y) { return x <= y ? x : y; }
static u64 u64_min(u64 x, u64 y) { return x <= y ? x : y; }
static i8  i8_min (i8  x, i8  y) { return x <= y ? x : y; }
static i16 i16_min(i16 x, i16 y) { return x <= y ? x : y; }
static i32 i32_min(i32 x, i32 y) { return x <= y ? x : y; }
static i64 i64_min(i64 x, i64 y) { return x <= y ? x : y; }

static u8  u8_max (u8  x, u8  y) { return x >= y ? x : y; }
static u16 u16_max(u16 x, u16 y) { return x >= y ? x : y; }
static u32 u32_max(u32 x, u32 y) { return x >= y ? x : y; }
static u64 u64_max(u64 x, u64 y) { return x >= y ? x : y; }
static i8  i8_max (i8  x, i8  y) { return x >= y ? x : y; }
static i16 i16_max(i16 x, i16 y) { return x >= y ? x : y; }
static i32 i32_max(i32 x, i32 y) { return x >= y ? x : y; }
static i64 i64_max(i64 x, i64 y) { return x >= y ? x : y; }

static u8  u8_clamp (u8  value, u8  min, u8  max) { return value <= min ? min : value >= max ? max : value; }
static u16 u16_clamp(u16 value, u16 min, u16 max) { return value <= min ? min : value >= max ? max : value; }
static u32 u32_clamp(u32 value, u32 min, u32 max) { return value <= min ? min : value >= max ? max : value; }
static u64 u64_clamp(u64 value, u64 min, u64 max) { return value <= min ? min : value >= max ? max : value; }
static i8  i8_clamp (i8  value, i8  min, i8  max) { return value <= min ? min : value >= max ? max : value; }
static i16 i16_clamp(i16 value, i16 min, i16 max) { return value <= min ? min : value >= max ? max : value; }
static i32 i32_clamp(i32 value, i32 min, i32 max) { return value <= min ? min : value >= max ? max : value; }
static i64 i64_clamp(i64 value, i64 min, i64 max) { return value <= min ? min : value >= max ? max : value; }

static i8  i8_mod (i8  x, i8  y) { return ((x % y) + y) % y; }
static i16 i16_mod(i16 x, i16 y) { return ((x % y) + y) % y; }
static i32 i32_mod(i32 x, i32 y) { return ((x % y) + y) % y; }
static i64 i64_mod(i64 x, i64 y) { return ((x % y) + y) % y; }

#ifdef f32
	static f32 f32_min(f32 x, f32 y) { return x <= y ? x : y; }
	static f32 f32_max(f32 x, f32 y) { return x >= y ? x : y; }
	static f32 f32_clamp(f32 value, f32 min, f32 max) { return value <= min ? min : value >= max ? max : value; }
	static f32 f32_abs(f32 value) { return value < 0 ? -value : value; }
#endif

#ifdef f64
	static f64 f64_min(f64 x, f64 y) { return x <= y ? x : y; }
	static f64 f64_max(f64 x, f64 y) { return x >= y ? x : y; }
	static f64 f64_clamp(f64 value, f64 min, f64 max) { return value <= min ? min : value >= max ? max : value; }
	static f64 f64_abs(f64 value) { return value < 0 ? -value : value; }
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

		static void
		debug_u16(u16 value)
		{
			pin_output(PIN_U16_CLK);
			pin_output(PIN_U16_DATA);
			pin_low(PIN_U16_CLK);
			pin_low(PIN_U16_DATA);

			for (u8 i = 0; i < sizeof(value) * 8; i += 1)
			{
				if ((value >> i) & 1)
				{
					pin_high(PIN_U16_DATA);
				}
				else
				{
					pin_low(PIN_U16_DATA);
				}

				pin_high(PIN_U16_CLK);
				pin_low(PIN_U16_CLK);
			}
		}

		#define debug_unhandled() debug_unhandled_(__LINE__, PIN_HALT_SOURCE)
		__attribute__((noreturn))
		static void
		debug_unhandled_(u16 line_number, enum HaltSource source)
		{
			cli();
			debug_u16(line_number);
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
		debug_u16(line_number);
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

#if DEBUG && !PROGRAM_MICROSERVIENT && implies(PROGRAM_DIPLOMAT, USB_CDC_ENABLE)
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

#if PROGRAM_MICROSERVIENT
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
		#define assert(...) do { if (!(__VA_ARGS__)) { debug_halt(); } } while (false)
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

#if PROGRAM_MICROSERVIENT
	#define alloc(DST, COUNT) \
		do \
		{ \
			*(DST) = calloc((COUNT),  sizeof(*(DST))); \
			if (!*(DST)) \
			{ \
				error("Failed to allocate."); \
			} \
		} \
		while (false)
#endif
