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

static void
create_dir(str dir_path)
{
	struct StrBuf formatted_dir_path = StrBuf(256);
	for (i32 i = 0; i < dir_path.length; i += 1)
	{
		if (dir_path.data[i] == '/')
		{
			strbuf_char(&formatted_dir_path, '\\');
		}
		else
		{
			strbuf_char(&formatted_dir_path, dir_path.data[i]);
		}
	}
	strbuf_char(&formatted_dir_path, '\\');
	strbuf_char(&formatted_dir_path, '\0');

	if (!MakeSureDirectoryPathExists(formatted_dir_path.data))
	{
		error("Failed create directory path \"%s\".", formatted_dir_path.data);
	}
}

static i32 // Amount of characters printed.
print_cli_field_pattern(str pattern)
{
	i32 result = 0;

	printf("%c", pattern.data[0] == '-' ? '(' : '[');
	result += 1;

	printf("%.*s", i32(pattern.length), pattern.data);
	result += i32(pattern.length);

	printf("%c", pattern.data[0] == '-' ? ')' : ']');
	result += 1;

	return result;
}

static void
print_cli_program_help(str exe_name, enum CLIProgram program)
{
	struct CLIProgramInfo program_info = CLI_PROGRAM_INFO[program];

	i64 margin_length = program_info.name.length;
	for (i32 field_index = {0}; field_index < program_info.field_count; field_index += 1)
	{
		margin_length = i64_max(margin_length, program_info.fields[field_index].pattern.length + CLI_FIELD_ADDITIONAL_MARGIN);
	}
	margin_length += 1;

	//
	// CLI program call structure.
	//

	printf
	(
		"%.*s %.*s",
		i32(exe_name.length), exe_name.data,
		i32(program_info.name.length), program_info.name.data
	);
	for (i32 field_index = {0}; field_index < program_info.field_count; field_index += 1)
	{
		printf(" ");
		print_cli_field_pattern(program_info.fields[field_index].pattern);
	}
	printf("\n");

	//
	// CLI program description.
	//

	printf
	(
		"\t%.*s%*s| %.*s\n",
		i32(program_info.name.length), program_info.name.data,
		i32(margin_length - program_info.name.length), "",
		i32(program_info.desc.length), program_info.desc.data
	);

	//
	// Description of each CLI field.
	//

	for (i32 field_index = {0}; field_index < program_info.field_count; field_index += 1)
	{
		struct CLIFieldInfo field_info = program_info.fields[field_index];

		printf("\t");
		i32 cli_field_length = print_cli_field_pattern(field_info.pattern);
		printf
		(
			"%*s| %.*s\n",
			i32(margin_length - cli_field_length), "",
			i32(field_info.desc.length), field_info.desc.data
		);
	}
}

