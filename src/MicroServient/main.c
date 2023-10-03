#define _CRT_SECURE_NO_DEPRECATE 1
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "defs.h"
#include "misc.c"
#include "bmp.c"

int
main(void)
{
	struct BMP bmp = {0};
	if (bmp_malloc_read_file(&bmp, str("W:/data/wordbites_0.bmp")))
	{
		error_unhandled;
	}

//	if (bmp_file.length < sizeof(struct BMPFileHeader) + sizeof(struct BMPInfoHeader))
//	{
//		error();
//	}
//	struct BMPFileHeader* input_bmp_file_header = (struct BMPFileHeader*) input_bmp_file_data;
//	struct BMPInfoHeader* input_bmp_info_header = (struct BMPInfoHeader*) (input_bmp_file_data + sizeof(struct BMPFileHeader));
//
//	if (!(input_bmp_file_header->header_field[0] == 'B' && input_bmp_file_header->header_field[1] == 'M'))
//	{
//		error();
//	}
//	if (input_bmp_file_header->file_size != bmp_file.length)
//	{
//		error();
//	}
//	if (input_bmp_info_header->header_size < sizeof(struct BMPInfoHeader))
//	{
//		error();
//	}
//	if (input_bmp_info_header->image_width < 0)
//	{
//		error();
//	}
//	if (input_bmp_info_header->image_height < 0)
//	{
//		error();
//	}
//	if (input_bmp_info_header->color_plane_count != 1)
//	{
//		error();
//	}
//	if (input_bmp_info_header->pixel_bitwidth != 24)
//	{
//		error();
//	}
//	if (input_bmp_info_header->compression_method)
//	{
//		error();
//	}
//
//	u32 padded_row_size =
//		((u32) input_bmp_info_header->image_width * (input_bmp_info_header->pixel_bitwidth / 8) + 3)
//			/ 4
//			* 4;
//
//	if (input_bmp_info_header->image_size != padded_row_size * input_bmp_info_header->image_height)
//	{
//		error();
//	}
//	if (input_bmp_info_header->palatte_color_count)
//	{
//		error();
//	}
//
//	struct LERGBA* input_pixels = malloc(sizeof(struct LERGBA) * input_bmp_info_header->image_width * input_bmp_info_header->image_height);
//	if (!input_pixels)
//	{
//		error();
//	}

	debug_halt();

	bmp_free_read_file(&bmp);

	return 0;
}
