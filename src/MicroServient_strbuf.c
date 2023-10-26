#define error() error_abort("`%s` reached maximum capacity.", __func__)

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
		error();
	}

	return result;
}

static str
strbuf_char_n(struct StrBuf* buf, char value, i64 count)
{
	i64 capped_count = i64_max(count, 0);
	str result       =
		{
			.data   = buf->data + buf->length,
			.length = i64_min(capped_count, buf->size - buf->length)
		};

	memset(result.data, value, result.length);
	buf->length += result.length;

	if (result.length != capped_count)
	{
		error();
	}

	return result;
}

#define strbuf_strbuf(STRBUF_PTR, STRBUF) strbuf_str((STRBUF_PTR), (STRBUF).str)
static str
strbuf_str(struct StrBuf* buf, str value)
{
	i64 copy_amount = i64_min(value.length, buf->size - buf->length);
	str result      =
		{
			.data   = buf->data + buf->length,
			.length = copy_amount
		};

	memmove(result.data, value.data, result.length);
	buf->length += result.length;

	if (result.length != value.length)
	{
		error();
	}

	return result;
}

static str
strbuf_cstr(struct StrBuf* buf, char* value)
{
	str string = { .data = value, .length = strlen(value) };
	str result = strbuf_str(buf, string);

	if (result.length != string.length)
	{
		error();
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

#undef error