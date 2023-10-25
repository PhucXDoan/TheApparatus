#define _CRT_SECURE_NO_DEPRECATE 1
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "defs.h"
#include "misc.c"
#include "Microservient_bmp.c"


int
main(int argc, char** argv)
{
	b32        cli_parsed = false;
	b32        err        = 0;
	struct CLI cli        = {0};

	if (argc >= 2)
	{
		b32 cli_field_filled[CLIField_COUNT] = {0};

		for
		(
			i32 arg_index = 1;
			arg_index < argc;
			arg_index += 1
		)
		{
			for (enum CLIField cli_field = {0}; cli_field < CLIField_COUNT; cli_field += 1)
			{
				if (!cli_field_filled[cli_field])
				{
					switch (CLI_FIELD_METADATA[cli_field].typing)
					{
						case CLIFieldTyping_str:
						{
							*(str*) (((u8*) &cli) + CLI_FIELD_METADATA[cli_field].offset) =
								(str)
								{
									.data   = argv[arg_index],
									.length = strlen(argv[arg_index]),
								};
						} break;

						default:
						{
							assert(false);
						} break;
					}

					cli_field_filled[cli_field] = true;
					break;
				}
			}
		}

		cli_parsed = true;
		for (enum CLIField cli_field = {0}; cli_field < CLIField_COUNT; cli_field += 1)
		{
			if (!cli_field_filled[cli_field])
			{
				cli_parsed = false;
				err        = true;

				printf
				(
					"Required argument [%.*s] not provided.\n",
					i32(CLI_FIELD_METADATA[cli_field].name.length), CLI_FIELD_METADATA[cli_field].name.data
				);

				break;
			}
		}
	}

	if (cli_parsed)
	{
		struct BMP bmp = bmp_malloc_read_file(cli.input_file_path);

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

		free(slot.data);
		bmp_free_read_file(&bmp);
	}
	else
	{
		printf("%s", argc ? argv[0] : CLI_EXE_NAME_STRLIT);
		for (enum CLIField cli_field = {0}; cli_field < CLIField_COUNT; cli_field += 1)
		{
			printf(" [%.*s]", i32(CLI_FIELD_METADATA[cli_field].name.length), CLI_FIELD_METADATA[cli_field].name.data);
		}
		printf("\n");
		for (enum CLIField cli_field = {0}; cli_field < CLIField_COUNT; cli_field += 1)
		{
			printf
			(
				"\t[%.*s]%*s%.*s\n",
				i32(CLI_FIELD_METADATA[cli_field].name.length), CLI_FIELD_METADATA[cli_field].name.data,
				CLI_LONGEST_NAME_LENGTH - i32(CLI_FIELD_METADATA[cli_field].name.length) + 1, "",
				i32(CLI_FIELD_METADATA[cli_field].desc.length), CLI_FIELD_METADATA[cli_field].desc.data
			);
		}
	}

	debug_halt();

	return err;
}
