static struct BMP
bmp_alloc_read_file(str file_path)
{
	struct BMP result = {0};

	//
	// Get file content.
	//

	str file_content = {0};

	char file_path_cstr[256] = {0};
	if (file_path.length >= countof(file_path_cstr))
	{
		error("File path too long.");
	}
	memmove(file_path_cstr, file_path.data, file_path.length);

	FILE* file = fopen(file_path_cstr, "rb");
	if (!file)
	{
		error("`fopen` failed. Does the file \"%s\" exist?", file_path_cstr);
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

	//
	// Parse file.
	//

	if (file_content.length < sizeof(struct BMPFileHeader))
	{
		error("Source too small for BMP file header.");
	}
	struct BMPFileHeader* file_header = (struct BMPFileHeader*) file_content.data;

	if (!(file_header->bfType == BMP_FILE_HEADER_SIGNATURE && file_header->bfSize == file_content.length))
	{
		error("Invalid BMP file header.");
	}

	struct BMPDIBHeader* dib_header = (struct BMPDIBHeader*) &file_header[1];
	if
	(
		u64(file_content.length) < sizeof(struct BMPFileHeader) + sizeof(dib_header->Size) ||
		u64(file_content.length) < sizeof(struct BMPFileHeader) + dib_header->Size
	)
	{
		error("Source too small for BMP DIB header.");
	}

	if (dib_header->Size < 40)
	{
		error("Unsupported DIB header size.");
	}

	//
	// Determine configuration.
	//

	if (dib_header->Planes != 1 && dib_header->Width < 0)
	{
		error("DIB with invalid field.");
	}

	if
	(
		dib_header->Compression == BMPCompression_RGB &&
		dib_header->BitCount == 1 &&
		dib_header->Height > 0
	)
	{
		u32 bytes_per_row = ((dib_header->Width + 7) / 8 + 3) / 4 * 4;
		if (dib_header->SizeImage && dib_header->SizeImage != bytes_per_row * dib_header->Height)
		{
			error("DIB with invalid \"SizeImage\" field.");
		}

		result =
			(struct BMP)
			{
				.dim.x = dib_header->Width,
				.dim.y = dib_header->Height,
			};
		alloc(&result.data, dib_header->Width * dib_header->Height);

		for (i64 y = 0; y < dib_header->Height; y += 1)
		{
			for (i32 byte_index = 0; byte_index < dib_header->Width / 8; byte_index += 1)
			{
				for (u8 bit_index = 0; bit_index < 8; bit_index += 1)
				{
					if (byte_index * 8 + bit_index < dib_header->Width)
					{
						result.data[y * result.dim.x + byte_index * 8 + bit_index] =
							(((file_content.data + file_header->bfOffBits)[y * bytes_per_row + byte_index] >> (7 - bit_index)) & 1)
								? (struct BMPPixel) { .r = 255, .g = 255, .b = 255, .a = 255, }
								: (struct BMPPixel) { .r = 0,   .g = 0,   .b = 0,   .a = 255, };
					}
				}
			}
		}
	}
	else if
	(
		(
			dib_header->Compression == BMPCompression_RGB ||
			dib_header->Compression == BMPCompression_BITFIELDS
		) &&
		dib_header->Height > 0
	)
	{
		i32 bytes_per_row   = (dib_header->Width * (dib_header->BitCount / 8) + 3) / 4 * 4;
		i32 bytes_per_pixel = dib_header->BitCount / 8;
		i32 index_r         = {0};
		i32 index_g         = {0};
		i32 index_b         = {0};
		i32 index_a         = {0};

		if (dib_header->Compression == BMPCompression_RGB)
		{
			index_r =  2;
			index_g =  1;
			index_b =  0;
			index_a = -1;
		}
		else if (dib_header->Size >= BMPDIBHEADER_MIN_SIZE_V3)
		{
			#define DETERMINE_MASK(MASK) \
				( \
					(MASK) == (u32(0xFF) <<  0) ? 0 : \
					(MASK) == (u32(0xFF) <<  8) ? 1 : \
					(MASK) == (u32(0xFF) << 16) ? 2 : 3 \
				)

			index_r = DETERMINE_MASK(dib_header->v2.RedMask);
			index_g = DETERMINE_MASK(dib_header->v2.GreenMask);
			index_b = DETERMINE_MASK(dib_header->v2.BlueMask);
			index_a = DETERMINE_MASK(dib_header->v3.AlphaMask);

			#undef DETERMINE_MASK
		}
		else
		{
			error("Unhandled BMP configuration.");
		}

		result.dim.x = dib_header->Width;
		result.dim.y = dib_header->Height;

		if (dib_header->SizeImage && dib_header->SizeImage != u32(bytes_per_row * dib_header->Height))
		{
			error("DIB with invalid \"SizeImage\" field.");
		}

		alloc(&result.data, result.dim.x * result.dim.y);

		for (i64 y = 0; y < result.dim.y; y += 1)
		{
			for (i64 x = 0; x < result.dim.x; x += 1)
			{
				u8* channels =
					((u8*) file_content.data) + file_header->bfOffBits
						+ y * bytes_per_row
						+ x * bytes_per_pixel;

				result.data[y * result.dim.x + x] =
					(struct BMPPixel)
					{
						.r = index_r == -1 ? 0   : channels[index_r],
						.g = index_g == -1 ? 0   : channels[index_g],
						.b = index_b == -1 ? 0   : channels[index_b],
						.a = index_a == -1 ? 255 : channels[index_a],
					};
			}
		}
	}
	else
	{
		error("Unhandled BMP configuration.");
	}


	return result;
}

static void
bmp_export(struct BMP src, str file_path)
{
	HANDLE file_handle = create_file_writing_handle(file_path);

	struct BMPDIBHeader dib_header =
		{
			.Size         = BMPDIBHEADER_MIN_SIZE_V3,
			.Width        = src.dim.x,
			.Height       = src.dim.y,
			.Planes       = 1,
			.BitCount     = bitsof(struct BMPPixel),
			.Compression  = BMPCompression_BITFIELDS,
			.v2.RedMask   = u32(0xFF) << (offsetof(struct BMPPixel, r) * 8),
			.v2.GreenMask = u32(0xFF) << (offsetof(struct BMPPixel, g) * 8),
			.v2.BlueMask  = u32(0xFF) << (offsetof(struct BMPPixel, b) * 8),
			.v3.AlphaMask = u32(0xFF) << (offsetof(struct BMPPixel, a) * 8),
		};
	struct BMPFileHeader file_header =
		{
			.bfType    = BMP_FILE_HEADER_SIGNATURE,
			.bfSize    = sizeof(file_header) + dib_header.Size + src.dim.x * src.dim.y * sizeof(struct BMPPixel),
			.bfOffBits = sizeof(file_header) + dib_header.Size,
		};

	write_raw_data(file_handle, &file_header, sizeof(file_header));
	write_raw_data(file_handle, &dib_header, dib_header.Size);
	write_raw_data(file_handle, src.data, src.dim.x * src.dim.y * sizeof(struct BMPPixel));

	close_file_writing_handle(file_handle);
}
