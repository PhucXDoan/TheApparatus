#define bmp_monochrome_calc_size(DIM_X, DIM_Y) (((DIM_X) * (DIM_Y) + 7) / 8)

static b32
bmp_monochrome_get(struct BMPMonochrome src, i32 x, i32 y)
{
	assert(0 <= x && x < src.dim_x);
	assert(0 <= y && y < src.dim_y);
	i32 bit_index = y * src.dim_x + x;
	b32 result    = (src.data[bit_index / 8] >> (bit_index % 8)) & 1;
	return result;
}

static void
bmp_monochrome_set(struct BMPMonochrome* src, i32 x, i32 y, b32 value)
{
	assert(0 <= x && x < src->dim_x);
	assert(0 <= y && y < src->dim_y);

	i32 bit_index = y * src->dim_x + x;
	src->data[bit_index / 8] &=       ~(1 << (bit_index % 8));
	src->data[bit_index / 8] |= (!!value) << (bit_index % 8);
}

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

	switch (dib_header->size)
	{
		case sizeof(struct BMPDIBHeaderInfo):
		{
			if (!(dib_header->info.biPlanes == 1))
			{
				error("DIB with invalid field.");
			}

			switch (dib_header->info.biCompression)
			{
				case BMPCompression_BI_RGB:
				{
					if
					(
						!(
							dib_header->info.biWidth > 0 &&
							dib_header->info.biHeight != 0 &&
							dib_header->info.biPlanes == 1
						)
					)
					{
						error("DIB with invalid field.");
					}

					if (dib_header->info.biBitCount == 1 && dib_header->info.biHeight > 0)
					{
						u32 bytes_per_row = ((dib_header->info.biWidth + 7) / 8 + 3) / 4 * 4;
						if (dib_header->info.biSizeImage && dib_header->info.biSizeImage != bytes_per_row * dib_header->info.biHeight)
						{
							error("DIB configured with an invalid field.");
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
					else
					{
						error("Unhandled DIB configuration.");
					}
				} break;

				case BMPCompression_BI_RLE8:
				case BMPCompression_BI_RLE4:
				case BMPCompression_BI_BITFIELDS:
				case BMPCompression_BI_JPEG:
				case BMPCompression_BI_PNG:
				case BMPCompression_BI_CMYK:
				case BMPCompression_BI_CMYKRLE8:
				case BMPCompression_BI_CMYKRLE4:
				default:
				{
					error("Unhandled compression method 0x%02X.", dib_header->info.biCompression);
				} break;
			}
		} break;

		case sizeof(struct BMPDIBHeaderV5):
		{
			if (!(dib_header->v5.bV5Planes == 1))
			{
				error("DIB with invalid field.");
			}

			switch (dib_header->v5.bV5Compression)
			{
				case BMPCompression_BI_RGB:
				{
					if
					(
						!(
							dib_header->v5.bV5Width > 0 &&
							dib_header->v5.bV5Height != 0 &&
							dib_header->v5.bV5Planes == 1
						)
					)
					{
						error("DIB with invalid field.");
					}

					if (dib_header->v5.bV5BitCount == 24 && dib_header->v5.bV5Height > 0)
					{
						u32 bytes_per_row = (dib_header->v5.bV5Width * (dib_header->v5.bV5BitCount / 8) + 3) / 4 * 4;
						if (dib_header->v5.bV5SizeImage != bytes_per_row * dib_header->v5.bV5Height)
						{
							error("DIB configured with an invalid field.");
						}

						result =
							(struct BMP)
							{
								.dim_x = dib_header->v5.bV5Width,
								.dim_y = dib_header->v5.bV5Height,
							};
						alloc(&result.data, dib_header->v5.bV5Width * dib_header->v5.bV5Height);

						for (i64 y = 0; y < dib_header->v5.bV5Height; y += 1)
						{
							for (i64 x = 0; x < dib_header->v5.bV5Width; x += 1)
							{
								u8* channels =
									((u8*) file_content.data) + file_header->bfOffBits
										+ y * bytes_per_row
										+ x * (dib_header->v5.bV5BitCount / bitsof(u8));

								result.data[y * result.dim_x + x] =
									(struct BMPPixel)
									{
										.r = channels[2],
										.g = channels[1],
										.b = channels[0],
										.a = 255,
									};
							}
						}
					}
					else
					{
						error("Bitmap with DIB header (BITMAPV5HEADER) configuration that's not yet supported.");
					}
				} break;

				case BMPCompression_BI_RLE8:
				case BMPCompression_BI_RLE4:
				case BMPCompression_BI_BITFIELDS:
				case BMPCompression_BI_JPEG:
				case BMPCompression_BI_PNG:
				case BMPCompression_BI_CMYK:
				case BMPCompression_BI_CMYKRLE8:
				case BMPCompression_BI_CMYKRLE4:
				default:
				{
					error("Unhandled compression method 0x%02X.", dib_header->v5.bV5Compression);
				} break;
			}
		} break;

		case sizeof(struct BMPDIBHeaderCore):
		case sizeof(struct BMPDIBHeaderV4):
		default:
		{
			error("Unhandled DIB header.");
		} break;
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

static void
bmp_monochrome_export(struct BMPMonochrome src, str file_path)
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

	struct BMPDIBHeaderInfo dib_header =
		{
			.biSize        = sizeof(dib_header),
			.biWidth       = src.dim_x,
			.biHeight      = src.dim_y,
			.biPlanes      = 1,
			.biBitCount    = 1,
			.biCompression = BMPCompression_BI_RGB,
		};
	struct BMPRGBQuad bmi_colors[] = // See: "BITMAPINFO" @ Source(22) @ Page(284).
		{
			{ .rgbRed = 0,   .rgbGreen = 0,   .rgbBlue = 0,   },
			{ .rgbRed = 255, .rgbGreen = 255, .rgbBlue = 255, },
		};
	struct BMPFileHeader file_header =
		{
			.bfType    = u16('B') | (u16('M') << 8),
			.bfSize    = sizeof(file_header) + sizeof(dib_header) + sizeof(bmi_colors) + ((src.dim_x + bitsof(u8) - 1) / bitsof(u8) + 3) / 4 * 4 * src.dim_y,
			.bfOffBits = sizeof(file_header) + sizeof(dib_header) + sizeof(bmi_colors),
		};

	if (fwrite(&file_header, sizeof(file_header), 1, file) != 1)
	{
		error("Failed to write BMP file header.");
	}

	if (fwrite(&dib_header, sizeof(dib_header), 1, file) != 1)
	{
		error("Failed to write DIB header.");
	}

	if (fwrite(&bmi_colors, sizeof(bmi_colors), 1, file) != 1)
	{
		error("Failed to write color table.");
	}

	for (i32 y = 0; y < src.dim_y; y += 1)
	{
		for (i32 src_byte_index = 0; src_byte_index < src.dim_x / 8; src_byte_index += 1)
		{
			u8 byte = 0;

			for (i32 src_bit_index = 0; src_bit_index < 8; src_bit_index += 1)
			{
				byte <<= 1;
				if (src_byte_index * 8 + src_bit_index < src.dim_x)
				{
					byte |= bmp_monochrome_get(src, src_byte_index * 8 + src_bit_index, y);
				}
			}

			if (fwrite(&byte, sizeof(byte), 1, file) != 1)
			{
				error("Failed to write pixel.");
			}
		}

		u64 padding = (4 - i32((src.dim_x + 7) / 8 % 4)) % 4;
		if (fwrite(&(u32) {0}, sizeof(u8), padding, file) != padding)
		{
			error("Failed to write row padding byte.");
		}
	}

	if (fclose(file))
	{
		error("`fclose` failed.");
	}
}
