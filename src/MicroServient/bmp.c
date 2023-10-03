static b32 // Error.
bmp_malloc_read_file(struct BMP* dst, str file_path)
{
	b32 err = true;

	str file_content = {0};
	if (!malloc_read_file(&file_content, file_path))
	{
		str stream = file_content;

		struct BMPFileHeader file_header = {0};
		if (eat_stream(&file_header, &stream))
		{
			if
			(
				((char*) &file_header.bfType)[0] == 'B' &&
				((char*) &file_header.bfType)[1] == 'M' &&
				file_header.bfSize == file_content.length
			)
			{
				u32 dib_header_size = 0;
				if (eat_stream(&dib_header_size, (str[]) { stream }))
				{
					switch (dib_header_size)
					{
						case sizeof(struct BMPDIBHeaderCore):
						{
							error_unhandled;
						} break;

						case sizeof(struct BMPDIBHeaderInfo):
						{
							error_unhandled;
						} break;

						case sizeof(struct BMPDIBHeaderV4):
						{
							error_unhandled;
						} break;

						case sizeof(struct BMPDIBHeaderV5):
						{
							struct BMPDIBHeaderV5 v5 = {0};
							if (eat_stream(&v5, &stream))
							{
								debug_halt();
							}
						} break;
					}
				}
			}
		}

		free_read_file(&file_content);
	}

	return err;
}

static void
bmp_free_read_file(struct BMP* bmp)
{
	*bmp = (struct BMP) {0};
}

//
// Documentation.
//

/* [Overview].
	TODO
*/
