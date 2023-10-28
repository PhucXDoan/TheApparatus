static b32
bmp_monochrome_get(struct BMPMonochrome src, i32 x, i32 y)
{
	assert(0 <= x && x < src.dim_x);
	assert(0 <= y && y < src.dim_y);
	b32 result =
		(
			src.data[(y * src.dim_x + x) / bitsof(*src.data)]
				>>  ((y * src.dim_x + x) % bitsof(*src.data))
		) & 1;
	return result;
}

static void
bmp_monochrome_set(struct BMPMonochrome* src, i32 x, i32 y, b32 value)
{
	assert(0 <= x && x < src->dim_x);
	assert(0 <= y && y < src->dim_y);

	src->data[(y * src->dim_x + x) / bitsof(*src->data)] &=       ~(1 << ((y * src->dim_x + x) % bitsof(*src->data)));
	src->data[(y * src->dim_x + x) / bitsof(*src->data)] |= (!!value) << ((y * src->dim_x + x) % bitsof(*src->data));
}

#define error_f(STRLIT, ...) error("(\"%.*s\") " STRLIT, i32(file_path.length), file_path.data,##__VA_ARGS__)
static struct BMP
bmp_malloc_read_file(str file_path)
{
	struct BMP result = {0};

	//
	// Get file content.
	//

	str file_content = {0};

	char file_path_cstr[256] = {0};
	if (file_path.length >= countof(file_path_cstr))
	{
		error_f("File path too long.");
	}
	memmove(file_path_cstr, file_path.data, file_path.length);

	FILE* file = fopen(file_path_cstr, "rb");
	if (!file)
	{
		error_f("`fopen` failed. Does the file exist?");
	}

	if (fseek(file, 0, SEEK_END))
	{
		error_f("`fseek` failed.");
	}

	file_content.length = ftell(file);
	if (file_content.length == -1)
	{
		error_f("`ftell` failed.");
	}

	if (file_content.length)
	{
		file_content.data = malloc(file_content.length * sizeof(*file_content.data));
		if (!file_content.data)
		{
			error_f("Failed to allocate enough memory");
		}

		if (fseek(file, 0, SEEK_SET))
		{
			error_f("`fseek` failed.");
		}

		if (fread(file_content.data, file_content.length, 1, file) != 1)
		{
			error_f("`fread` failed.");
		}
	}

	if (fclose(file))
	{
		error_f("`fclose` failed.");
	}

	//
	// Parse file.
	//

	if (file_content.length < sizeof(struct BMPFileHeader))
	{
		error_f("Source too small for BMP file header.");
	}
	struct BMPFileHeader* file_header = (struct BMPFileHeader*) file_content.data;

	if (!(file_header->bfType == (u16('B') | (u16('M') << 8)) && file_header->bfSize == file_content.length))
	{
		error_f("Invalid BMP file header.");
	}

	union BMPDIBHeader* dib_header = (union BMPDIBHeader*) &file_header[1];
	if
	(
		u64(file_content.length) < sizeof(struct BMPFileHeader) + sizeof(dib_header->size) ||
		u64(file_content.length) < sizeof(struct BMPFileHeader) + dib_header->size
	)
	{
		error_f("Source too small for BMP DIB header.");
	}

	switch (dib_header->size)
	{
		case sizeof(struct BMPDIBHeaderCore):
		{
			error_f("BMPDIBHeaderCore not supported... yet.");
		} break;

		case sizeof(struct BMPDIBHeaderInfo):
		{
			error_f("BMPDIBHeaderInfo not supported... yet.");
		} break;

		case sizeof(struct BMPDIBHeaderV4):
		{
			error_f("BMPDIBHeaderV4 not supported... yet.");
		} break;

		case sizeof(struct BMPDIBHeaderV5):
		{
			if (!(dib_header->v5.bV5Planes == 1))
			{
				error_f("BMPDIBHeaderV5 with invalid field(s).");
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
						error_f("Bitmap DIB header (BITMAPV5HEADER) with an invalid field.");
					}

					if
					(
						dib_header->v5.bV5BitCount == 24 &&
						dib_header->v5.bV5Compression == BMPCompression_BI_RGB &&
						dib_header->v5.bV5Height > 0
					)
					{
						u32 bytes_per_row = (dib_header->v5.bV5Width * (dib_header->v5.bV5BitCount / bitsof(u8)) + 3) / 4 * 4;
						if (dib_header->v5.bV5SizeImage != bytes_per_row * dib_header->v5.bV5Height)
						{
							error_f("Bitmap DIB header (BITMAPV5HEADER) configured with an invalid field.");
						}

						result =
							(struct BMP)
							{
								.dim_x = dib_header->v5.bV5Width,
								.dim_y = dib_header->v5.bV5Height,
								.data  = malloc(dib_header->v5.bV5Width * dib_header->v5.bV5Height * sizeof(*result.data)),
							};
						if (!result.data)
						{
							error_f("Failed to allocate.");
						}

						for (i64 y = 0; y < dib_header->v5.bV5Height; y += 1)
						{
							for (i64 x = 0; x < dib_header->v5.bV5Width; x += 1)
							{
								u8* channels =
									((u8*) file_content.data) + file_header->bfOffBits
										+ y * bytes_per_row
										+ x * (dib_header->v5.bV5BitCount / bitsof(u8));

								// It's probably somewhere, but couldn't find where the byte ordering was explicitly described in the documentation.
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
						error_f("Bitmap with DIB header (BITMAPV5HEADER) configuration that's not yet supported.");
					}
				} break;

				case BMPCompression_BI_RLE8:
				{
					error_f("BI_RLE8 not handled... yet.");
				} break;

				case BMPCompression_BI_RLE4:
				{
					error_f("BI_RLE4 not handled... yet.");
				} break;

				case BMPCompression_BI_BITFIELDS:
				{
					error_f("BI_BITFIELDS not handled... yet.");
				} break;

				case BMPCompression_BI_JPEG:
				{
					error_f("BI_JPEG not handled... yet.");
				} break;

				case BMPCompression_BI_PNG:
				{
					error_f("BI_PNG not handled... yet.");
				} break;

				case BMPCompression_BI_CMYK:
				{
					error_f("BI_CMYK not handled... yet.");
				} break;

				case BMPCompression_BI_CMYKRLE8:
				{
					error_f("BI_CMYKRLE8 not handled... yet.");
				} break;

				case BMPCompression_BI_CMYKRLE4:
				{
					error_f("BI_CMYKRLE4 not handled... yet.");
				} break;

				default:
				{
					error_f("Unknown compression method 0x%02X.", dib_header->v5.bV5Compression);
				} break;
			}
		} break;
	}

	return result;
}
#undef error_f

