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

static b32 // Directory is empty.
create_dir(str dir_path, b32 delete_content)
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

	b32 empty = PathIsDirectoryEmptyA(formatted_dir_path.data);
	if (!empty && delete_content)
	{
		struct StrBuf double_nullterminated_output_dir_path = StrBuf(256);
		strbuf_str (&double_nullterminated_output_dir_path, dir_path);
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

		if (SHFileOperationA(&file_operation) || file_operation.fAnyOperationsAborted)
		{
			error("Clearing directory \"%s\" failed.", formatted_dir_path.data);
		}

		if (!MakeSureDirectoryPathExists(formatted_dir_path.data)) // Create the directory since emptying it will delete it too.
		{
			error("Failed create directory path \"%s\".", formatted_dir_path.data);
		}
	}

	return empty;
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

struct DirBMPIteratorState
{
	str              dir_path;
	HANDLE           finder_handle;
	WIN32_FIND_DATAA finder_data;
	b32              inited;
};
static b32 // Found BMP.
iterate_dir_bmp_alloc(struct BMP* dst_bmp, struct DirBMPIteratorState* state)
{
	b32 found_bmp = false;
	*dst_bmp  = (struct BMP) {0};

	if (!state->inited)
	{
		struct StrBuf wildcard_path = StrBuf(256);
		strbuf_str (&wildcard_path, state->dir_path);
		strbuf_cstr(&wildcard_path, "\\*");
		strbuf_char(&wildcard_path, '\0');

		state->finder_handle = FindFirstFileA(wildcard_path.data, &state->finder_data);
		if (state->finder_handle == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_NOT_FOUND)
		{
			error("`FindFirstFileA` failed on \"%s\".", wildcard_path.data);
		}
	}

	if (state->finder_handle != INVALID_HANDLE_VALUE)
	{
		while (true)
		{
			if (state->inited)
			{
				if (!FindNextFileA(state->finder_handle, &state->finder_data))
				{
					if (GetLastError() != ERROR_NO_MORE_FILES)
					{
						error("`FindNextFileA` failed.");
					}

					if (!FindClose(state->finder_handle))
					{
						error("`FindClose` failed.");
					}

					break;
				}
			}
			else
			{
				state->inited = true;
			}

			if
			(
				!(state->finder_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
				str_ends_with_caseless(str_cstr(state->finder_data.cFileName), str(".bmp"))
			)
			{
				struct StrBuf file_path = StrBuf(256);
				strbuf_str (&file_path, state->dir_path);
				strbuf_char(&file_path, '\\');
				strbuf_cstr(&file_path, state->finder_data.cFileName);

				*dst_bmp  = bmp_alloc_read_file(file_path.str);

				found_bmp = true;
				break;
			}
		}
	}

	return found_bmp;
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
						cli_param       = 0;
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

				if (!create_dir(cli.output_dir_path.str, cli.clear_output_dir) && !cli.clear_output_dir)
				{
					error("Output directory \"%s\" is not empty. Use (--clear-output-dir) to empty the directory for processing.\n", cli.output_dir_path.cstr);
				}

				printf("Processing \"%s\".\n", cli.input_dir_path.cstr);

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

				struct DirBMPIteratorState iterator_state =
					{
						.dir_path = cli.input_dir_path.str
					};
				struct BMP screenshot = {0};
				while (iterate_dir_bmp_alloc(&screenshot, &iterator_state))
				{
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
							strbuf_cstr(&slot_bmp_file_path, iterator_state.finder_data.cFileName);
							bmp_export(slot_bmp, slot_bmp_file_path.str);
						}

						free(slot_bmp.data);

						slots_extracted += slot_count;

						//
						// Finish processing the screenshot.
						//

						printf
						(
							"% 4d : \"%s\" : AvgRGB(%7.3f, %7.3f, %7.3f) : ",
							screenshots_processed + 1,
							iterator_state.finder_data.cFileName,
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

				if (screenshots_processed)
				{
					overall_screenshot_avg_r /= 256.0 * screenshots_processed * PHONE_DIM_PX_X * PHONE_DIM_PX_Y;
					overall_screenshot_avg_g /= 256.0 * screenshots_processed * PHONE_DIM_PX_X * PHONE_DIM_PX_Y;
					overall_screenshot_avg_b /= 256.0 * screenshots_processed * PHONE_DIM_PX_X * PHONE_DIM_PX_Y;

					printf
					(
						"\n"
						"Examined %d screenshots.\n"
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
				struct CLIProgram_monochromize_t cli = cli_unknown.monochromize;

				if (!create_dir(cli.output_dir_path.str, cli.clear_output_dir) && !cli.clear_output_dir)
				{
					error("Output directory \"%s\" is not empty. Use (--clear-output-dir) to empty the directory for processing.\n", cli.output_dir_path.cstr);
				}

				i32 images_processed = 0;

				struct DirBMPIteratorState iterator_state =
					{
						.dir_path = cli.input_dir_path.str
					};
				struct BMP bmp = {0};
				while (iterate_dir_bmp_alloc(&bmp, &iterator_state))
				{
					for (i32 i = 0; i < bmp.dim_x * bmp.dim_y; i += 1)
					{
						if ((bmp.data[i].r + bmp.data[i].g + bmp.data[i].b) / 3 <= MONOCHROMIZE_THRESHOLD)
						{
							bmp.data[i] = (struct BMPPixel) { .r = 255, .g = 255, .b = 255, .a = 255 };
						}
						else
						{
							bmp.data[i] = (struct BMPPixel) { .r = 0, .g = 0, .b = 0, .a = 255 };
						}
					}

					struct StrBuf file_path = StrBuf(256);
					strbuf_str (&file_path, cli.output_dir_path.str);
					strbuf_char(&file_path, '\\');
					strbuf_cstr(&file_path, iterator_state.finder_data.cFileName);
					bmp_export(bmp, file_path.str);

					printf("% 4d : \"%s\".\n", images_processed + 1, iterator_state.finder_data.cFileName);

					images_processed += 1;

					free(bmp.data);
				}

				if (images_processed)
				{
					printf
					(
						"\n"
						"Monochromized %d images.\n",
						images_processed
					);
				}
				else
				{
					printf("No images were monochromized.\n");
				}
			} break;

			case CLIProgram_meltingpot:
			{
				struct CLIProgram_meltingpot_t cli = cli_unknown.meltingpot;

				i32        images_processed = 0;
				i32        images_skipped   = 0;
				struct BMP meltingpot       = {0};

				struct { i32 r; i32 g; i32 b; i32 a; }* weights = 0;

				struct DirBMPIteratorState iterator_state =
					{
						.dir_path = cli.input_dir_path.str
					};
				struct BMP sample_bmp = {0};
				while (iterate_dir_bmp_alloc(&sample_bmp, &iterator_state))
				{
					if (!weights)
					{
						alloc(&weights, sample_bmp.dim_x * sample_bmp.dim_y);

						meltingpot.dim_x = sample_bmp.dim_x;
						meltingpot.dim_y = sample_bmp.dim_y;
						alloc(&meltingpot.data, meltingpot.dim_x * meltingpot.dim_y);

						printf
						(
							"Using \"%s\" for bounding dimensions %dx%d.\n",
							iterator_state.finder_data.cFileName,
							meltingpot.dim_x,
							meltingpot.dim_y
						);
					}

					if (sample_bmp.dim_x == meltingpot.dim_x && sample_bmp.dim_y == meltingpot.dim_y)
					{
						if (cli.or_filter)
						{
							for (i32 i = 0; i < sample_bmp.dim_x * sample_bmp.dim_y; i += 1)
							{
								meltingpot.data[i].r |= sample_bmp.data[i].r;
								meltingpot.data[i].g |= sample_bmp.data[i].g;
								meltingpot.data[i].b |= sample_bmp.data[i].b;
								meltingpot.data[i].a |= sample_bmp.data[i].a;
							}
						}
						else
						{
							for (i32 i = 0; i < sample_bmp.dim_x * sample_bmp.dim_y; i += 1)
							{
								weights[i].r += sample_bmp.data[i].r;
								weights[i].g += sample_bmp.data[i].g;
								weights[i].b += sample_bmp.data[i].b;
								weights[i].a += sample_bmp.data[i].a;
							}
						}

						printf("% 4d : \"%s\".\n", images_processed + 1, iterator_state.finder_data.cFileName);
						images_processed += 1;
					}
					else
					{
						printf("\tSkipping \"%s\".\n", iterator_state.finder_data.cFileName);
						images_skipped += 1;
					}

					free(sample_bmp.data);
				}

				if (images_processed)
				{
					if (!cli.or_filter)
					{
						for (i32 i = 0; i < meltingpot.dim_x * meltingpot.dim_y; i += 1)
						{
							meltingpot.data[i] =
								(struct BMPPixel)
								{
									.r = u8(weights[i].r / images_processed),
									.g = u8(weights[i].g / images_processed),
									.b = u8(weights[i].b / images_processed),
									.a = u8(weights[i].a / images_processed),
								};
						}
					}

					bmp_export(meltingpot, cli.output_file_path.str);

					printf
					(
						"\n"
						"Meltingpotted %d images.\n",
						images_processed
					);
					if (images_skipped)
					{
						printf
						(
							"Skipped %d images.\n",
							images_skipped
						);
					}
				}
				else
				{
					printf("No BMPs found.\n");
				}
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
