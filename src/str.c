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
