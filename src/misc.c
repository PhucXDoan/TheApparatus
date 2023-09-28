static u8 // Amount written. Will never exceed 255 since all serialized u64s will be 20 bytes or shorter.
serialize_u64(char* dst, u16 dst_size, u64 value) // dst_size of at least 20 will handle all values.
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

static u8 // Amount written. Will never exceed 255 since all serialized i64s will be 20 bytes or shorter.
serialize_i64(char* dst, u16 dst_size, i64 value) // dst_size of at least 20 will handle all values.
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
