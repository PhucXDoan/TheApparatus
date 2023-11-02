static str
str_cstr(char* cstr)
{
	str result = { cstr, strlen(cstr) };
	return result;
}

static b32
str_begins_with(str src, str starting)
{
	b32 result = src.length >= starting.length && !memcmp(src.data, starting.data, starting.length);
	return result;
}

static b32
str_ends_with(str src, str ending)
{
	b32 result = src.length >= ending.length && !memcmp(src.data + src.length - ending.length, ending.data, ending.length);
	return result;
}

static b32
str_ends_with_caseless(str src, str ending)
{
	b32 result = true;

	if (src.length >= ending.length)
	{
		for (i64 i = 0; i < ending.length; i += 1)
		{
			if (to_lower(src.data[src.length - ending.length + i]) != to_lower(ending.data[i]))
			{
				result = false;
				break;
			}
		}
	}
	else
	{
		result = false;
	}

	return result;
}

static b32
str_eq(str lhs, str rhs)
{
	b32 result = lhs.length == rhs.length && !memcmp(lhs.data, rhs.data, lhs.length);
	return result;
}

#define error_capacity() error("`%s` reached maximum capacity.", __func__)

static str
strbuf_char(struct StrBuf* buf, char value)
{
	str result = {0};

	if (buf->length + 1 <= buf->size)
	{
		buf->data[buf->length]  = value;
		buf->length            += 1;
		result                  =
			(str)
			{
				.data   = buf->data + buf->length - 1,
				.length = 1
			};
	}
	else
	{
		error_capacity();
	}

	return result;
}

static str
strbuf_char_n(struct StrBuf* buf, char value, i64 count)
{
	i64 capped_count = count >= 0 ? count : 0;
	str result       =
		{
			.data   = buf->data + buf->length,
			.length = buf->size - buf->length
		};
	if (result.length > capped_count)
	{
		error_capacity();
	}

	memset(result.data, value, result.length);
	buf->length += result.length;

	return result;
}

#define strbuf_strbuf(STRBUF_PTR, STRBUF) strbuf_str((STRBUF_PTR), (STRBUF).str)
static str
strbuf_str(struct StrBuf* buf, str value)
{
	if (value.length > buf->size - buf->length)
	{
		error_capacity();
	}

	str result      =
		{
			.data   = buf->data + buf->length,
			.length = value.length
		};

	memmove(result.data, value.data, result.length);
	buf->length += result.length;

	return result;
}

static str
strbuf_cstr(struct StrBuf* buf, char* value)
{
	str string = { .data = value, .length = strlen(value) };
	str result = strbuf_str(buf, string);

	if (result.length != string.length)
	{
		error_capacity();
	}

	return result;
}

static str
strbuf_u64(struct StrBuf* buf, u64 value) // Remaining space of at least 20 will handle all values.
{
	char serialized_buffer[20] = {0};
	i32  serialized_length     = serialize_u64(serialized_buffer, sizeof(serialized_buffer), value);
	str  result                = strbuf_str(buf, (str) { .data = serialized_buffer, .length = serialized_length });
	return result;
}

static str
strbuf_i64(struct StrBuf* buf, i64 value) // Remaining space of at least 20 will handle all values.
{
	char serialized_buffer[20] = {0};
	i32  serialized_length     = serialize_i64(serialized_buffer, sizeof(serialized_buffer), value);
	str  result                = strbuf_str(buf, (str) { .data = serialized_buffer, .length = serialized_length });
	return result;
}

static str
strbuf_8b(struct StrBuf* buf, u8 value)
{
	char digits[8] = {0};
	for (i32 i = 0; i < countof(digits); i += 1)
	{
		digits[i] = '0' + ((value >> (7 - i)) & 1);
	}
	str result = strbuf_str(buf, (str) { digits, countof(digits) });
	return result;
}

static str
strbuf_8H(struct StrBuf* buf, u8 value)
{
	char digits[] =
		{
			((value >> 4) & 0x0F) < 10 ? '0' + ((value >> 4) & 0x0F) : 'A' + ((value >> 4) & 0x0F) - 10,
			((value >> 0) & 0x0F) < 10 ? '0' + ((value >> 0) & 0x0F) : 'A' + ((value >> 0) & 0x0F) - 10,
		};
	str result = strbuf_str(buf, (str) { digits, countof(digits) });
	return result;
}

static str
strbuf_16H(struct StrBuf* buf, u16 value)
{
	str result = {0};
	result         = strbuf_8H(buf, (value >> 8) & 0xFF);
	result.length += strbuf_8H(buf, (value >> 0) & 0xFF).length;
	return result;
}

static str
strbuf_32H(struct StrBuf* buf, u32 value)
{
	str result = {0};
	result         = strbuf_16H(buf, (value >> 16) & 0xFFFF);
	result.length += strbuf_16H(buf, (value >>  0) & 0xFFFF).length;
	return result;
}

static str
strbuf_64H(struct StrBuf* buf, u64 value)
{
	str result = {0};
	result         = strbuf_32H(buf, (value >> 32) & 0xFFFF'FFFF);
	result.length += strbuf_32H(buf, (value >>  0) & 0xFFFF'FFFF).length;
	return result;
}

static str
strbuf_pad_right_char(struct StrBuf* buf, i64 min_length, char padding)
{
	str result = {0};

	if (min_length > buf->size)
	{
		error_capacity();
	}
	else if (min_length > buf->length)
	{
		result.data   = buf->data + buf->length;
		result.length = min_length - buf->length;
		memset(result.data, padding, result.length);
		buf->length += result.length;
	}

	return result;
}

#undef error_capacity
