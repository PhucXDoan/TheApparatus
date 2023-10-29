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

static i32
u8_count_ones(u8 value)
{
	i32 result = 0;
	for (i32 i = 0; i < bitsof(value); i += 1)
	{
		result += (value >> i) & 1;
	}
	return result;
}

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

static void
create_output_slot_dir(str dir_path, str slot_name)
{
	struct StrBuf formatted_path = StrBuf(256);
	strbuf_str (&formatted_path, dir_path);
	strbuf_char(&formatted_path, '\\');
	strbuf_str (&formatted_path, slot_name);
	strbuf_char(&formatted_path, '\\');
	{
		struct StrBuf scratch_path = formatted_path;
		strbuf_cstr(&scratch_path, "raw\\");
		create_dir(scratch_path.str);
	}
	{
		struct StrBuf scratch_path = formatted_path;
		strbuf_cstr(&scratch_path, "monochrome\\");
		create_dir(scratch_path.str);
	}
	{
		struct StrBuf scratch_path = formatted_path;
		strbuf_cstr(&scratch_path, "compressed\\");
		create_dir(scratch_path.str);
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
		// Load in solution set.
		//

		struct IdentificationEntry
		{
			struct BMPMonochrome      monochrome_bmp;
			struct Dary_BMPMonochrome matches;
		} exam_entries[Letter_COUNT] = {0};

		for (enum Letter letter = {0}; letter < Letter_COUNT; letter += 1)
		{
			struct StrBuf exam_file_path = StrBuf(256);
			strbuf_str (&exam_file_path, cli.solution_dir_path.str);
			strbuf_char(&exam_file_path, '\\');
			strbuf_str (&exam_file_path, LETTER_DT[letter]);
			strbuf_cstr(&exam_file_path, ".bmp");
			strbuf_char(&exam_file_path, '\0');

			struct BMP exam_bmp = bmp_malloc_read_file(exam_file_path.str);

			if (exam_bmp.dim_x != COMPRESSED_MONOCHROME_DIM || exam_bmp.dim_y != COMPRESSED_MONOCHROME_DIM)
			{
				error
				(
					"Exam is of wrong dimensions (expected %dx%d, but \"%s\" is %dx%d\").",
					COMPRESSED_MONOCHROME_DIM, COMPRESSED_MONOCHROME_DIM,
					exam_file_path.data,
					exam_bmp.dim_x, exam_bmp.dim_y
				);
			}

			exam_entries[letter].monochrome_bmp =
				(struct BMPMonochrome)
				{
					.data  = calloc(calc_bmp_monochrome_size(COMPRESSED_MONOCHROME_DIM, COMPRESSED_MONOCHROME_DIM), sizeof(u8)),
					.dim_x = COMPRESSED_MONOCHROME_DIM,
					.dim_y = COMPRESSED_MONOCHROME_DIM,
				};
			if (!exam_entries[letter].monochrome_bmp.data)
			{
				error("Failed to allocate BMP.");
			}

			for (i32 y = 0; y < exam_bmp.dim_y; y += 1)
			{
				for (i32 x = 0; x < exam_bmp.dim_x; x += 1)
				{
					bmp_monochrome_set
					(
						&exam_entries[letter].monochrome_bmp,
						x,
						y,
						exam_bmp.data[y * exam_bmp.dim_x + x].r
					);
				}
			}

			free(exam_bmp.data);

			if (err)
			{
				break;
			}
		}

		if (!err)
		{
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
				create_dir(cli.output_dir_path.str); // Recreate the directory again.

				for (enum Letter letter = {0}; letter < Letter_COUNT; letter += 1)
				{
					create_output_slot_dir(cli.output_dir_path.str, LETTER_DT[letter]);
				}
			}

			//
			// Begin processing.
			//

			if (!err)
			{
				//
				// Set up iterator to go through the directory.
				//

				struct StrBuf wildcard_training_dir_path = StrBuf(256);
				strbuf_str (&wildcard_training_dir_path, cli.training_dir_path.str);
				strbuf_cstr(&wildcard_training_dir_path, "\\*");
				strbuf_char(&wildcard_training_dir_path, '\0');

				WIN32_FIND_DATAA finder_data   = {0};
				HANDLE           finder_handle = FindFirstFileA(wildcard_training_dir_path.data, &finder_data);

				if (finder_handle == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_NOT_FOUND)
				{
					error("`FindFirstFileA` failed on \"%s\".", wildcard_training_dir_path.data);
				}

				printf("Processing \"%s\".\n", wildcard_training_dir_path.data);

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
							strbuf_str (&input_file_path, cli.training_dir_path.str);
							strbuf_char(&input_file_path, '\\');
							strbuf_str (&input_file_path, input_file_name);

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
										slot_dim      = ANAGRAMS_6_SLOT_DIM;
										slot_origin_x = ANAGRAMS_6_BOARD_POS_X;
										slot_origin_y = ANAGRAMS_6_BOARD_POS_Y;

										for (i32 slot_y = 0; slot_y < ANAGRAMS_6_BOARD_SLOTS_Y; slot_y += 1)
										{
											for (i32 slot_x = 0; slot_x < ANAGRAMS_6_BOARD_SLOTS_X; slot_x += 1)
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
								// Parse each slot.
								//

								struct BMP raw_slot_bmp =
									{
										.data  = malloc(slot_dim * slot_dim * sizeof(*raw_slot_bmp.data)),
										.dim_x = slot_dim,
										.dim_y = slot_dim,
									};
								if (!raw_slot_bmp.data)
								{
									error("Failed to allocate memory for raw slot BMP.");
								}

								struct BMPMonochrome monochrome_slot_bmp =
									{
										.data  = calloc(calc_bmp_monochrome_size(slot_dim, slot_dim), sizeof(u8)),
										.dim_x = slot_dim,
										.dim_y = slot_dim,
									};
								if (!monochrome_slot_bmp.data)
								{
									error("Failed to allocate memory for monochrome slot BMP.");
								}

								struct BMPMonochrome compressed_monochrome_slot_bmp =
									{
										.data  = calloc(calc_bmp_monochrome_size(COMPRESSED_MONOCHROME_DIM, COMPRESSED_MONOCHROME_DIM), sizeof(u8)),
										.dim_x = COMPRESSED_MONOCHROME_DIM,
										.dim_y = COMPRESSED_MONOCHROME_DIM,
									};
								if (!compressed_monochrome_slot_bmp.data)
								{
									error("Failed to allocate memory for compressed monochrome slot BMP.");
								}

								for (i32 slot_index = 0; slot_index < slot_count; slot_index += 1)
								{
									assert(wordgame != WordGame_COUNT);
									assert(slot_origin_x + slot_buffer[slot_index].x * slot_dim <= input_bmp.dim_x);
									assert(slot_origin_y + slot_buffer[slot_index].y * slot_dim <= input_bmp.dim_y);
									for (i32 y = 0; y < slot_dim; y += 1)
									{
										for (i32 x = 0; x < slot_dim; x += 1)
										{
											struct BMPPixel pixel =
												input_bmp.data
													[
														(slot_origin_y + slot_buffer[slot_index].y * slot_dim + y) * input_bmp.dim_x
															+ slot_origin_x + slot_buffer[slot_index].x * slot_dim + x
													];

											raw_slot_bmp.data[y * slot_dim + x] = pixel;

											bmp_monochrome_set(&monochrome_slot_bmp, x, y, (pixel.r + pixel.g + pixel.b) / 3.0 < BLACK_THRESHOLD);
										}
									}

									for (i32 y = 0; y < COMPRESSED_MONOCHROME_DIM; y += 1)
									{
										for (i32 x = 0; x < COMPRESSED_MONOCHROME_DIM; x += 1)
										{
											bmp_monochrome_set
											(
												&compressed_monochrome_slot_bmp,
												x, y,
												bmp_monochrome_get
												(
													monochrome_slot_bmp,
													i32(f64(x) / f64(COMPRESSED_MONOCHROME_DIM) * f64(monochrome_slot_bmp.dim_x)),
													i32(f64(y) / f64(COMPRESSED_MONOCHROME_DIM) * f64(monochrome_slot_bmp.dim_y))
												)
											);
										}
									}

									enum Letter best_exam_letter = Letter_COUNT;
									i32         best_exam_score  = -1;
									for (enum Letter letter = {0}; letter < Letter_COUNT; letter += 1)
									{
										i32 exam_score = 0;
										for (i32 byte_index = 0; byte_index < calc_bmp_monochrome_size(COMPRESSED_MONOCHROME_DIM, COMPRESSED_MONOCHROME_DIM); byte_index += 1)
										{
											exam_score += u8_count_ones(~((exam_entries[letter].monochrome_bmp.data[byte_index] ^ compressed_monochrome_slot_bmp.data[byte_index])));
										}
										if (best_exam_score < exam_score)
										{
											best_exam_letter = letter;
											best_exam_score  = exam_score;
										}
									}

									struct StrBuf output_file_name = StrBuf(256);
									strbuf_str (&output_file_name, WORDGAME_DT[wordgame].print_name);
									strbuf_char(&output_file_name, '_');
									strbuf_str (&output_file_name, input_file_name_extless);
									strbuf_char(&output_file_name, '_');
									strbuf_i64 (&output_file_name, slot_buffer[slot_index].x);
									strbuf_char(&output_file_name, '_');
									strbuf_i64 (&output_file_name, slot_buffer[slot_index].y);
									strbuf_cstr(&output_file_name, ".bmp");

									assert(best_exam_letter != Letter_COUNT);
									struct StrBuf slot_dir_path = StrBuf(256);
									strbuf_str (&slot_dir_path, cli.output_dir_path.str);
									strbuf_char(&slot_dir_path, '\\');
									strbuf_str (&slot_dir_path, LETTER_DT[best_exam_letter]);
									strbuf_char(&slot_dir_path, '\\');
									{
										struct StrBuf scratch_path = slot_dir_path;
										strbuf_cstr(&scratch_path, "raw\\");
										strbuf_str (&scratch_path, output_file_name.str);
										bmp_export(raw_slot_bmp, scratch_path.str);
									}
									{
										struct StrBuf scratch_path = slot_dir_path;
										strbuf_cstr(&scratch_path, "monochrome\\");
										strbuf_str (&scratch_path, output_file_name.str);
										bmp_monochrome_export(monochrome_slot_bmp, scratch_path.str);
									}
									{
										struct StrBuf scratch_path = slot_dir_path;
										strbuf_cstr(&scratch_path, "compressed\\");
										strbuf_str (&scratch_path, output_file_name.str);
										bmp_monochrome_export(compressed_monochrome_slot_bmp, scratch_path.str);
									}
								}

								free(compressed_monochrome_slot_bmp.data);
								free(monochrome_slot_bmp.data);
								free(raw_slot_bmp.data);

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
						error("`FindClose` failed on \"%s\".", wildcard_training_dir_path.data);
					}
				}
				printf("Finished processing %d images.\n", processed_amount);
			}

			for (i32 i = 0; i < countof(exam_entries); i += 1)
			{
				free(exam_entries[i].monochrome_bmp.data);
				free(exam_entries[i].matches.data);
			}
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
