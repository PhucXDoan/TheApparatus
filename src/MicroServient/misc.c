#if DEBUG
#define debug_halt() __debugbreak()
#define error_unhandled \
	do \
	{ \
		debug_halt(); \
		exit(1); \
	} \
	while (false)
#else
#define error_unhandled \
	do \
	{ \
		exit(1); \
	} \
	while (false)
#endif

static b32 // Error.
malloc_read_file(str* dst, str file_path)
{
	b32 err = true;
	*dst = (str) {0};

	char file_path_cstrbuf[256] = {0};
	if (file_path.length < countof(file_path_cstrbuf))
	{
		memmove(file_path_cstrbuf, file_path.data, file_path.length);

		FILE* file = fopen(file_path_cstrbuf, "rb");
		if (file)
		{
			if (!fseek(file, 0, SEEK_END))
			{
				dst->length = ftell(file);
				if (dst->length != -1)
				{
					dst->data = malloc(dst->length);
					if (dst->data)
					{
						if (!fseek(file, 0, SEEK_SET))
						{
							if (!fread(dst->data, dst->length, 1, file) != 1)
							{
								err = false;
							}
						}
					}
				}
			}

			if (fclose(file))
			{
				err = true;
			}
		}
	}


	if (err)
	{
		if (dst->data)
		{
			free(dst->data);
		}

		*dst = (str) {0};
	}

	return err;
}

static void
free_read_file(str* src)
{
	free(src->data);
	*src = (str) {0};
}