#define error_f(STRLIT, ...) error("(\"%.*s\") " STRLIT, i32(file_path.length), file_path.data,##__VA_ARGS__)
static void
bmp_export(struct BMP src, str file_path)
{
	char file_path_cstr[256] = {0};
	if (file_path.length >= countof(file_path_cstr))
	{
		error_f("File path too long.");
	}
	memmove(file_path_cstr, file_path.data, file_path.length);

	FILE* file = fopen(file_path_cstr, "wb");
	if (!file)
	{
		error_f("`fopen` failed.");
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
		error_f("Failed to write BMP file header.");
	}

	if (fwrite(&dib_header, sizeof(dib_header), 1, file) != 1)
	{
		error_f("Failed to write DIB header.");
	}

	i64 write_size = src.dim_x * src.dim_y * sizeof(*src.data);
	if (write_size && fwrite(src.data, write_size, 1, file) != 1)
	{
		error_f("Failed to write pixel data.");
	}

	if (fclose(file))
	{
		error_f("`fclose` failed.");
	}
}
#undef error_f

#define error_f(STRLIT, ...) error("(\"%.*s\") " STRLIT, i32(file_path.length), file_path.data,##__VA_ARGS__)
static void
bmp_monochrome_export(struct BMPMonochrome src, str file_path)
{
	char file_path_cstr[256] = {0};
	if (file_path.length >= countof(file_path_cstr))
	{
		error_f("File path too long.");
	}
	memmove(file_path_cstr, file_path.data, file_path.length);

	FILE* file = fopen(file_path_cstr, "wb");
	if (!file)
	{
		error_f("`fopen` failed.");
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
			.bfSize    = sizeof(file_header) + sizeof(dib_header) + sizeof(bmi_colors) + (src.dim_x + bitsof(u8) * 4 - 1) / (bitsof(u8) * 4) * src.dim_y,
			.bfOffBits = sizeof(file_header) + sizeof(dib_header) + sizeof(bmi_colors),
		};

	if (fwrite(&file_header, sizeof(file_header), 1, file) != 1)
	{
		error_f("Failed to write BMP file header.");
	}

	if (fwrite(&dib_header, sizeof(dib_header), 1, file) != 1)
	{
		error_f("Failed to write DIB header.");
	}

	if (fwrite(&bmi_colors, sizeof(bmi_colors), 1, file) != 1)
	{
		error_f("Failed to write color table.");
	}

	for (i32 y = 0; y < src.dim_y; y += 1)
	{
		u8 byte = 0;

		for
		(
			i32 x = 0;
			x < src.dim_x;
			x += bitsof(*src.data)
		)
		{
			for (u8 i = 0; i < bitsof(*src.data); i += 1)
			{
				byte <<= 1;
				if (x + i < src.dim_x)
				{
					byte |= bmp_monochrome_get(src, x + i, y);
				}
			}

			if (fwrite(&byte, sizeof(byte), 1, file) != 1)
			{
				error_f("Failed to write pixel.");
			}
		}

		for (i32 i = 0; i < 4 - i32((src.dim_x + bitsof(*src.data) - 1) / bitsof(*src.data) % 4); i += 1)
		{
			if (fwrite(&(u8) {0}, sizeof(u8), 1, file) != 1)
			{
				error_f("Failed to write row padding byte.");
			}
		}
	}

	if (fclose(file))
	{
		error_f("`fclose` failed.");
	}
}
#undef error_f
