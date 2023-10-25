#define _CRT_SECURE_NO_DEPRECATE 1
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "defs.h"
#include "misc.c"
#include "MicroServient_bmp.c"
#include "MicroServient_strbuf.c"

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
		str input_file_name_extless = cli.input_file_path;
		for
		(
			i64 i = input_file_name_extless.length - 1;
			i >= 0;
			i -= 1
		)
		{
			if (input_file_name_extless.data[i] == '.')
			{
				input_file_name_extless.length = i;
			}
			else if (input_file_name_extless.data[i] == '/' || input_file_name_extless.data[i] == '\\')
			{
				input_file_name_extless.length -= i + 1;
				input_file_name_extless.data   += i + 1;
				break;
			}
		}

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

				struct StrBuf output_file_path = StrBuf(256);
				strbuf_str (&output_file_path, cli.output_dir_path);
				strbuf_char(&output_file_path, '/');
				strbuf_str (&output_file_path, input_file_name_extless);
				strbuf_char(&output_file_path, '_');
				strbuf_u64 (&output_file_path, slot_pos_x);
				strbuf_char(&output_file_path, '_');
				strbuf_u64 (&output_file_path, slot_pos_y);
				strbuf_cstr(&output_file_path, ".bmp");
				printf("Exporting: \"%.*s\"\n", i32(output_file_path.length), output_file_path.data);
				bmp_export(slot, output_file_path.str);
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
