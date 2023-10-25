#define _CRT_SECURE_NO_DEPRECATE 1
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "defs.h"
#include "misc.c"
#include "Microservient_bmp.c"

#define WORDBITES_BOARD_SLOTS_X  8
#define WORDBITES_BOARD_SLOTS_Y  9

#define PHONE_DIM_X 1170
#define PHONE_DIM_Y 2532

#define WORDBITES_RAW_SLOT_PX_DIM    140
#define WORDBITES_RAW_BOARD_PX_POS_X 25
#define WORDBITES_RAW_BOARD_PX_POS_Y 354 // Bottom-up.
#define WORDBITES_RAW_BOARD_PX_DIM_X (WORDBITES_BOARD_SLOTS_X * WORDBITES_RAW_SLOT_PX_DIM)
#define WORDBITES_RAW_BOARD_PX_DIM_Y (WORDBITES_BOARD_SLOTS_Y * WORDBITES_RAW_SLOT_PX_DIM)

static_assert(WORDBITES_RAW_BOARD_PX_POS_X + WORDBITES_RAW_BOARD_PX_DIM_X <= PHONE_DIM_X); // Should not obviously exceed phone screen boundries.
static_assert(WORDBITES_RAW_BOARD_PX_POS_Y + WORDBITES_RAW_BOARD_PX_DIM_Y <= PHONE_DIM_Y); // Should not obviously exceed phone screen boundries.

int
main(void)
{
	struct BMP bmp = bmp_malloc_read_file(str("W:/data/wordbites_1.bmp"));

	struct BMP slot =
		{
			.data  = malloc(WORDBITES_RAW_BOARD_PX_DIM_X * WORDBITES_RAW_BOARD_PX_DIM_Y * sizeof(struct BMPPixel)),
			.dim_x = WORDBITES_RAW_SLOT_PX_DIM,
			.dim_y = WORDBITES_RAW_SLOT_PX_DIM,
		};
	if (!slot.data)
	{
		error_abort("");
	}

	for (i32 slot_pos_y = 0; slot_pos_y < WORDBITES_BOARD_SLOTS_Y; slot_pos_y += 1)
	{
		for (i32 slot_pos_x = 0; slot_pos_x < WORDBITES_BOARD_SLOTS_X; slot_pos_x += 1)
		{
			for (i32 slot_px_y = 0; slot_px_y < slot.dim_y; slot_px_y += 1)
			{
				for (i32 slot_px_x = 0; slot_px_x < slot.dim_x; slot_px_x += 1)
				{
					slot.data[slot_px_y * slot.dim_x + slot_px_x] =
						bmp.data
						[
							(WORDBITES_RAW_BOARD_PX_POS_Y + slot_pos_y * WORDBITES_RAW_SLOT_PX_DIM + slot_px_y) * bmp.dim_x
								+ (WORDBITES_RAW_BOARD_PX_POS_X + slot_pos_x * WORDBITES_RAW_SLOT_PX_DIM + slot_px_x)
						];
				}
			}

			char file_path_buf[] = "W:/data/export_XX.bmp";
			file_path_buf[15] = (char) ('0' + slot_pos_x);
			file_path_buf[16] = (char) ('0' + slot_pos_y);
			str file_path = { file_path_buf, strlen(file_path_buf) };
			bmp_export(slot, file_path);
		}
	}


	//debug_halt();

	free(slot.data);
	bmp_free_read_file(&bmp);
	return 0;
}
