#pragma warning(push)
#pragma warning(disable : 4255)
#define _CRT_SECURE_NO_DEPRECATE 1
#include <Windows.h>
#include <shlwapi.h>
#include <dbghelp.h>
#pragma warning(pop)
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

static void
create_dir(str base_path, str opt_dir_name)
{
	struct StrBuf dir_path = StrBuf(256);

	for (i32 i = 0; i < base_path.length; i += 1)
	{
		if (base_path.data[i] == '/')
		{
			strbuf_char(&dir_path, '\\');
		}
		else
		{
			strbuf_char(&dir_path, base_path.data[i]);
		}
	}

	strbuf_char(&dir_path, '\\');

	if (opt_dir_name.data)
	{
		strbuf_str (&dir_path, opt_dir_name);
		strbuf_char(&dir_path, '\\');
	}

	strbuf_char(&dir_path, '\0');

	if (!MakeSureDirectoryPathExists(dir_path.data))
	{
		error("Failed create directory path \"%s\".", dir_path.data);
	}
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
		// Iterate through each argument.
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
					str_begins_with(canidate_metadata.pattern, str("-")) &&
					str_ends_with(canidate_metadata.pattern, str("="))
				)
				{
					if (str_begins_with(str_cstr(argv[arg_index]), canidate_metadata.pattern))
					{
						cli_field = canidate_cli_field;
						cli_param = argv[arg_index] + canidate_metadata.pattern.length;
						break;
					}
				}
				else if // Canidate field is an optional boolean switch.
				(
					str_begins_with(canidate_metadata.pattern, str("--"))
				)
				{
					if (str_eq(str_cstr(argv[arg_index]), canidate_metadata.pattern))
					{
						cli_field = canidate_cli_field;
						break;
					}
				}
				else if // Otherwise, we consider this canidate field as required.
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

					case CLIFieldTyping_b32:
					{
						assert(!cli_param); // Boolean CLI fields right now are just simple switches.
						*((b32*) cli_member) = true;
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
					metadata.pattern.data[0] != '-' && // Field is required.
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
		// Handle output directory.
		//

		if (cli_field_filled[CLIField_output_dir_path])
		{
			create_dir(cli.output_dir_path.str, (str) {0}); // Create the path if it doesn't already exist.

			if (!PathIsDirectoryEmptyA(cli.output_dir_path.cstr)) // In the case that it already existed and it's nonempty.
			{
				if (cli.clear_output_dir)
				{
					struct StrBuf double_nullterminated_output_dir_path = StrBuf(256);
					strbuf_str (&double_nullterminated_output_dir_path, cli.output_dir_path.str);
					strbuf_char(&double_nullterminated_output_dir_path, '\0');
					strbuf_char(&double_nullterminated_output_dir_path, '\0');

					SHFILEOPSTRUCTA file_operation =
						{
							.wFunc             = FO_DELETE,
							.pFrom             = double_nullterminated_output_dir_path.data,
							.pTo               = "\0",
							.fFlags            = FOF_NOCONFIRMATION,
							.lpszProgressTitle = "",
						};

					printf("Clearing out \"%s\"...\n", cli.output_dir_path.cstr);
					if (SHFileOperationA(&file_operation) || file_operation.fAnyOperationsAborted)
					{
						error("Clearing output directory \"%s\" failed.", cli.output_dir_path.cstr);
					}
				}
				else
				{
					printf("Output directory \"%s\" is not empty. Use --clear-output-dir to empty the directory for processing.\n", cli.output_dir_path.cstr);
					err = true;
				}
			}

			if (!err)
			{
				create_dir(cli.output_dir_path.str, (str) {0}); // Recreate the directory again.
				for (char alphabet = 'A'; alphabet <= 'Z'; alphabet += 1)
				{
					create_dir(cli.output_dir_path.str, (str) { &alphabet, 1 });
				}
				create_dir(cli.output_dir_path.str, str("_"));
			}
		}

		//
		// Begin processing.
		//

		if (!err)
		{
			printf("Processing \"%s\".\n", cli.input_wildcard_path.cstr);

			//
			// Set up iterator to go through the directory.
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

			i32 processed_amount = 0;
			if (finder_handle != INVALID_HANDLE_VALUE)
			{
				f64 overall_input_bmp_avg_r     = 0.0;
				f64 overall_input_bmp_avg_g     = 0.0;
				f64 overall_input_bmp_avg_b     = 0.0;
				f64 overall_min_input_bmp_avg_r = 0.0;
				f64 overall_min_input_bmp_avg_g = 0.0;
				f64 overall_min_input_bmp_avg_b = 0.0;
				f64 overall_max_input_bmp_avg_r = 0.0;
				f64 overall_max_input_bmp_avg_g = 0.0;
				f64 overall_max_input_bmp_avg_b = 0.0;

				while (true)
				{
					//
					// Analyze image.
					//

					str input_file_name         = str_cstr(finder_data.cFileName);
					str input_file_name_extless = input_file_name;
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
							break;
						}
					}

					if
					(
						!(finder_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
						str_ends_with_caseless(input_file_name, str(".bmp"))
					)
					{
						struct StrBuf input_file_path = StrBuf(256);
						strbuf_str(&input_file_path, input_dir_path);
						strbuf_str(&input_file_path, input_file_name);

						struct BMP input_bmp = bmp_malloc_read_file(input_file_path.str);

						if (input_bmp.dim_x == PHONE_DIM_PX_X && input_bmp.dim_y == PHONE_DIM_PX_Y)
						{
							//
							// Compute average, min, and max RGB in image.
							//

							f64 input_bmp_avg_r = 0.0;
							f64 input_bmp_avg_g = 0.0;
							f64 input_bmp_avg_b = 0.0;

							for (i32 i = 0; i < input_bmp.dim_x * input_bmp.dim_y; i += 1)
							{
								input_bmp_avg_r += input_bmp.data[i].r;
								input_bmp_avg_g += input_bmp.data[i].g;
								input_bmp_avg_b += input_bmp.data[i].b;
							}
							overall_input_bmp_avg_r += input_bmp_avg_r;
							overall_input_bmp_avg_g += input_bmp_avg_g;
							overall_input_bmp_avg_b += input_bmp_avg_b;
							input_bmp_avg_r         /= 256.0 * input_bmp.dim_x * input_bmp.dim_y;
							input_bmp_avg_g         /= 256.0 * input_bmp.dim_x * input_bmp.dim_y;
							input_bmp_avg_b         /= 256.0 * input_bmp.dim_x * input_bmp.dim_y;

							if (processed_amount)
							{
								overall_min_input_bmp_avg_r = f64_min(overall_min_input_bmp_avg_r, input_bmp_avg_r);
								overall_min_input_bmp_avg_g = f64_min(overall_min_input_bmp_avg_g, input_bmp_avg_g);
								overall_min_input_bmp_avg_b = f64_min(overall_min_input_bmp_avg_b, input_bmp_avg_b);
								overall_max_input_bmp_avg_r = f64_max(overall_max_input_bmp_avg_r, input_bmp_avg_r);
								overall_max_input_bmp_avg_g = f64_max(overall_max_input_bmp_avg_g, input_bmp_avg_g);
								overall_max_input_bmp_avg_b = f64_max(overall_max_input_bmp_avg_b, input_bmp_avg_b);
							}
							else
							{
								overall_min_input_bmp_avg_r = input_bmp_avg_r;
								overall_min_input_bmp_avg_g = input_bmp_avg_g;
								overall_min_input_bmp_avg_b = input_bmp_avg_b;
								overall_max_input_bmp_avg_r = input_bmp_avg_r;
								overall_max_input_bmp_avg_g = input_bmp_avg_g;
								overall_max_input_bmp_avg_b = input_bmp_avg_b;
							}

							//
							// Determine word game based on RGB.
							//

							enum WordGame wordgame = WordGame_COUNT;
							for (enum WordGame canidate_wordgame = {0}; canidate_wordgame < WordGame_COUNT; canidate_wordgame += 1)
							{
								if
								(
									f64_abs(input_bmp_avg_r - WORDGAME_DT[canidate_wordgame].avg_r) <= AVG_RGB_EPSILON &&
									f64_abs(input_bmp_avg_g - WORDGAME_DT[canidate_wordgame].avg_g) <= AVG_RGB_EPSILON &&
									f64_abs(input_bmp_avg_b - WORDGAME_DT[canidate_wordgame].avg_b) <= AVG_RGB_EPSILON
								)
								{
									wordgame = canidate_wordgame;
									break;
								}
							}

							//
							// Make note of where slots for the slots will be.
							//

							struct Slot
							{
								i32 x;
								i32 y;
							};
							struct Slot slot_buffer[128] = {0};
							i32         slot_count       = 0;
							i32         slot_dim         = 0;
							i32         slot_origin_x    = 0;
							i32         slot_origin_y    = 0;

							switch (wordgame)
							{
								case WordGame_anagrams:
								{
								} break;

								case WordGame_wordhunt:
								{
								} break;

								case WordGame_wordbites:
								{
									slot_dim      = WORDBITES_SLOT_DIM;
									slot_origin_x = WORDBITES_BOARD_POS_X;
									slot_origin_y = WORDBITES_BOARD_POS_Y;

									for (i32 slot_y = 0; slot_y < WORDBITES_BOARD_SLOTS_Y; slot_y += 1)
									{
										for (i32 slot_x = 0; slot_x < WORDBITES_BOARD_SLOTS_X; slot_x += 1)
										{
											assert(slot_count < countof(slot_buffer));
											slot_buffer[slot_count] =
												(struct Slot)
												{
													.x = slot_x,
													.y = slot_y,
												};
											slot_count += 1;
										}
									}
								} break;

								case WordGame_COUNT:
								{
								} break;
							}

							//
							// Analyze each slot.
							//

							struct BMP slot_bmp =
								{
									.data  = malloc(slot_dim * slot_dim * sizeof(struct BMPPixel)),
									.dim_x = slot_dim,
									.dim_y = slot_dim,
								};
							if (!slot_bmp.data)
							{
								error("Failed to allocate memory for slot BMP.");
							}
							for (i32 slot_index = 0; slot_index < slot_count; slot_index += 1)
							{
								assert(slot_origin_x + slot_buffer[slot_index].x * slot_dim <= input_bmp.dim_x);
								assert(slot_origin_y + slot_buffer[slot_index].y * slot_dim <= input_bmp.dim_y);
								for (i32 y = 0; y < slot_dim; y += 1)
								{
									for (i32 x = 0; x < slot_dim; x += 1)
									{
										slot_bmp.data[y * slot_dim + x] =
											input_bmp.data
												[
													(slot_origin_y + slot_buffer[slot_index].y * slot_dim + y) * input_bmp.dim_x
														+ slot_origin_x + slot_buffer[slot_index].x * slot_dim + x
												];
									}
								}

								assert(wordgame != WordGame_COUNT);
								if (cli_field_filled[CLIField_output_dir_path])
								{
									struct StrBuf output_slot_file_path = StrBuf(256);
									strbuf_str (&output_slot_file_path, cli.output_dir_path.str);
									strbuf_char(&output_slot_file_path, '\\');
									strbuf_char(&output_slot_file_path, slot_index % 27 == 26 ? '_' : 'A' + slot_index % 27);
									strbuf_char(&output_slot_file_path, '\\');
									strbuf_str (&output_slot_file_path, WORDGAME_DT[wordgame].print_name);
									strbuf_char(&output_slot_file_path, '_');
									strbuf_str (&output_slot_file_path, input_file_name_extless);
									strbuf_char(&output_slot_file_path, '_');
									strbuf_i64 (&output_slot_file_path, slot_buffer[slot_index].x);
									strbuf_char(&output_slot_file_path, '_');
									strbuf_i64 (&output_slot_file_path, slot_buffer[slot_index].y);
									strbuf_cstr(&output_slot_file_path, ".bmp");
									bmp_export(slot_bmp, output_slot_file_path.str);
								}

								f64 slot_avg_r = 0.0;
								f64 slot_avg_g = 0.0;
								f64 slot_avg_b = 0.0;
								for (i32 i = 0; i < slot_bmp.dim_x * slot_bmp.dim_y; i += 1)
								{
									slot_avg_r += slot_bmp.data[i].r;
									slot_avg_g += slot_bmp.data[i].g;
									slot_avg_b += slot_bmp.data[i].b;
								}
								slot_avg_r /= 256.0 * slot_bmp.dim_x * slot_bmp.dim_y;
								slot_avg_g /= 256.0 * slot_bmp.dim_x * slot_bmp.dim_y;
								slot_avg_b /= 256.0 * slot_bmp.dim_x * slot_bmp.dim_y;
							}

							free(slot_bmp.data);

							//
							// Wrap up this image.
							//

							printf
							(
								"% 4d : %.*s : AvgRGB(%7.3f, %7.3f, %7.3f) : ",
								processed_amount + 1,
								i32(input_file_path.length), input_file_path.data,
								input_bmp_avg_r * 256.0,
								input_bmp_avg_g * 256.0,
								input_bmp_avg_b * 256.0
							);
							if (wordgame == WordGame_COUNT)
							{
								printf("Unknown game.\n");
							}
							else
							{
								printf("%.*s.\n", i32(WORDGAME_DT[wordgame].print_name.length), WORDGAME_DT[wordgame].print_name.data);
							}

							processed_amount += 1;
						}

						free(input_bmp.data);
					}

					//
					// Iterate to the next file.
					//

					if (!FindNextFileA(finder_handle, &finder_data))
					{
						if (GetLastError() == ERROR_NO_MORE_FILES)
						{
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
					overall_input_bmp_avg_r /= 256.0 * processed_amount * PHONE_DIM_PX_X * PHONE_DIM_PX_Y;
					overall_input_bmp_avg_g /= 256.0 * processed_amount * PHONE_DIM_PX_X * PHONE_DIM_PX_Y;
					overall_input_bmp_avg_b /= 256.0 * processed_amount * PHONE_DIM_PX_X * PHONE_DIM_PX_Y;
				}

				if (processed_amount)
				{
					printf
					(
						"\tOverall maximum RGB : (%7.3f, %7.3f, %7.3f).\n"
						"\tOverall average RGB : (%7.3f, %7.3f, %7.3f).\n"
						"\tOverall minimum RGB : (%7.3f, %7.3f, %7.3f).\n"
						"\tMinimum wiggle room : (%7.3f, %7.3f, %7.3f).\n",
						overall_max_input_bmp_avg_r * 256.0, overall_max_input_bmp_avg_g * 256.0, overall_max_input_bmp_avg_b * 256.0,
						overall_input_bmp_avg_r     * 256.0, overall_input_bmp_avg_g     * 256.0, overall_input_bmp_avg_b     * 256.0,
						overall_min_input_bmp_avg_r * 256.0, overall_min_input_bmp_avg_g * 256.0, overall_min_input_bmp_avg_b * 256.0,
						f64_max(f64_abs(overall_min_input_bmp_avg_r - overall_input_bmp_avg_r), f64_abs(overall_max_input_bmp_avg_r - overall_input_bmp_avg_r)) * 256.0,
						f64_max(f64_abs(overall_min_input_bmp_avg_g - overall_input_bmp_avg_g), f64_abs(overall_max_input_bmp_avg_g - overall_input_bmp_avg_g)) * 256.0,
						f64_max(f64_abs(overall_min_input_bmp_avg_b - overall_input_bmp_avg_b), f64_abs(overall_max_input_bmp_avg_b - overall_input_bmp_avg_b)) * 256.0
					);
				}

				if (!FindClose(finder_handle))
				{
					error("`FindClose` failed on \"%s\".", cli.input_wildcard_path.cstr);
				}
			}
			printf("Finished processing %d images.\n", processed_amount);
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
