#define _CRT_SECURE_NO_DEPRECATE 1
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "defs.h"
#include "misc.c"
#include "Microservient_bmp.c"

int
main(void)
{
	struct BMP bmp = bmp_malloc_read_file(str("W:/data/wordbites_1.bmp"));

	struct BMP modified =
		{
			.data  = malloc(bmp.dim_x * bmp.dim_y * sizeof(struct BMPPixel)),
			.dim_x = bmp.dim_y,
			.dim_y = bmp.dim_x,
		};
	if (!modified.data)
	{
		error_abort("");
	}

	for (i32 y = 0; y < modified.dim_y; y += 1)
	{
		for (i32 x = 0; x < modified.dim_x; x += 1)
		{
			modified.data[y * modified.dim_x + x] = bmp.data[x * bmp.dim_x + y];
		}
	}

	bmp_export(modified, str("W:/data/export.bmp"));

	//debug_halt();

	free(modified.data);
	bmp_free_read_file(&bmp);
	return 0;
}
