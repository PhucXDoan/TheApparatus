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
		error("`fopen` failed. Does the file exist?");
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

	if (!(file_header->bfType == (u16('B') | (u16('M') << 8)) && file_header->bfSize == file_content.length))
	{
		error("Invalid BMP file header.");
	}

	union BMPDIBHeader* dib_header = (union BMPDIBHeader*) &file_header[1];
	if
	(
		u64(file_content.length) < sizeof(struct BMPFileHeader) + sizeof(dib_header->size) ||
		u64(file_content.length) < sizeof(struct BMPFileHeader) + dib_header->size
	)
	{
		error("Source too small for BMP DIB header.");
	}

	//
	// Determine configuration.
	//

	if
	(
		dib_header->size == sizeof(struct BMPDIBHeaderInfo) &&
		(
			dib_header->info.biCompression == BMPCompression_BI_RGB &&
			dib_header->info.biBitCount == 1 &&
			dib_header->info.biHeight > 0
		)
	)
	{
		u32 bytes_per_row = ((dib_header->info.biWidth + 7) / 8 + 3) / 4 * 4;
		if
		(
			!(
				dib_header->info.biPlanes == 1 &&
				dib_header->info.biWidth > 0 &&
				dib_header->info.biHeight != 0 &&
				(!dib_header->v5.bV5SizeImage || dib_header->v5.bV5SizeImage == bytes_per_row * dib_header->v5.bV5Height)
			)
		)
		{
			error("DIB with invalid field.");
		}

		result =
			(struct BMP)
			{
				.dim_x = dib_header->info.biWidth,
				.dim_y = dib_header->info.biHeight,
			};
		alloc(&result.data, dib_header->info.biWidth * dib_header->info.biHeight);

		for (i64 y = 0; y < dib_header->info.biHeight; y += 1)
		{
			for (i32 byte_index = 0; byte_index < dib_header->info.biWidth / 8; byte_index += 1)
			{
				for (u8 bit_index = 0; bit_index < 8; bit_index += 1)
				{
					if (byte_index * 8 + bit_index < dib_header->info.biWidth)
					{
						result.data[y * result.dim_x + byte_index * 8 + bit_index] =
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
		dib_header->size == sizeof(struct BMPDIBHeaderV4) &&
		(
			dib_header->v4.bV4Compression == BMPCompression_BI_BITFIELDS &&
			dib_header->v4.bV4BitCount == 32 &&
			dib_header->v4.bV4Height > 0
		) ||
		dib_header->size == sizeof(struct BMPDIBHeaderV5) &&
		(
			dib_header->v5.bV5Compression == BMPCompression_BI_RGB &&
			dib_header->v5.bV5BitCount == 24 &&
			dib_header->v5.bV5Height > 0
		)
	)
	{
		i32 bytes_per_row   = {0};
		i32 bytes_per_pixel = {0};
		i32 r_index         = {0};
		i32 g_index         = {0};
		i32 b_index         = {0};
		i32 a_index         = {0};

		if (dib_header->size == sizeof(struct BMPDIBHeaderV4))
		{
			bytes_per_row   = (dib_header->v4.bV4Width * (dib_header->v4.bV4BitCount / 8) + 3) / 4 * 4;
			bytes_per_pixel = dib_header->v4.bV4BitCount / 8;

			#define DETERMINE(MASK) \
				( \
					(MASK) == (u32(0xFF) <<  0) ? 0 : \
					(MASK) == (u32(0xFF) <<  8) ? 1 : \
					(MASK) == (u32(0xFF) << 16) ? 2 : 3 \
				)
			r_index = DETERMINE(dib_header->v4.bV4RedMask);
			g_index = DETERMINE(dib_header->v4.bV4GreenMask);
			b_index = DETERMINE(dib_header->v4.bV4BlueMask);
			a_index = DETERMINE(dib_header->v4.bV4AlphaMask);
			#undef DETERMINE

			result.dim_x    = dib_header->v4.bV4Width;
			result.dim_y    = dib_header->v4.bV4Height;

			if
			(
				!(
					dib_header->v4.bV4Planes == 1 &&
					dib_header->v4.bV4Width > 0 &&
					dib_header->v4.bV4Height != 0 &&
					!(dib_header->v4.bV4RedMask & dib_header->v4.bV4GreenMask & dib_header->v4.bV4BlueMask & dib_header->v4.bV4AlphaMask) &&
					(dib_header->v4.bV4RedMask | dib_header->v4.bV4GreenMask | dib_header->v4.bV4BlueMask | dib_header->v4.bV4AlphaMask) == u32(-1) &&
					(!dib_header->v4.bV4SizeImage || dib_header->v4.bV4SizeImage == u32(bytes_per_row * dib_header->v4.bV4Height))
				)
			)
			{
				error("DIB with invalid field.");
			}
		}
		else if (dib_header->size == sizeof(struct BMPDIBHeaderV5))
		{
			bytes_per_row   = (dib_header->v5.bV5Width * (dib_header->v5.bV5BitCount / 8) + 3) / 4 * 4;
			bytes_per_pixel = dib_header->v5.bV5BitCount / 8;
			r_index         =  2;
			g_index         =  1;
			b_index         =  0;
			a_index         = -1;
			result.dim_x    = dib_header->v5.bV5Width;
			result.dim_y    = dib_header->v5.bV5Height;

			if
			(
				!(
					dib_header->v5.bV5Planes == 1 &&
					dib_header->v5.bV5Width > 0 &&
					dib_header->v5.bV5Height != 0 &&
					(!dib_header->v5.bV5SizeImage || dib_header->v5.bV5SizeImage == u32(bytes_per_row * dib_header->v5.bV5Height))
				)
			)
			{
				error("DIB with invalid field.");
			}
		}

		alloc(&result.data, result.dim_x * result.dim_y);

		for (i64 y = 0; y < result.dim_y; y += 1)
		{
			for (i64 x = 0; x < result.dim_x; x += 1)
			{
				u8* channels =
					((u8*) file_content.data) + file_header->bfOffBits
						+ y * bytes_per_row
						+ x * bytes_per_pixel;

				result.data[y * result.dim_x + x] =
					(struct BMPPixel)
					{
						.r = r_index == -1 ? 0   : channels[r_index],
						.g = g_index == -1 ? 0   : channels[g_index],
						.b = b_index == -1 ? 0   : channels[b_index],
						.a = a_index == -1 ? 255 : channels[a_index],
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
	char file_path_cstr[256] = {0};
	if (file_path.length >= countof(file_path_cstr))
	{
		error("File path too long.");
	}
	memmove(file_path_cstr, file_path.data, file_path.length);

	FILE* file = fopen(file_path_cstr, "wb");
	if (!file)
	{
		error("`fopen` failed.");
	}

	struct BMPDIBHeaderV4 dib_header =
		{
			.bV4Size          = sizeof(dib_header),
			.bV4Width         = src.dim_x,
			.bV4Height        = src.dim_y,
			.bV4Planes        = 1,
			.bV4BitCount      = bitsof(struct BMPPixel),
			.bV4Compression   = BMPCompression_BI_BITFIELDS,
			.bV4RedMask       = u32(0xFF) << (offsetof(struct BMPPixel, r) * 8),
			.bV4GreenMask     = u32(0xFF) << (offsetof(struct BMPPixel, g) * 8),
			.bV4BlueMask      = u32(0xFF) << (offsetof(struct BMPPixel, b) * 8),
			.bV4AlphaMask     = u32(0xFF) << (offsetof(struct BMPPixel, a) * 8),
			.bV4CSType        = BMPColorSpace_LCS_CALIBRATED_RGB,
		};
	struct BMPFileHeader file_header =
		{
			.bfType    = u16('B') | (u16('M') << 8),
			.bfSize    = sizeof(file_header) + sizeof(dib_header) + src.dim_x * src.dim_y * sizeof(struct BMPPixel),
			.bfOffBits = sizeof(file_header) + sizeof(dib_header),
		};

	if (fwrite(&file_header, sizeof(file_header), 1, file) != 1)
	{
		error("Failed to write BMP file header.");
	}

	if (fwrite(&dib_header, sizeof(dib_header), 1, file) != 1)
	{
		error("Failed to write DIB header.");
	}

	i64 write_size = src.dim_x * src.dim_y * sizeof(*src.data);
	if (write_size && fwrite(src.data, write_size, 1, file) != 1)
	{
		error("Failed to write pixel data.");
	}

	if (fclose(file))
	{
		error("`fclose` failed.");
	}
}
