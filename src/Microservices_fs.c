static b32 // Directory is empty.
create_dir(str dir_path, b32 delete_content)
{
	strbuf formatted_dir_path = strbuf(256);
	for (i32 i = 0; i < dir_path.length; i += 1)
	{
		if (dir_path.data[i] == '/')
		{
			strbuf_char(&formatted_dir_path, '\\');
		}
		else
		{
			strbuf_char(&formatted_dir_path, dir_path.data[i]);
		}
	}
	strbuf_char(&formatted_dir_path, '\\');
	strbuf_char(&formatted_dir_path, '\0');

	if (!MakeSureDirectoryPathExists(formatted_dir_path.data))
	{
		error("Failed create directory path \"%s\".", formatted_dir_path.data);
	}

	b32 empty = PathIsDirectoryEmptyA(formatted_dir_path.data);
	if (!empty && delete_content)
	{
		strbuf double_nullterminated_output_dir_path = strbuf(256);
		strbuf_str (&double_nullterminated_output_dir_path, dir_path);
		strbuf_char(&double_nullterminated_output_dir_path, '\0');
		strbuf_char(&double_nullterminated_output_dir_path, '\0');

		SHFILEOPSTRUCTA file_operation =
			{
				.wFunc             = FO_DELETE,
				.pFrom             = double_nullterminated_output_dir_path.data,
				.pTo               = "\0",
				.fFlags            = FOF_NOCONFIRMATION,
				.lpszProgressTitle = "",
			};

		if (SHFileOperationA(&file_operation) || file_operation.fAnyOperationsAborted)
		{
			error("Clearing directory \"%s\" failed.", formatted_dir_path.data);
		}

		if (!MakeSureDirectoryPathExists(formatted_dir_path.data)) // Create the directory since emptying it will delete it too.
		{
			error("Failed create directory path \"%s\".", formatted_dir_path.data);
		}
	}

	return empty;
}

static HANDLE
create_file_writing_handle(str file_path)
{
	str file_dir = file_path;
	while (file_dir.length && file_dir.data[file_dir.length - 1] != '/' && file_dir.data[file_dir.length - 1] != '\\')
	{
		file_dir.length -= 1;
	}
	create_dir(file_dir, false);

	strbuf file_path_cstr = strbuf(256);
	strbuf_str (&file_path_cstr, file_path);
	strbuf_char(&file_path_cstr, '\0');

	HANDLE file_handle = CreateFileA(file_path_cstr.data, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (file_handle == INVALID_HANDLE_VALUE)
	{
		error("Failed to open \"%s\" for writing.", file_path_cstr.data);
	}

	return file_handle;
}

static void
close_file_writing_handle(HANDLE file_writing_handle)
{
	if (!CloseHandle(file_writing_handle))
	{
		error("Failed to close file handle.");
	}
}

static void
write_raw_data(HANDLE file_writing_handle, void* data, i64 size)
{
	DWORD bytes_written = {0};
	if (!WriteFile(file_writing_handle, data, (u32) size, &bytes_written, 0) || bytes_written != (u32) size)
	{
		error("Failed to write.");
	}
}

static void
write_flush_strbuf(HANDLE file_writing_handle, strbuf* buf)
{
	write_raw_data(file_writing_handle, buf->data, buf->length);
	buf->length = 0;
}

static str
alloc_read_file(str file_path)
{
	str file_content = {0};

	strbuf file_path_cstr = strbuf(256);
	strbuf_str (&file_path_cstr, file_path);
	strbuf_char(&file_path_cstr, '\0');

	FILE* file = fopen(file_path_cstr.data, "rb");
	if (!file)
	{
		error("`fopen` failed. Does the file \"%s\" exist?", file_path_cstr.data);
	}

	if (fseek(file, 0, SEEK_END))
	{
		error("`fseek` failed.");
	}

	file_content.length = ftell(file);
	if (file_content.length == -1)
	{
		error("`ftell` failed.");
	}

	if (file_content.length)
	{
		alloc(&file_content.data, file_content.length);

		if (fseek(file, 0, SEEK_SET))
		{
			error("`fseek` failed.");
		}

		if (fread(file_content.data, file_content.length, 1, file) != 1)
		{
			error("`fread` failed.");
		}
	}

	if (fclose(file))
	{
		error("`fclose` failed.");
	}

	return file_content;
}
