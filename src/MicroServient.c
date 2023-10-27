#define _CRT_SECURE_NO_DEPRECATE 1
#include <Windows.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "defs.h"
#include "misc.c"
#include "str.c"
#include "strbuf.c"
#include "bmp.c"
#include "dary.c"

int
main(int argc, char** argv)
{
	b32 err = 0;

	//
	// Parse CLI arguments.
	//

	b32        cli_parsed = false;
	struct CLI cli        = {0};

	if (argc >= 2)
	{
		b32 cli_field_parsed[CLIField_COUNT] = {0};

		//
		// Parse each argument passed by the user.
		//

		for
		(
			i32 arg_index = 1;
			arg_index < argc;
			arg_index += 1
		)
		{
			for (enum CLIField cli_field = {0}; cli_field < CLIField_COUNT; cli_field += 1)
			{
				if (!cli_field_parsed[cli_field])
				{
					switch (CLI_FIELD_METADATA[cli_field].typing)
					{
						case CLIFieldTyping_string:
						{
							*(CLIFieldTyping_string_t*) (((u8*) &cli) + CLI_FIELD_METADATA[cli_field].offset) =
								(CLIFieldTyping_string_t)
								{
									.data   = argv[arg_index],
									.length = strlen(argv[arg_index]),
								};
						} break;
					}

					cli_field_parsed[cli_field] = true;
					break;
				}
			}
		}

		//
		// Verify all required arguments were fulfilled.
		//

		cli_parsed = true;
		for (enum CLIField cli_field = {0}; cli_field < CLIField_COUNT; cli_field += 1)
		{
			if (!cli_field_parsed[cli_field])
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

	//
	// Carry out CLI execution.
	//

	if (cli_parsed)
	{
		//
		// Set up JSON.
		//

		FILE* output_json_file = fopen(cli.output_json_file_path.cstr, "wb");
		if (!output_json_file)
		{
			error("`fopen` failed on \"%s\".", cli.output_json_file_path.cstr);
		}

		#define TRY_WRITE(STRLIT, ...) \
			do \
			{ \
				if (fprintf(output_json_file, (STRLIT),##__VA_ARGS__) < 0) \
				{ \
					error("`fwrite` failed."); \
				} \
			} \
			while (false)

		TRY_WRITE("{\n");

		//
		// Begin generating JSON data of average RGB values of the screenshots.
		//

		str input_dir_path = cli.input_wildcard_path.str;
		while (input_dir_path.length && (input_dir_path.data[input_dir_path.length - 1] != '/' && input_dir_path.data[input_dir_path.length - 1] != '\\'))
		{
			input_dir_path.length -= 1;
		}

		WIN32_FIND_DATAA finder_data   = {0};
		HANDLE           finder_handle = FindFirstFileA(cli.input_wildcard_path.cstr, &finder_data);

		if (finder_handle == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_NOT_FOUND)
		{
			error("`FindFirstFileA` failed on \"%s\".", cli.input_wildcard_path.cstr);
		}

		TRY_WRITE
		(
			"\t\"avg_screenshot_rgbs\":\n"
			"\t\t[\n"
		);
		if (finder_handle != INVALID_HANDLE_VALUE)
		{
			i32   processed_amount    = 0;
			f64_3 overall_avg_rgb     = {0};
			f64_3 overall_min_avg_rgb = {0};
			f64_3 overall_max_avg_rgb = {0};

			while (true)
			{
				//
				// Analyze image.
				//

				if
				(
					!(finder_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
					str_ends_with_caseless(str_cstr(finder_data.cFileName), str(".bmp"))
				)
				{
					struct StrBuf input_file_path = StrBuf(256);
					strbuf_str(&input_file_path, input_dir_path);
					strbuf_str(&input_file_path, str_cstr(finder_data.cFileName));

					struct BMP bmp = bmp_malloc_read_file(input_file_path.str);

					if (bmp.dim_x == PHONE_DIM_X && bmp.dim_y == PHONE_DIM_Y)
					{
						f64_3 avg_rgb = {0};
						for (i32 i = 0; i < bmp.dim_x * bmp.dim_y; i += 1)
						{
							avg_rgb.x += bmp.data[i].r;
							avg_rgb.y += bmp.data[i].g;
							avg_rgb.z += bmp.data[i].b;
						}
						overall_avg_rgb.x += avg_rgb.x;
						overall_avg_rgb.y += avg_rgb.y;
						overall_avg_rgb.z += avg_rgb.z;
						avg_rgb.x         /= 256.0 * bmp.dim_x * bmp.dim_y;
						avg_rgb.y         /= 256.0 * bmp.dim_x * bmp.dim_y;
						avg_rgb.z         /= 256.0 * bmp.dim_x * bmp.dim_y;

						if (processed_amount)
						{
							overall_min_avg_rgb.x = f64_min(overall_min_avg_rgb.x, avg_rgb.x);
							overall_min_avg_rgb.y = f64_min(overall_min_avg_rgb.y, avg_rgb.y);
							overall_min_avg_rgb.z = f64_min(overall_min_avg_rgb.z, avg_rgb.z);

							overall_max_avg_rgb.x = f64_max(overall_max_avg_rgb.x, avg_rgb.x);
							overall_max_avg_rgb.y = f64_max(overall_max_avg_rgb.y, avg_rgb.y);
							overall_max_avg_rgb.z = f64_max(overall_max_avg_rgb.z, avg_rgb.z);
						}
						else
						{
							overall_min_avg_rgb.x = avg_rgb.x;
							overall_min_avg_rgb.y = avg_rgb.y;
							overall_min_avg_rgb.z = avg_rgb.z;

							overall_max_avg_rgb.x = avg_rgb.x;
							overall_max_avg_rgb.y = avg_rgb.y;
							overall_max_avg_rgb.z = avg_rgb.z;
						}

						printf
						(
							"%.*s : AvgRGB(%.3f, %.3f, %.3f).\n",
							i32(input_file_path.length), input_file_path.data,
							avg_rgb.x * 256.0,
							avg_rgb.y * 256.0,
							avg_rgb.z * 256.0
						);

						if (processed_amount)
						{
							TRY_WRITE(",\n");
						}

						TRY_WRITE
						(
							"\t\t\t{ \"name\": \"%s\", \"avg_rgb\": { \"r\": %f, \"g\": %f, \"b\": %f } }",
							finder_data.cFileName,
							avg_rgb.x,
							avg_rgb.y,
							avg_rgb.z
						);

						processed_amount += 1;
					}

					free(bmp.data);
				}

				//
				// Iterate to the next file.
				//

				if (!FindNextFileA(finder_handle, &finder_data))
				{
					if (GetLastError() == ERROR_NO_MORE_FILES)
					{
						if (processed_amount)
						{
							TRY_WRITE("\n");
						}

						break;
					}
					else
					{
						error("`FindNextFileA` failed.");
					}
				}
			}

			if (processed_amount)
			{
				overall_avg_rgb.x /= 256.0 * processed_amount * PHONE_DIM_X * PHONE_DIM_Y;
				overall_avg_rgb.y /= 256.0 * processed_amount * PHONE_DIM_X * PHONE_DIM_Y;
				overall_avg_rgb.z /= 256.0 * processed_amount * PHONE_DIM_X * PHONE_DIM_Y;
				printf("\n");
			}

			printf("Processed %d images with \"%s\"; outputted to \"%s\".\n", processed_amount, cli.input_wildcard_path.cstr, cli.output_json_file_path.cstr);

			if (processed_amount)
			{
				printf
				(
					"Overall maximum RGB : (%7.3f, %7.3f, %7.3f).\n"
					"Overall average RGB : (%7.3f, %7.3f, %7.3f).\n"
					"Overall minimum RGB : (%7.3f, %7.3f, %7.3f).\n"
					"Minimum wiggle room : (%7.3f, %7.3f, %7.3f).\n",
					overall_max_avg_rgb.x * 256.0, overall_max_avg_rgb.y * 256.0, overall_max_avg_rgb.z * 256.0,
					overall_avg_rgb    .x * 256.0, overall_avg_rgb    .y * 256.0, overall_avg_rgb    .z * 256.0,
					overall_min_avg_rgb.x * 256.0, overall_min_avg_rgb.y * 256.0, overall_min_avg_rgb.z * 256.0,
					f64_max(f64_abs(overall_min_avg_rgb.x - overall_avg_rgb.x), f64_abs(overall_max_avg_rgb.x - overall_avg_rgb.x)) * 256.0,
					f64_max(f64_abs(overall_min_avg_rgb.y - overall_avg_rgb.y), f64_abs(overall_max_avg_rgb.y - overall_avg_rgb.y)) * 256.0,
					f64_max(f64_abs(overall_min_avg_rgb.z - overall_avg_rgb.z), f64_abs(overall_max_avg_rgb.z - overall_avg_rgb.z)) * 256.0
				);
			}
		}
		TRY_WRITE("\t\t]\n");

		if (!FindClose(finder_handle))
		{
			error("`FindClose` failed on \"%s\".", cli.input_wildcard_path.cstr);
		}

		//
		// Finish JSON.
		//

		TRY_WRITE("}\n");

		#undef TRY_WRITE

		if (fclose(output_json_file))
		{
			error("`fclose` failed on \"%s\".", cli.output_json_file_path.cstr);
		}
	}
	else // Failed to parse CLI arguments.
	{
		str exe_name = argc ? str_cstr(argv[0]) : CLI_EXE_NAME;

		i64 margin_length = exe_name.length;
		for (enum CLIField cli_field = {0}; cli_field < CLIField_COUNT; cli_field += 1)
		{
			margin_length = i64_max(margin_length, CLI_FIELD_METADATA[cli_field].name.length);
		}

		//
		// CLI call structure.
		//

		printf("%.*s", i32(exe_name.length), exe_name.data);
		for (enum CLIField cli_field = {0}; cli_field < CLIField_COUNT; cli_field += 1)
		{
			printf(" [%.*s]", i32(CLI_FIELD_METADATA[cli_field].name.length), CLI_FIELD_METADATA[cli_field].name.data);
		}
		printf("\n");

		//
		// Description of each CLI field.
		//

		printf
		(
			"\t%.*s%*s | %.*s\n",
			i32(exe_name.length), exe_name.data,
			i32(margin_length - exe_name.length), "",
			i32(CLI_EXE_DESC.length), CLI_EXE_DESC.data
		);
		for (enum CLIField cli_field = {0}; cli_field < CLIField_COUNT; cli_field += 1)
		{
			printf
			(
				"\t[%.*s]%*s | %.*s\n",
				i32(CLI_FIELD_METADATA[cli_field].name.length), CLI_FIELD_METADATA[cli_field].name.data,
				i32(margin_length - (CLI_FIELD_METADATA[cli_field].name.length + 2)), "",
				i32(CLI_FIELD_METADATA[cli_field].desc.length), CLI_FIELD_METADATA[cli_field].desc.data
			);
		}
	}

	debug_halt();

	return err;
}

//
// Documentation.
//

/* [Overview].
	TODO
*/
