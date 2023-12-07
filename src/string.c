// Basic operations between null-terminated strings (cstrs) and lengthed strings.

static str
str_cstr(char* cstr)
{
	str result = { cstr, strlen(cstr) };
	return result;
}

static b32
str_begins_with(str src, str starting)
{
	b32 result = src.length >= starting.length && memeq(src.data, starting.data, starting.length);
	return result;
}

static b32
str_ends_with(str src, str ending)
{
	b32 result = src.length >= ending.length && memeq(src.data + src.length - ending.length, ending.data, ending.length);
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
	b32 result = lhs.length == rhs.length && memeq(lhs.data, rhs.data, lhs.length);
	return result;
}

#define error_capacity() error("`%s` reached maximum capacity.", __func__)

static void
strbuf_char(strbuf* buf, char value)
{
	if (buf->length + 1 <= buf->size)
	{
		buf->data[buf->length]  = value;
		buf->length            += 1;
	}
	else
	{
		error_capacity();
	}
}

#define strbuf_strbuf(STRBUF_PTR, STRBUF) strbuf_str((STRBUF_PTR), (STRBUF).str)
static void
strbuf_str(strbuf* buf, str value)
{
	if (buf->length + value.length <= buf->size)
	{
		memmove(buf->data + buf->length, value.data, value.length);
		buf->length += value.length;
	}
	else
	{
		error_capacity();
	}
}

static void
strbuf_cstr(strbuf* buf, char* value)
{
	strbuf_str(buf, (str) { value, strlen(value) });
}

static void
strbuf_u64(strbuf* buf, u64 value) // Remaining space of at least 20 will handle all values.
{
	char serialized_buffer[20] = {0};
	i32  serialized_length     = serialize_u64(serialized_buffer, sizeof(serialized_buffer), value);
	strbuf_str(buf, (str) { serialized_buffer, serialized_length });
}

static void
strbuf_i64(strbuf* buf, i64 value) // Remaining space of at least 20 will handle all values.
{
	char serialized_buffer[20] = {0};
	i32  serialized_length     = serialize_i64(serialized_buffer, sizeof(serialized_buffer), value);
	strbuf_str(buf, (str) { serialized_buffer, serialized_length });
}

static void
strbuf_8b(strbuf* buf, u8 value)
{
	char digits[8] = {0};
	for (i32 i = 0; i < countof(digits); i += 1)
	{
		digits[i] = '0' + ((value >> (7 - i)) & 1);
	}
	strbuf_str(buf, (str) { digits, countof(digits) });
}

static void
strbuf_char_n(strbuf* buf, char value, i64 count)
{
	assert(count >= 0);

	if (buf->length + count <= buf->size)
	{
		memset(buf->data + buf->length, value, count);
		buf->length += count;
	}
	else
	{
		error_capacity();
	}
}

#undef error_capacity
