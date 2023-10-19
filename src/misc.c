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
		result = dst_size;
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
			result = 1 + serialize_u64(dst + 1, dst_size - 1, -((u64) value));
		}
	}
	else
	{
		result = serialize_u64(dst, dst_size, (u64) value);
	}

	return result;
}

#if DEBUG
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
		if (debug_usb_diagnostic_signal_received)
		{
			char buffer[20];
			u8   length = serialize_u64(buffer, countof(buffer), value);
			debug_tx_chars(buffer, length);
		}
	}

	static void
	debug_tx_i64(i64 value)
	{
		if (debug_usb_diagnostic_signal_received)
		{
			char buffer[20];
			u8   length = serialize_i64(buffer, countof(buffer), value);
			debug_tx_chars(buffer, length);
		}
	}

	static void
	debug_tx_cstr(char* value)
	{
		debug_tx_chars(value, strlen(value));
	}
#endif
