#define error(STRLIT, ...) error_abort("(\"%.*s\") :: " STRLIT, i32(file_path.length), file_path.data,##__VA_ARGS__)
static struct BMP
bmp_malloc_read_file(str file_path)
{
	struct BMP result = {0};

	str file_content = malloc_read_file(file_path);

	if (file_content.length < sizeof(struct BMPFileHeader))
	{
		error("File too small for BMP file header.");
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
		error("File too small for BMP DIB header.");
	}

	switch (dib_header->size)
	{
		case sizeof(struct BMPDIBHeaderCore):
		{
			error("BMPDIBHeaderCore not supported... yet.");
		} break;

		case sizeof(struct BMPDIBHeaderInfo):
		{
			error("BMPDIBHeaderInfo not supported... yet.");
		} break;

		case sizeof(struct BMPDIBHeaderV4):
		{
			error("BMPDIBHeaderV4 not supported... yet.");
		} break;

		case sizeof(struct BMPDIBHeaderV5):
		{
			if (!(dib_header->v5.bV5Planes == 1))
			{
				error("BMPDIBHeaderV5 with invalid field(s).");
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
						error("Bitmap DIB header (BITMAPV5HEADER) with an invalid field.");
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
							error("Bitmap DIB header (BITMAPV5HEADER) configured with an invalid field.");
						}

						result =
							(struct BMP)
							{
								.dim_x = dib_header->v5.bV5Width,
								.dim_y = dib_header->v5.bV5Height,
								.data  = malloc(dib_header->v5.bV5Width * dib_header->v5.bV5Height * sizeof(struct BMPPixel)),
							};

						if (!result.data)
						{
							error("Failed to allocate.");
						}

						for (i64 y = 0; y < dib_header->v5.bV5Height; y += 1)
						{
							for (i64 x = 0; x < dib_header->v5.bV5Width; x += 1)
							{
								u8* channels = ((u8*) file_content.data) + file_header->bfOffBits + y * bytes_per_row;

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
				{
					error("BI_RLE8 not handled... yet.");
				} break;

				case BMPCompression_BI_RLE4:
				{
					error("BI_RLE4 not handled... yet.");
				} break;

				case BMPCompression_BI_BITFIELDS:
				{
					error("BI_BITFIELDS not handled... yet.");
				} break;

				case BMPCompression_BI_JPEG:
				{
					error("BI_JPEG not handled... yet.");
				} break;

				case BMPCompression_BI_PNG:
				{
					error("BI_PNG not handled... yet.");
				} break;

				case BMPCompression_BI_CMYK:
				{
					error("BI_CMYK not handled... yet.");
				} break;

				case BMPCompression_BI_CMYKRLE8:
				{
					error("BI_CMYKRLE8 not handled... yet.");
				} break;

				case BMPCompression_BI_CMYKRLE4:
				{
					error("BI_CMYKRLE4 not handled... yet.");
				} break;

				default:
				{
					error("Unknown compression method 0x%02X.", dib_header->v5.bV5Compression);
				} break;
			}
		} break;
	}

	free_read_file(&file_content);
	return result;
}
#undef error

#define error(STRLIT, ...) error_abort("(\"%.*s\") :: " STRLIT, i32(file_path.length), file_path.data,##__VA_ARGS__)
static void
bmp_export(struct BMP src, str file_path)
{
	error("TODO");
}
#undef error

static void
bmp_free_read_file(struct BMP* bmp)
{
	free(bmp->data);
	*bmp = (struct BMP) {0};
}

//
// Documentation.
//

/* [Overview].
	TODO
*/
