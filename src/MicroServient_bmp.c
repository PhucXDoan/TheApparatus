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
				case BMPCompression_RGB:
				{
					result.dim_x = dib_header->v5.bV5Width;
					result.dim_y = abs(dib_header->v5.bV5Height);

					debug_halt();
				} break;

				case BMPCompression_RLE8:
				{
					error("BMPCompression_RLE8 not handled... yet.");
				} break;

				case BMPCompression_RLE4:
				{
					error("BMPCompression_RLE4 not handled... yet.");
				} break;

				case BMPCompression_BITFIELDS:
				{
					error("BMPCompression_BITFIELDS not handled... yet.");
				} break;

				case BMPCompression_JPEG:
				{
					error("BMPCompression_JPEG not handled... yet.");
				} break;

				case BMPCompression_PNG:
				{
					error("BMPCompression_PNG not handled... yet.");
				} break;

				case BMPCompression_CMYK:
				{
					error("BMPCompression_CMYK not handled... yet.");
				} break;

				case BMPCompression_CMYKRLE8:
				{
					error("BMPCompression_CMYKRLE8 not handled... yet.");
				} break;

				case BMPCompression_CMYKRLE4:
				{
					error("BMPCompression_CMYKRLE4 not handled... yet.");
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
