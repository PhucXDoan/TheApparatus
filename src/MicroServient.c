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

static i32 // Amount of characters printed.
print_cli_field(enum CLIField cli_field)
{
	i32 result = 0;

	printf("%c", CLI_FIELD_METADATA[cli_field].pattern.data[0] == '-' ? '(' : '[');
	result += 1;

	printf("%.*s", i32(CLI_FIELD_METADATA[cli_field].pattern.length), CLI_FIELD_METADATA[cli_field].pattern.data);
	result += i32(CLI_FIELD_METADATA[cli_field].pattern.length);

	printf("%c", CLI_FIELD_METADATA[cli_field].pattern.data[0] == '-' ? ')' : ']');
	result += 1;

	return result;
}

int
main(int argc, char** argv)
{
	b32 err = false;

	//
	// Parse CLI arguments.
	//

	b32        cli_parsed                       = false;
	b32        cli_field_filled[CLIField_COUNT] = {0};
	struct CLI cli                              = {0};

	if (argc >= 2)
	{
		//
		// Parse arguments.
		//

		i32 arg_index = 1;
		while (arg_index < argc && !err)
		{
			//
			// Determine the most appropriate CLI field and the argument's parameter.
			//

			enum CLIField cli_field = CLIField_COUNT;
			char*         cli_param = 0;

			for (enum CLIField canidate_cli_field = {0}; canidate_cli_field < CLIField_COUNT; canidate_cli_field += 1)
			{
				struct CLIFieldMetaData canidate_metadata = CLI_FIELD_METADATA[canidate_cli_field];

				if // Canidate field is optional with explicit parameter.
				(
					canidate_metadata.pattern.data[0] == '-' &&
					canidate_metadata.pattern.data[canidate_metadata.pattern.length - 1] == '='
				)
				{
					if (str_begins_with(str_cstr(argv[arg_index]), canidate_metadata.pattern))
					{
						cli_field = canidate_cli_field;
						cli_param = argv[arg_index] + canidate_metadata.pattern.length;
						break;
					}
				}
				else if // Canidate field that is required.
				(
					(!cli_field_filled[canidate_cli_field] || str_ends_with(canidate_metadata.pattern, str("..."))) &&
					cli_field == CLIField_COUNT
				)
				{
					cli_field = canidate_cli_field;
					cli_param = argv[arg_index];
				}
			}

			//
			// Parse parameter.
			//

			if (cli_field == CLIField_COUNT)
			{
				printf("Unknown argument \"%s\".\n", argv[arg_index]);
				err = true;
			}
			else
			{
				u8* cli_member = (((u8*) &cli) + CLI_FIELD_METADATA[cli_field].offset);

				switch (CLI_FIELD_METADATA[cli_field].typing)
				{
					case CLIFieldTyping_string:
					{
						((CLIFieldTyping_string_t*) cli_member)->str = str_cstr(cli_param);
					} break;

					case CLIFieldTyping_dary_string:
					{
						dary_push // Buffer is purposely leaked; no need to clean it up really.
						(
							(CLIFieldTyping_dary_string_t*) cli_member,
							(CLIFieldTyping_string_t[]) { { .str = str_cstr(cli_param) } }
						);
					} break;
				}

				cli_field_filled[cli_field] = true;

				arg_index += 1;
			}
		}

		//
		// Verify fields.
		//

		if (!err)
		{
			for (enum CLIField cli_field = {0}; cli_field < CLIField_COUNT; cli_field += 1)
			{
				struct CLIFieldMetaData metadata = CLI_FIELD_METADATA[cli_field];
				if
				(
					!(metadata.pattern.data[0] == '-') && // Field is not optional.
					!cli_field_filled[cli_field]
				)
				{
					printf("Required argument ");
					print_cli_field(cli_field);
					printf(" not provided.\n");
					err = true;
					break;
				}
			}
		}

		if (!err)
		{
			cli_parsed = true;
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

		FILE* output_json_file = 0;

		if (cli_field_filled[CLIField_output_json_file_path])
		{
			output_json_file = fopen(cli.output_json_file_path.cstr, "wb");
			if (!output_json_file)
			{
				error("`fopen` failed on \"%s\".", cli.output_json_file_path.cstr);
			}
		}

		#define OUTPUT_WRITE(STRLIT, ...) \
			do \
			{ \
				if (output_json_file && fprintf(output_json_file, (STRLIT),##__VA_ARGS__) < 0) \
				{ \
					error("`fwrite` failed."); \
				} \
			} \
			while (false)

		OUTPUT_WRITE("{\n");

		//
		// Begin outputting data of average RGB values of the screenshots.
		//

		OUTPUT_WRITE
		(
			"\t\"avg_screenshot_rgb_analysis\":\n"
			"\t\t[\n"
		);

		for (i32 input_wildcard_path_index = 0; input_wildcard_path_index < cli.input_wildcard_paths.length; input_wildcard_path_index += 1)
		{
			CLIFieldTyping_string_t input_wildcard_path = cli.input_wildcard_paths.data[input_wildcard_path_index];

			if (input_wildcard_path_index)
			{
				printf("\n");
			}

			printf("[%d/%lld] Processing \"%s\".\n", input_wildcard_path_index + 1, cli.input_wildcard_paths.length, input_wildcard_path.cstr);

			OUTPUT_WRITE
			(
				"\t\t\t{\n"
				"\t\t\t\t\"wildcard_path\": \""
			);
			for (i32 i = 0; i < input_wildcard_path.length; i += 1)
			{
				if (input_wildcard_path.data[i] == '\\')
				{
					OUTPUT_WRITE("\\\\");
				}
				else
				{
					OUTPUT_WRITE("%c", input_wildcard_path.data[i]);
				}
			}
			OUTPUT_WRITE
			(
				"\",\n"
				"\t\t\t\t\"samples\":\n"
				"\t\t\t\t\t[\n"
			);

			//
			// Set up iterator to go through the directory.
			//

			str input_dir_path = input_wildcard_path.str;
			while (input_dir_path.length && (input_dir_path.data[input_dir_path.length - 1] != '/' && input_dir_path.data[input_dir_path.length - 1] != '\\'))
			{
				input_dir_path.length -= 1;
			}

			WIN32_FIND_DATAA finder_data   = {0};
			HANDLE           finder_handle = FindFirstFileA(input_wildcard_path.cstr, &finder_data);

			if (finder_handle == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_NOT_FOUND)
			{
				error("`FindFirstFileA` failed on \"%s\".", input_wildcard_path.cstr);
			}

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
								"% 4d : %.*s : AvgRGB(%.3f, %.3f, %.3f).\n",
								processed_amount + 1,
								i32(input_file_path.length), input_file_path.data,
								avg_rgb.x * 256.0,
								avg_rgb.y * 256.0,
								avg_rgb.z * 256.0
							);

							if (processed_amount)
							{
								OUTPUT_WRITE(",\n");
							}

							OUTPUT_WRITE
							(
								"\t\t\t\t\t\t{ \"name\": \"%s\", \"avg_rgb\": { \"r\": %f, \"g\": %f, \"b\": %f } }",
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
								OUTPUT_WRITE
								(
									"\n"
									"\t\t\t\t\t]\n"
									"\t\t\t}"
								);

								if (input_wildcard_path_index != cli.input_wildcard_paths.length - 1)
								{
									if (processed_amount)
									{
										OUTPUT_WRITE(",");
									}
								}

								OUTPUT_WRITE("\n");
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
				}

				if (processed_amount)
				{
					printf
					(
						"\tOverall maximum RGB : (%7.3f, %7.3f, %7.3f).\n"
						"\tOverall average RGB : (%7.3f, %7.3f, %7.3f).\n"
						"\tOverall minimum RGB : (%7.3f, %7.3f, %7.3f).\n"
						"\tMinimum wiggle room : (%7.3f, %7.3f, %7.3f).\n",
						overall_max_avg_rgb.x * 256.0, overall_max_avg_rgb.y * 256.0, overall_max_avg_rgb.z * 256.0,
						overall_avg_rgb    .x * 256.0, overall_avg_rgb    .y * 256.0, overall_avg_rgb    .z * 256.0,
						overall_min_avg_rgb.x * 256.0, overall_min_avg_rgb.y * 256.0, overall_min_avg_rgb.z * 256.0,
						f64_max(f64_abs(overall_min_avg_rgb.x - overall_avg_rgb.x), f64_abs(overall_max_avg_rgb.x - overall_avg_rgb.x)) * 256.0,
						f64_max(f64_abs(overall_min_avg_rgb.y - overall_avg_rgb.y), f64_abs(overall_max_avg_rgb.y - overall_avg_rgb.y)) * 256.0,
						f64_max(f64_abs(overall_min_avg_rgb.z - overall_avg_rgb.z), f64_abs(overall_max_avg_rgb.z - overall_avg_rgb.z)) * 256.0
					);
				}
			}

			if (!FindClose(finder_handle))
			{
				error("`FindClose` failed on \"%s\".", input_wildcard_path.cstr);
			}
		}

		OUTPUT_WRITE("\t\t]\n");

		//
		// Finish JSON.
		//

		OUTPUT_WRITE("}\n");

		#undef OUTPUT_WRITE

		if (output_json_file && fclose(output_json_file))
		{
			error("`fclose` failed on \"%s\".", cli.output_json_file_path.cstr);
		}
	}
	else // CLI fields were not fully parsed.
	{
		if (err)
		{
			printf("\n");
		}

		str exe_name = argc ? str_cstr(argv[0]) : CLI_EXE_NAME;

		i64 margin_length = exe_name.length;
		for (enum CLIField cli_field = {0}; cli_field < CLIField_COUNT; cli_field += 1)
		{
			margin_length = i64_max(margin_length, CLI_FIELD_METADATA[cli_field].pattern.length + CLI_ARG_ADDITIONAL_MARGIN);
		}

		//
		// CLI call structure.
		//

		printf("%.*s", i32(exe_name.length), exe_name.data);
		for (enum CLIField cli_field = {0}; cli_field < CLIField_COUNT; cli_field += 1)
		{
			printf(" ");
			print_cli_field(cli_field);
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
			printf("\t");
			i32 cli_field_length = print_cli_field(cli_field);
			printf
			(
				"%*s | %.*s\n",
				i32(margin_length - cli_field_length), "",
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
