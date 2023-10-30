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

	printf("%c", CLI_FIELD_INFO[cli_field].pattern.data[0] == '-' ? '(' : '[');
	result += 1;

	printf("%.*s", i32(CLI_FIELD_INFO[cli_field].pattern.length), CLI_FIELD_INFO[cli_field].pattern.data);
	result += i32(CLI_FIELD_INFO[cli_field].pattern.length);

	printf("%c", CLI_FIELD_INFO[cli_field].pattern.data[0] == '-' ? ')' : ']');
	result += 1;

	return result;
}

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

int
main(int argc, char** argv)
{
	//
	// Parse CLI arguments.
	//

	b32        cli_parsed                       = false;
	b32        cli_field_filled[CLIField_COUNT] = {0};
	struct CLI cli                              = {0};

	if (argc <= 1) // CLI help.
	{
		str exe_name = argc ? str_cstr(argv[0]) : CLI_EXE_NAME;

		i64 margin_length = exe_name.length;
		for (enum CLIField cli_field = {0}; cli_field < CLIField_COUNT; cli_field += 1)
		{
			margin_length = i64_max(margin_length, CLI_FIELD_INFO[cli_field].pattern.length + CLI_ARG_ADDITIONAL_MARGIN);
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
				i32(CLI_FIELD_INFO[cli_field].desc.length), CLI_FIELD_INFO[cli_field].desc.data
			);
		}
	}
	else // CLI program.
	{
		//
		// Iterate through each argument.
		//

		for (i32 arg_index = 1; arg_index < argc; arg_index += 1)
		{
			//
			// Determine the most appropriate CLI field and the argument's parameter.
			//

			enum CLIField cli_field = CLIField_COUNT;
			char*         cli_param = 0;

			for (enum CLIField canidate_cli_field = {0}; canidate_cli_field < CLIField_COUNT; canidate_cli_field += 1)
			{
				struct CLIFieldMetaData canidate_metadata = CLI_FIELD_INFO[canidate_cli_field];

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

			if (cli_field == CLIField_COUNT)
			{
				error("Unknown argument \"%s\".\n", argv[arg_index]);
			}

			//
			// Parse parameter.
			//

			u8* cli_member = (((u8*) &cli) + CLI_FIELD_INFO[cli_field].offset);

			switch (CLI_FIELD_INFO[cli_field].typing)
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

			cli_field_filled[cli_field] = true;
		}

		//
		// Verify all required fields are satisfied.
		//

		for (enum CLIField cli_field = {0}; cli_field < CLIField_COUNT; cli_field += 1)
		{
			if (CLI_FIELD_INFO[cli_field].pattern.data[0] != '-' && !cli_field_filled[cli_field])
			{
				error
				(
					"Required argument [%.*s] not provided.\n",
					i32(CLI_FIELD_INFO[cli_field].pattern.length), CLI_FIELD_INFO[cli_field].pattern.data
				);
			}
		}

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

		//
		// Generate subfolders.
		//

		for (enum Letter letter = {0}; letter < Letter_COUNT; letter += 1)
		{
			struct StrBuf letter_dir_path = StrBuf(256);
			strbuf_str (&letter_dir_path, cli.output_dir_path.str);
			strbuf_char(&letter_dir_path, '\\');
			strbuf_str (&letter_dir_path, LETTER_NAMES[letter]);
			strbuf_char(&letter_dir_path, '\\');
			create_dir(letter_dir_path.str);
		}

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

		i32 processed_amount = 0;
		if (screenshot_finder_handle != INVALID_HANDLE_VALUE)
		{
			f64 overall_screenshot_avg_r     = 0.0;
			f64 overall_screenshot_avg_g     = 0.0;
			f64 overall_screenshot_avg_b     = 0.0;
			f64 overall_min_screenshot_avg_r = 0.0;
			f64 overall_min_screenshot_avg_g = 0.0;
			f64 overall_min_screenshot_avg_b = 0.0;
			f64 overall_max_screenshot_avg_r = 0.0;
			f64 overall_max_screenshot_avg_g = 0.0;
			f64 overall_max_screenshot_avg_b = 0.0;

			while (true)
			{
				//
				// Analyze image.
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

						if (processed_amount)
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
						i32                      slot_dim         = 0;
						i32                      slot_origin_x    = 0;
						i32                      slot_origin_y    = 0;

						switch (wordgame)
						{
							case WordGame_anagrams:
							{
								slot_dim      = ANAGRAMS_6_SLOT_DIM;
								slot_origin_x = ANAGRAMS_6_BOARD_POS_X;
								slot_origin_y = ANAGRAMS_6_BOARD_POS_Y;

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
								slot_dim      = WORDHUNT_4x4_SLOT_DIM;
								slot_origin_x = WORDHUNT_4x4_BOARD_POS_X;
								slot_origin_y = WORDHUNT_4x4_BOARD_POS_Y;

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
								slot_dim      = WORDBITES_SLOT_DIM;
								slot_origin_x = WORDBITES_BOARD_POS_X;
								slot_origin_y = WORDBITES_BOARD_POS_Y;

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
						// Convert slots into reduced slots.
						//

						b8* max_reduced_slot = 0;
						alloc(&max_reduced_slot, REDUCED_SLOT_MAX_DIM * REDUCED_SLOT_MAX_DIM);

						for (i32 slot_index = 0; slot_index < slot_count; slot_index += 1)
						{
							assert(wordgame != WordGame_COUNT);
							assert(slot_origin_x + slot_buffer[slot_index].x * slot_dim <= screenshot.dim_x);
							assert(slot_origin_y + slot_buffer[slot_index].y * slot_dim <= screenshot.dim_y);

							//
							// Process pixels.
							//

							i32 rows_of_interest_beginning_index = 0;
							i32 rows_of_interest_ending_index    = 0;

							for (i32 max_reduced_slot_px_y = 0; max_reduced_slot_px_y < REDUCED_SLOT_MAX_DIM; max_reduced_slot_px_y += 1)
							{
								for (i32 max_reduced_slot_px_x = 0; max_reduced_slot_px_x < REDUCED_SLOT_MAX_DIM; max_reduced_slot_px_x += 1)
								{
									i32             screenshot_x     = slot_origin_x + slot_buffer[slot_index].x * slot_dim + i32(f64(max_reduced_slot_px_x) / f64(REDUCED_SLOT_MAX_DIM) * f64(slot_dim));
									i32             screenshot_y     = slot_origin_y + slot_buffer[slot_index].y * slot_dim + i32(f64(max_reduced_slot_px_y) / f64(REDUCED_SLOT_MAX_DIM) * f64(slot_dim));
									struct BMPPixel slot_pixel       = screenshot.data[screenshot_y * screenshot.dim_x + screenshot_x];

									if ((slot_pixel.r + slot_pixel.g + slot_pixel.b) / 3 <= MONOCHROMIC_THRESHOLD)
									{
										max_reduced_slot[max_reduced_slot_px_y * REDUCED_SLOT_MAX_DIM + max_reduced_slot_px_x] = true;

										if (!rows_of_interest_ending_index)
										{
											rows_of_interest_beginning_index = max_reduced_slot_px_y;
										}
										rows_of_interest_ending_index = max_reduced_slot_px_y + 1;
									}
									else
									{
										max_reduced_slot[max_reduced_slot_px_y * REDUCED_SLOT_MAX_DIM + max_reduced_slot_px_x] = false;
									}
								}
							}

//							//
//							// Determine closest matching letter.
//							//
//
//							enum Letter best_exam_letter = Letter_COUNT;
//							i32         best_exam_score  = 0;
//							for (enum Letter letter = {0}; letter < Letter_COUNT; letter += 1)
//							{
//								i32 exam_score = 0;
//
//								for (i32 y = 0; y < EXAM_DIM; y += 1)
//								{
//									for (i32 x = 0; x < EXAM_DIM; x += 1)
//									{
//										if (bmp_monochrome_get(compressed_monochrome_slot_bmp, x, y) == (exams[letter].data[y * exams[letter].dim_x + x].r & 1))
//										{
//											exam_score += 1;
//										}
//									}
//								}
//
//								if (best_exam_score < exam_score)
//								{
//									best_exam_letter = letter;
//									best_exam_score  = exam_score;
//								}
//							}

							//
							// Export BMPs.
							//

							struct BMPMonochrome reduced_slot =
								{
									.dim_x = REDUCED_SLOT_MAX_DIM,
									.dim_y = rows_of_interest_ending_index - rows_of_interest_beginning_index,
								};
							alloc(&reduced_slot.data, bmp_monochrome_calc_size(reduced_slot.dim_x, reduced_slot.dim_y));

							for (i32 reduced_slot_px_y = 0; reduced_slot_px_y < reduced_slot.dim_y; reduced_slot_px_y += 1)
							{
								for (i32 reduced_slot_px_x = 0; reduced_slot_px_x < reduced_slot.dim_x; reduced_slot_px_x += 1)
								{
									bmp_monochrome_set
									(
										reduced_slot,
										reduced_slot_px_x,
										reduced_slot_px_y,
										max_reduced_slot
										[
											(rows_of_interest_beginning_index + reduced_slot_px_y) * REDUCED_SLOT_MAX_DIM
												+ reduced_slot_px_x
										]
									);
								}
							}

//							assert(best_exam_letter != Letter_COUNT);
							struct StrBuf reduced_slot_file_path = StrBuf(256);
							strbuf_str (&reduced_slot_file_path, cli.output_dir_path.str);
							strbuf_char(&reduced_slot_file_path, '\\');
							strbuf_str (&reduced_slot_file_path, LETTER_NAMES[Letter_A]); // TEMP
							strbuf_char(&reduced_slot_file_path, '_');
							strbuf_str (&reduced_slot_file_path, WORDGAME_DT[wordgame].print_name);
							strbuf_char(&reduced_slot_file_path, '_');
							strbuf_i64 (&reduced_slot_file_path, slot_buffer[slot_index].x);
							strbuf_char(&reduced_slot_file_path, '_');
							strbuf_i64 (&reduced_slot_file_path, slot_buffer[slot_index].y);
							strbuf_char(&reduced_slot_file_path, '_');
							strbuf_str (&reduced_slot_file_path, input_file_name_extless);
							strbuf_cstr(&reduced_slot_file_path, ".bmp");
							bmp_monochrome_export(reduced_slot, reduced_slot_file_path.str);

							free(reduced_slot.data);
						}

						free(max_reduced_slot);

						//
						// Finish processing the screenshot.
						//

						printf
						(
							"% 4d : %.*s : AvgRGB(%7.3f, %7.3f, %7.3f) : ",
							processed_amount + 1,
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

						processed_amount += 1;
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

			printf("Finished processing %d images.\n", processed_amount);

			if (processed_amount)
			{
				overall_screenshot_avg_r /= 256.0 * processed_amount * PHONE_DIM_PX_X * PHONE_DIM_PX_Y;
				overall_screenshot_avg_g /= 256.0 * processed_amount * PHONE_DIM_PX_X * PHONE_DIM_PX_Y;
				overall_screenshot_avg_b /= 256.0 * processed_amount * PHONE_DIM_PX_X * PHONE_DIM_PX_Y;

				printf
				(
					"\n"
					"\tOverall maximum RGB : (%7.3f, %7.3f, %7.3f).\n"
					"\tOverall average RGB : (%7.3f, %7.3f, %7.3f).\n"
					"\tOverall minimum RGB : (%7.3f, %7.3f, %7.3f).\n"
					"\tMinimum wiggle room : (%7.3f, %7.3f, %7.3f).\n",
					overall_max_screenshot_avg_r * 256.0, overall_max_screenshot_avg_g * 256.0, overall_max_screenshot_avg_b * 256.0,
					overall_screenshot_avg_r     * 256.0, overall_screenshot_avg_g     * 256.0, overall_screenshot_avg_b     * 256.0,
					overall_min_screenshot_avg_r * 256.0, overall_min_screenshot_avg_g * 256.0, overall_min_screenshot_avg_b * 256.0,
					f64_max(f64_abs(overall_min_screenshot_avg_r - overall_screenshot_avg_r), f64_abs(overall_max_screenshot_avg_r - overall_screenshot_avg_r)) * 256.0,
					f64_max(f64_abs(overall_min_screenshot_avg_g - overall_screenshot_avg_g), f64_abs(overall_max_screenshot_avg_g - overall_screenshot_avg_g)) * 256.0,
					f64_max(f64_abs(overall_min_screenshot_avg_b - overall_screenshot_avg_b), f64_abs(overall_max_screenshot_avg_b - overall_screenshot_avg_b)) * 256.0
				);

//				printf
//				(
//					"\n"
//					"Verify all compressed images are intact and correct.\n"
//				);
//
//				b32 generate_next_gen_exams = false;
//				while (true)
//				{
//					printf("Continue onto generating next generation exams? (Y/N) : ");
//					char input_buffer[4] = {0};
//					if (!fgets(input_buffer, countof(input_buffer), stdin))
//					{
//						error("Failed to get input.");
//					}
//
//					if (!strcmp(input_buffer, "Y\n") || !strcmp(input_buffer, "y\n"))
//					{
//						generate_next_gen_exams = true;
//						break;
//					}
//					else if (!strcmp(input_buffer, "N\n") || !strcmp(input_buffer, "n\n"))
//					{
//						break;
//					}
//					else
//					{
//						while (!strchr(input_buffer, '\n'))
//						{
//							if (!fgets(input_buffer, countof(input_buffer), stdin))
//							{
//								error("Failed to get input.");
//							}
//						}
//					}
//				}
//
//				if (generate_next_gen_exams)
//				{
//					i32* heatmap = {0};
//					alloc(&heatmap, EXAM_DIM * EXAM_DIM);
//
//					struct BMPMonochrome nextgen_exam =
//						{
//							.dim_x = EXAM_DIM,
//							.dim_y = EXAM_DIM,
//						};
//					alloc(&nextgen_exam.data, nextgen_exam.dim_x * nextgen_exam.dim_y);
//
//					for (enum Letter letter = {0}; letter < Letter_COUNT; letter += 1)
//					{
//						struct StrBuf compressed_monochrome_bmp_dir_path = StrBuf(256);
//						strbuf_str (&compressed_monochrome_bmp_dir_path, cli.output_dir_path.str);
//						strbuf_char(&compressed_monochrome_bmp_dir_path, '\\');
//						strbuf_str (&compressed_monochrome_bmp_dir_path, LETTER_NAMES[letter]);
//						strbuf_cstr(&compressed_monochrome_bmp_dir_path, "\\compressed\\");
//
//						struct StrBuf compressed_monochrome_bmp_wildcard_path = StrBuf(256);
//						strbuf_str (&compressed_monochrome_bmp_wildcard_path, compressed_monochrome_bmp_dir_path.str);
//						strbuf_cstr(&compressed_monochrome_bmp_wildcard_path, "*.bmp");
//						strbuf_char(&compressed_monochrome_bmp_wildcard_path, '\0');
//
//						WIN32_FIND_DATAA compressed_monochrome_bmp_finder_data   = {0};
//						HANDLE           compressed_monochrome_bmp_finder_handle = FindFirstFileA(compressed_monochrome_bmp_wildcard_path.data, &compressed_monochrome_bmp_finder_data);
//
//						if (compressed_monochrome_bmp_finder_handle == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_NOT_FOUND)
//						{
//							error("`FindFirstFileA` failed on \"%s\".", compressed_monochrome_bmp_wildcard_path.data);
//						}
//
//						if (compressed_monochrome_bmp_finder_handle != INVALID_HANDLE_VALUE)
//						{
//							printf("Generating for \"%.*s\".\n", i32(compressed_monochrome_bmp_wildcard_path.length), compressed_monochrome_bmp_wildcard_path.data);
//							memset(heatmap, 0, sizeof(i32) * EXAM_DIM * EXAM_DIM);
//							i32 subexam_count = 0;
//
//							while (true)
//							{
//								str input_file_name         = str_cstr(compressed_monochrome_bmp_finder_data.cFileName);
//								str input_file_name_extless = input_file_name;
//								for
//								(
//									i64 i = input_file_name_extless.length - 1;
//									i >= 0;
//									i -= 1
//								)
//								{
//									if (input_file_name_extless.data[i] == '.')
//									{
//										input_file_name_extless.length = i;
//										break;
//									}
//								}
//
//								if (!(compressed_monochrome_bmp_finder_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
//								{
//									struct StrBuf scratch_path = compressed_monochrome_bmp_dir_path;
//									strbuf_str(&scratch_path, input_file_name);
//									printf("\t\"%.*s\"\n", i32(scratch_path.length), scratch_path.data);
//
//									struct BMP compressed_monochrome_bmp = bmp_alloc_read_file(scratch_path.str);
//
//									if (compressed_monochrome_bmp.dim_x != EXAM_DIM || compressed_monochrome_bmp.dim_y != EXAM_DIM)
//									{
//										error
//										(
//											"Subexam \"%s\" is %dx%d, expected %dx%d.",
//											scratch_path.data,
//											compressed_monochrome_bmp.dim_x, compressed_monochrome_bmp.dim_y,
//											EXAM_DIM, EXAM_DIM
//										);
//									}
//
//									for (i32 i = 0; i < compressed_monochrome_bmp.dim_x * compressed_monochrome_bmp.dim_y; i += 1)
//									{
//										struct BMPPixel pixel = compressed_monochrome_bmp.data[i];
//										if (pixel.r == pixel.g && pixel.g == pixel.b && (pixel.b == 0 || pixel.b == 255) && pixel.a == 255)
//										{
//											heatmap[i] += !!pixel.r;
//										}
//										else
//										{
//											error("Subexam \"%s\" is not monochromatic.", scratch_path.data);
//										}
//									}
//
//									free(compressed_monochrome_bmp.data);
//									subexam_count += 1;
//								}
//
//								if (!FindNextFileA(compressed_monochrome_bmp_finder_handle, &compressed_monochrome_bmp_finder_data))
//								{
//									if (GetLastError() == ERROR_NO_MORE_FILES)
//									{
//										break;
//									}
//									else
//									{
//										error("`FindNextFileA` failed.");
//									}
//								}
//							}
//
//							if (subexam_count)
//							{
//								memset(nextgen_exam.data, 0, bmp_monochrome_calc_size(EXAM_DIM, EXAM_DIM));
//								for (i32 y = 0; y < EXAM_DIM; y += 1)
//								{
//									for (i32 x = 0; x < EXAM_DIM; x += 1)
//									{
//										bmp_monochrome_set(nextgen_exam, x, y, heatmap[y * EXAM_DIM + x] > subexam_count / 8);
//									}
//								}
//
//								struct StrBuf nextgen_exam_file_path = StrBuf(256);
//								strbuf_str (&nextgen_exam_file_path, cli.output_dir_path.str);
//								strbuf_cstr(&nextgen_exam_file_path, "\\nextgen_exams\\");
//								strbuf_str (&nextgen_exam_file_path, LETTER_NAMES[letter]);
//								strbuf_cstr(&nextgen_exam_file_path, ".bmp");
//								bmp_monochrome_export(nextgen_exam, nextgen_exam_file_path.str);
//							}
//							else
//							{
//								// TODO ??
//							}
//
//							if (!FindClose(compressed_monochrome_bmp_finder_handle))
//							{
//								error("`FindClose` failed on \"%s\".", compressed_monochrome_bmp_wildcard_path.data);
//							}
//						}
//					}
//
//					free(nextgen_exam.data);
//					free(heatmap);
//				}
			}
		}
		else
		{
			printf("No images were processed.\n");
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