int
main(int argc, char** argv)
{
	//
	// Determine CLI program.
	//

	enum CLIProgram cli_program = CLIProgram_COUNT;

	if (argc >= 2)
	{
		for (enum CLIProgram program = {0}; program < CLIProgram_COUNT; program += 1)
		{
			if (str_eq(CLI_PROGRAM_INFO[program].name, str_cstr(argv[1])))
			{
				cli_program = program;
				break;
			}
		}

		if (cli_program == CLIProgram_COUNT)
		{
			error("Unknown CLI program \"%s\".", argv[1]);
		}
	}

	//
	//
	//

	if (argc <= 2) // Provide CLI help if needed.
	{
		str exe_name = argc ? str_cstr(argv[0]) : CLI_EXE_NAME;

		if (argc <= 1)
		{
			i64 margin_length = exe_name.length;
			for (enum CLIProgram program = {0}; program < CLIProgram_COUNT; program += 1)
			{
				margin_length = i64_max(margin_length, CLI_PROGRAM_INFO[program].name.length);
			}
			margin_length += 1;

			//
			// CLI call structure.
			//

			printf("%.*s [program] ...", i32(exe_name.length), exe_name.data);
			printf("\n");

			//
			// Executable description.
			//

			printf
			(
				"\t%.*s%*s| %s\n",
				i32(exe_name.length), exe_name.data,
				i32(margin_length - exe_name.length), "",
				CLI_EXE_DESC
			);

			//
			// Description of each CLI program.
			//

			for (enum CLIProgram program = {0}; program < CLIProgram_COUNT; program += 1)
			{
				struct CLIProgramInfo program_info = CLI_PROGRAM_INFO[program];

				printf("\t");
				printf
				(
					"%.*s%*s| %.*s\n",
					i32(program_info.name.length), program_info.name.data,
					i32(margin_length - program_info.name.length), "",
					i32(program_info.desc.length), program_info.desc.data
				);
			}

			printf("\n");
		}

		if (cli_program == CLIProgram_COUNT)
		{
			for (enum CLIProgram program = {0}; program < CLIProgram_COUNT; program += 1)
			{
				print_cli_program_help(exe_name, program);
				printf("\n");
			}
		}
		else
		{
			print_cli_program_help(exe_name, cli_program);
			printf("\n");
		}
	}
	else // Move onto CLI program.
	{
		struct CLIProgramInfo cli_program_info = CLI_PROGRAM_INFO[cli_program];
		b32                   cli_field_filled[countof(CLI_PROGRAM_INFO[0].fields)] = {0};
		b32                   cli_parsed  = false;
		struct CLI            cli_unknown = {0};

		//
		// Iterate through each argument.
		//

		for
		(
			i32 arg_index = 2;
			arg_index < argc;
			arg_index += 1
		)
		{
			//
			// Determine the most appropriate CLI field and the argument's parameter.
			//

			i32   cli_field_index = -1;
			char* cli_param       = 0;

			for (i32 canidate_index = 0; canidate_index < cli_program_info.field_count; canidate_index += 1)
			{
				struct CLIFieldInfo canidate_info = cli_program_info.fields[canidate_index];

				if // Canidate field is optional with explicit parameter.
				(
					str_begins_with(canidate_info.pattern, str("-")) &&
					str_ends_with(canidate_info.pattern, str("="))
				)
				{
					if (str_begins_with(str_cstr(argv[arg_index]), canidate_info.pattern))
					{
						cli_field_index = canidate_index;
						cli_param       = argv[arg_index] + canidate_info.pattern.length;
						break;
					}
				}
				else if // Canidate field is an optional boolean switch.
				(
					str_begins_with(canidate_info.pattern, str("--"))
				)
				{
					if (str_eq(str_cstr(argv[arg_index]), canidate_info.pattern))
					{
						cli_field_index = canidate_index;
						break;
					}
				}
				else if // Otherwise, we consider this canidate field as required.
				(
					(!cli_field_filled[canidate_index] || str_ends_with(canidate_info.pattern, str("..."))) &&
					cli_field_index == -1
				)
				{
					cli_field_index = canidate_index;
					cli_param       = argv[arg_index];
				}
			}

			if (cli_field_index == -1)
			{
				error("Unknown argument \"%s\".\n", argv[arg_index]);
			}

			//
			// Parse parameter.
			//

			struct CLIFieldInfo cli_field_info = cli_program_info.fields[cli_field_index];
			u8*                 cli_member     = (((u8*) &cli_unknown) + cli_field_info.offset);

			switch (cli_field_info.typing)
			{
				case CLIFieldTyping_string:
				{
					((CLIFieldTyping_string_t*) cli_member)->str = str_cstr(cli_param);
				} break;

				case CLIFieldTyping_dary_string:
				{
					dary_push // Leak.
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

			cli_field_filled[cli_field_index] = true;
		}

		//
		// Verify all required fields are satisfied.
		//

		for (i32 cli_field_index = 0; cli_field_index < cli_program_info.field_count; cli_field_index += 1)
		{
			struct CLIFieldInfo cli_field_info = cli_program_info.fields[cli_field_index];
			if (cli_field_info.pattern.data[0] != '-' && !cli_field_filled[cli_field_index])
			{
				error
				(
					"Required argument [%.*s] not provided.\n",
					i32(cli_field_info.pattern.length), cli_field_info.pattern.data
				);
			}
		}

		//
		// Carry out CLI program.
		//

		switch (cli_program)
		{
			case CLIProgram_extractor:
			{
				struct CLIProgram_extractor_t cli = cli_unknown.extractor;

				//
				// Handle output directory.
				//

				create_dir(cli.output_dir_path.str); // Create the path if it doesn't already exist.

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
								.fFlags            = 0,
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
						error("Output directory \"%s\" is not empty. Use (--clear-output-dir) to empty the directory for processing.\n", cli.output_dir_path.cstr);
					}
				}

				create_dir(cli.output_dir_path.str); // Create the directory since emptying it will delete it too.

				//
				// Set up iterator to go through the screenshot directory.
				//

				struct StrBuf screenshot_wildcard_path = StrBuf(256);
				strbuf_str (&screenshot_wildcard_path, cli.screenshot_dir_path.str);
				strbuf_cstr(&screenshot_wildcard_path, "\\*");
				strbuf_char(&screenshot_wildcard_path, '\0');

				WIN32_FIND_DATAA screenshot_finder_data   = {0};
				HANDLE           screenshot_finder_handle = FindFirstFileA(screenshot_wildcard_path.data, &screenshot_finder_data);

				if (screenshot_finder_handle == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_NOT_FOUND)
				{
					error("`FindFirstFileA` failed on \"%s\".", screenshot_wildcard_path.data);
				}

				printf("Processing \"%s\".\n", screenshot_wildcard_path.data);

				i32 screenshots_processed        = 0;
				i32 slots_extracted              = 0;
				f64 overall_screenshot_avg_r     = 0.0;
				f64 overall_screenshot_avg_g     = 0.0;
				f64 overall_screenshot_avg_b     = 0.0;
				f64 overall_min_screenshot_avg_r = 0.0;
				f64 overall_min_screenshot_avg_g = 0.0;
				f64 overall_min_screenshot_avg_b = 0.0;
				f64 overall_max_screenshot_avg_r = 0.0;
				f64 overall_max_screenshot_avg_g = 0.0;
				f64 overall_max_screenshot_avg_b = 0.0;

				if (screenshot_finder_handle != INVALID_HANDLE_VALUE)
				{
					while (true)
					{
						//
						// Determine if image is a valid screenshot.
						//

						str input_file_name         = str_cstr(screenshot_finder_data.cFileName);
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
							!(screenshot_finder_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
							str_ends_with_caseless(input_file_name, str(".bmp"))
						)
						{
							struct StrBuf input_file_path = StrBuf(256);
							strbuf_str (&input_file_path, cli.screenshot_dir_path.str);
							strbuf_char(&input_file_path, '\\');
							strbuf_str (&input_file_path, input_file_name);

							struct BMP screenshot = bmp_alloc_read_file(input_file_path.str);

							if (screenshot.dim_x == PHONE_DIM_PX_X && screenshot.dim_y == PHONE_DIM_PX_Y)
							{
								//
								// Analyze RGB in screenshot.
								//

								f64 screenshot_avg_r = 0.0;
								f64 screenshot_avg_g = 0.0;
								f64 screenshot_avg_b = 0.0;

								for (i32 i = 0; i < screenshot.dim_x * screenshot.dim_y; i += 1)
								{
									screenshot_avg_r += screenshot.data[i].r;
									screenshot_avg_g += screenshot.data[i].g;
									screenshot_avg_b += screenshot.data[i].b;
								}
								overall_screenshot_avg_r += screenshot_avg_r;
								overall_screenshot_avg_g += screenshot_avg_g;
								overall_screenshot_avg_b += screenshot_avg_b;
								screenshot_avg_r         /= 256.0 * screenshot.dim_x * screenshot.dim_y;
								screenshot_avg_g         /= 256.0 * screenshot.dim_x * screenshot.dim_y;
								screenshot_avg_b         /= 256.0 * screenshot.dim_x * screenshot.dim_y;

								if (screenshots_processed)
								{
									overall_min_screenshot_avg_r = f64_min(overall_min_screenshot_avg_r, screenshot_avg_r);
									overall_min_screenshot_avg_g = f64_min(overall_min_screenshot_avg_g, screenshot_avg_g);
									overall_min_screenshot_avg_b = f64_min(overall_min_screenshot_avg_b, screenshot_avg_b);
									overall_max_screenshot_avg_r = f64_max(overall_max_screenshot_avg_r, screenshot_avg_r);
									overall_max_screenshot_avg_g = f64_max(overall_max_screenshot_avg_g, screenshot_avg_g);
									overall_max_screenshot_avg_b = f64_max(overall_max_screenshot_avg_b, screenshot_avg_b);
								}
								else
								{
									overall_min_screenshot_avg_r = screenshot_avg_r;
									overall_min_screenshot_avg_g = screenshot_avg_g;
									overall_min_screenshot_avg_b = screenshot_avg_b;
									overall_max_screenshot_avg_r = screenshot_avg_r;
									overall_max_screenshot_avg_g = screenshot_avg_g;
									overall_max_screenshot_avg_b = screenshot_avg_b;
								}

								//
								// Determine word game based on RGB.
								//

								enum WordGame wordgame = WordGame_COUNT;
								for (enum WordGame canidate_wordgame = {0}; canidate_wordgame < WordGame_COUNT; canidate_wordgame += 1)
								{
									if
									(
										f64_abs(screenshot_avg_r - WORDGAME_DT[canidate_wordgame].avg_r) <= AVG_RGB_MATCHING_EPSILON &&
										f64_abs(screenshot_avg_g - WORDGAME_DT[canidate_wordgame].avg_g) <= AVG_RGB_MATCHING_EPSILON &&
										f64_abs(screenshot_avg_b - WORDGAME_DT[canidate_wordgame].avg_b) <= AVG_RGB_MATCHING_EPSILON
									)
									{
										wordgame = canidate_wordgame;
										break;
									}
								}

								//
								// Make note of where the slots will be.
								//

								struct { i32 x; i32 y; } slot_buffer[128] = {0};
								i32                      slot_count       = 0;
								i32                      slot_origin_x    = 0;
								i32                      slot_origin_y    = 0;
								struct BMP               slot_bmp         = {0};

								switch (wordgame)
								{
									case WordGame_anagrams:
									{
										slot_bmp.dim_x = ANAGRAMS_6_SLOT_DIM;
										slot_bmp.dim_y = ANAGRAMS_6_SLOT_DIM;
										slot_origin_x  = ANAGRAMS_6_BOARD_POS_X;
										slot_origin_y  = ANAGRAMS_6_BOARD_POS_Y;

										for (i32 slot_y = 0; slot_y < ANAGRAMS_6_BOARD_SLOTS_Y; slot_y += 1)
										{
											for (i32 slot_x = 0; slot_x < ANAGRAMS_6_BOARD_SLOTS_X; slot_x += 1)
											{
												assert(slot_count < countof(slot_buffer));
												slot_buffer[slot_count].x = slot_x;
												slot_buffer[slot_count].y = slot_y;
												slot_count += 1;
											}
										}
									} break;

									case WordGame_wordhunt:
									{
										slot_bmp.dim_x = WORDHUNT_4x4_SLOT_DIM;
										slot_bmp.dim_y = WORDHUNT_4x4_SLOT_DIM;
										slot_origin_x  = WORDHUNT_4x4_BOARD_POS_X;
										slot_origin_y  = WORDHUNT_4x4_BOARD_POS_Y;

										for (i32 slot_y = 0; slot_y < WORDHUNT_4x4_BOARD_SLOTS_Y; slot_y += 1)
										{
											for (i32 slot_x = 0; slot_x < WORDHUNT_4x4_BOARD_SLOTS_X; slot_x += 1)
											{
												assert(slot_count < countof(slot_buffer));
												slot_buffer[slot_count].x = slot_x;
												slot_buffer[slot_count].y = slot_y;
												slot_count += 1;
											}
										}
									} break;

									case WordGame_wordbites:
									{
										slot_bmp.dim_x = WORDBITES_SLOT_DIM;
										slot_bmp.dim_y = WORDBITES_SLOT_DIM;
										slot_origin_x  = WORDBITES_BOARD_POS_X;
										slot_origin_y  = WORDBITES_BOARD_POS_Y;

										for (i32 slot_y = 0; slot_y < WORDBITES_BOARD_SLOTS_Y; slot_y += 1)
										{
											for (i32 slot_x = 0; slot_x < WORDBITES_BOARD_SLOTS_X; slot_x += 1)
											{
												assert(slot_count < countof(slot_buffer));
												slot_buffer[slot_count].x = slot_x;
												slot_buffer[slot_count].y = slot_y;
												slot_count += 1;
											}
										}
									} break;

									case WordGame_COUNT:
									{
										// Unknown game. No slots.
									} break;
								}

								//
								// Extract the slots.
								//

								alloc(&slot_bmp.data, slot_bmp.dim_x * slot_bmp.dim_y);

								for (i32 slot_index = 0; slot_index < slot_count; slot_index += 1)
								{
									assert(wordgame != WordGame_COUNT);
									assert(slot_origin_x + slot_buffer[slot_index].x * slot_bmp.dim_x <= screenshot.dim_x);
									assert(slot_origin_y + slot_buffer[slot_index].y * slot_bmp.dim_y <= screenshot.dim_y);

									//
									// Process pixels.
									//

									for (i32 slot_bmp_px_y = 0; slot_bmp_px_y < slot_bmp.dim_y; slot_bmp_px_y += 1)
									{
										for (i32 slot_bmp_px_x = 0; slot_bmp_px_x < slot_bmp.dim_x; slot_bmp_px_x += 1)
										{
											i32 screenshot_x = slot_origin_x + slot_buffer[slot_index].x * slot_bmp.dim_x + slot_bmp_px_x;
											i32 screenshot_y = slot_origin_y + slot_buffer[slot_index].y * slot_bmp.dim_y + slot_bmp_px_y;

											slot_bmp.data[slot_bmp_px_y * slot_bmp.dim_x + slot_bmp_px_x] =
												screenshot.data[screenshot_y * screenshot.dim_x + screenshot_x];
										}
									}

									struct StrBuf slot_bmp_file_path = StrBuf(256);
									strbuf_str (&slot_bmp_file_path, cli.output_dir_path.str);
									strbuf_char(&slot_bmp_file_path, '\\');
									strbuf_str (&slot_bmp_file_path, WORDGAME_DT[wordgame].print_name);
									strbuf_char(&slot_bmp_file_path, '_');
									strbuf_i64 (&slot_bmp_file_path, slot_buffer[slot_index].x);
									strbuf_char(&slot_bmp_file_path, '_');
									strbuf_i64 (&slot_bmp_file_path, slot_buffer[slot_index].y);
									strbuf_char(&slot_bmp_file_path, '_');
									strbuf_str (&slot_bmp_file_path, input_file_name_extless);
									strbuf_cstr(&slot_bmp_file_path, ".bmp");
									bmp_export(slot_bmp, slot_bmp_file_path.str);
								}

								free(slot_bmp.data);

								//
								// Finish processing the screenshot.
								//

								printf
								(
									"% 4d : %.*s : AvgRGB(%7.3f, %7.3f, %7.3f) : ",
									screenshots_processed + 1,
									i32(input_file_path.length), input_file_path.data,
									screenshot_avg_r * 256.0,
									screenshot_avg_g * 256.0,
									screenshot_avg_b * 256.0
								);
								if (wordgame == WordGame_COUNT)
								{
									printf("Unknown game.\n");
								}
								else
								{
									printf("%.*s.\n", i32(WORDGAME_DT[wordgame].print_name.length), WORDGAME_DT[wordgame].print_name.data);
								}

								screenshots_processed += 1;
							}

							free(screenshot.data);
						}

						//
						// Iterate to the next file.
						//

						if (!FindNextFileA(screenshot_finder_handle, &screenshot_finder_data))
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

					if (!FindClose(screenshot_finder_handle))
					{
						error("`FindClose` failed on \"%s\".", screenshot_wildcard_path.data);
					}
				}

				if (screenshots_processed)
				{
					overall_screenshot_avg_r /= 256.0 * screenshots_processed * PHONE_DIM_PX_X * PHONE_DIM_PX_Y;
					overall_screenshot_avg_g /= 256.0 * screenshots_processed * PHONE_DIM_PX_X * PHONE_DIM_PX_Y;
					overall_screenshot_avg_b /= 256.0 * screenshots_processed * PHONE_DIM_PX_X * PHONE_DIM_PX_Y;

					printf
					(
						"\n"
						"Processed %d screenshots.\n"
						"Extracted %d slots.\n"
						"\tOverall maximum RGB : (%7.3f, %7.3f, %7.3f).\n"
						"\tOverall average RGB : (%7.3f, %7.3f, %7.3f).\n"
						"\tOverall minimum RGB : (%7.3f, %7.3f, %7.3f).\n"
						"\tMinimum wiggle room : (%7.3f, %7.3f, %7.3f).\n",
						screenshots_processed,
						slots_extracted,
						overall_max_screenshot_avg_r * 256.0, overall_max_screenshot_avg_g * 256.0, overall_max_screenshot_avg_b * 256.0,
						overall_screenshot_avg_r     * 256.0, overall_screenshot_avg_g     * 256.0, overall_screenshot_avg_b     * 256.0,
						overall_min_screenshot_avg_r * 256.0, overall_min_screenshot_avg_g * 256.0, overall_min_screenshot_avg_b * 256.0,
						f64_max(f64_abs(overall_min_screenshot_avg_r - overall_screenshot_avg_r), f64_abs(overall_max_screenshot_avg_r - overall_screenshot_avg_r)) * 256.0,
						f64_max(f64_abs(overall_min_screenshot_avg_g - overall_screenshot_avg_g), f64_abs(overall_max_screenshot_avg_g - overall_screenshot_avg_g)) * 256.0,
						f64_max(f64_abs(overall_min_screenshot_avg_b - overall_screenshot_avg_b), f64_abs(overall_max_screenshot_avg_b - overall_screenshot_avg_b)) * 256.0
					);
				}
				else
				{
					printf("No screenshots found.\n");
				}
			} break;

			case CLIProgram_monochromize:
			{
				debug_halt();
			} break;

			case CLIProgram_heatmap:
			{
				debug_halt();
			} break;

			case CLIProgram_COUNT:
			default:
			{
				assert(false);
			} break;
		}
	}

	debug_halt();

	return 0;
}

//
// Documentation.
//

/* [Overview].
	TODO
*/
