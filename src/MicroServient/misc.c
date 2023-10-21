#define error(STRLIT, ...) \
	error_abort("(\"%.*s\") :: " STRLIT, i32(file_path.length), file_path.data,##__VA_ARGS__)
static str
malloc_read_file(str file_path)
{
	str result = {0};

	//
	// Turn lengthed string into null-terminated string.
	//

	char file_path_cstr[256] = {0};
	if (file_path.length >= countof(file_path_cstr))
	{
		error("File path too long.");
	}
	memmove(file_path_cstr, file_path.data, file_path.length);

	//
	// Get file length.
	//

	FILE* file = fopen(file_path_cstr, "rb");
	if (!file)
	{
		error("`fopen` failed. Does the file exist?");
	}

	if (fseek(file, 0, SEEK_END))
	{
		error("`fseek` failed.");
	}

	result.length = ftell(file);
	if (result.length == -1)
	{
		error("`ftell` failed.");
	}

	//
	// Get file data.
	//

	if (result.length)
	{
		result.data = malloc(result.length);
		if (!result.data)
		{
			error("Failed to allocate enough memory");
		}

		if (fseek(file, 0, SEEK_SET))
		{
			error("`fseek` failed.");
		}

		if (fread(result.data, result.length, 1, file) != 1)
		{
			error("`fread` failed.");
		}
	}

	//
	// Clean up.
	//

	if (fclose(file))
	{
		error("`fclose` failed.");
	}

	return result;
}
#undef error

static void
free_read_file(str* src)
{
	free(src->data);
	*src = (str) {0};
}
