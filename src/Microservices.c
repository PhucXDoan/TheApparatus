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
#include <math.h>
#include "defs.h"
#include "misc.c"
#include "string.c"
#include "dary.c"
#include "Microservices_fs.c"
#include "Microservices_bmp.c"

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

	i64 margin = program_info.name.length;
	for (i32 field_index = {0}; field_index < program_info.field_count; field_index += 1)
	{
		i64 length = program_info.fields[field_index].pattern.length + 2;
		if (margin < length)
		{
			margin = length;
		}
	}
	margin += 1;

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
		i32(margin - program_info.name.length), "",
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
			i32(margin - cli_field_length), "",
			i32(field_info.desc.length), field_info.desc.data
		);
	}
}

struct DirBMPIteratorState
{
	str              dir_path;
	struct BMP       bmp;
	HANDLE           finder_handle;
	WIN32_FIND_DATAA finder_data;
	b32              inited;
};
static b32 // Found BMP.
iterate_dir_bmp_alloc(struct DirBMPIteratorState* state)
{
	b32 found_bmp = false;

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
		free(state->bmp.data);

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

				state->bmp = bmp_alloc_read_file(file_path.str);

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
	enum CLIProgram cli_program = CLIProgram_COUNT;

	if (argc >= 2) // Determine CLI program.
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

	if (argc <= 2) // Provide CLI help.
	{
		str exe_name = argc ? str_cstr(argv[0]) : str(CLI_EXE_NAME);

		if (argc <= 1)
		{
			i64 margin = exe_name.length;
			for (enum CLIProgram program = {0}; program < CLIProgram_COUNT; program += 1)
			{
				if (margin < CLI_PROGRAM_INFO[program].name.length)
				{
					margin = CLI_PROGRAM_INFO[program].name.length;
				}
			}
			margin += 1;

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
				i32(margin - exe_name.length), "",
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
					i32(margin - program_info.name.length), "",
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
	else
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
				struct CLIProgram_extractor_t cli                  = cli_unknown.extractor;
				i32                           screenshots_examined = 0;
				i32                           unknown_games        = 0;
				i32                           slots_extracted      = 0;
				f64_3                         accumulated_unknown_game_test_region_rgbs[WordGame_COUNT] = {0};

				if (!create_dir(cli.output_dir_path.str, cli.clear_output_dir) && !cli.clear_output_dir)
				{
					error("Output directory \"%s\" is not empty. Use (--clear-output-dir) to empty the directory for processing.\n", cli.output_dir_path.cstr);
				}

				printf("EXTRACTOR EXTRACTIN' FROM \"%s\"!!\n", cli.input_dir_path.cstr);

				struct DirBMPIteratorState iterator_state =
					{
						.dir_path = cli.input_dir_path.str
					};
				while (iterate_dir_bmp_alloc(&iterator_state))
				{
					if (iterator_state.bmp.dim.x == EXTRACTOR_SCREENSHOT_DIM_X && iterator_state.bmp.dim.y == EXTRACTOR_SCREENSHOT_DIM_Y)
					{
						enum WordGame identified_wordgame = WordGame_COUNT;

						//
						// Analyze screenshot.
						//

						f64_3 unknown_game_test_region_rgbs[WordGame_COUNT] = {0};
						for (enum WordGame wordgame = {0}; wordgame < WordGame_COUNT; wordgame += 1)
						{
							//
							// Compute average RGB of test region.
							//

							f64_3 test_region_rgb = {0};
							for
							(
								i32 y = WORDGAME_INFO[wordgame].test_region_pos.y;
								y < WORDGAME_INFO[wordgame].test_region_pos.y + WORDGAME_INFO[wordgame].test_region_dim.y;
								y += 1
							)
							{
								for
								(
									i32 x = WORDGAME_INFO[wordgame].test_region_pos.x;
									x < WORDGAME_INFO[wordgame].test_region_pos.x + WORDGAME_INFO[wordgame].test_region_dim.x;
									x += 1
								)
								{
									test_region_rgb.x += iterator_state.bmp.data[y * iterator_state.bmp.dim.x + x].r;
									test_region_rgb.y += iterator_state.bmp.data[y * iterator_state.bmp.dim.x + x].g;
									test_region_rgb.z += iterator_state.bmp.data[y * iterator_state.bmp.dim.x + x].b;
								}
							}
							test_region_rgb.x /= 256.0 * WORDGAME_INFO[wordgame].test_region_dim.x * WORDGAME_INFO[wordgame].test_region_dim.y;
							test_region_rgb.y /= 256.0 * WORDGAME_INFO[wordgame].test_region_dim.x * WORDGAME_INFO[wordgame].test_region_dim.y;
							test_region_rgb.z /= 256.0 * WORDGAME_INFO[wordgame].test_region_dim.x * WORDGAME_INFO[wordgame].test_region_dim.y;

							//
							// Extract slots.
							//

							if
							(
								fabs(test_region_rgb.x - WORDGAME_INFO[wordgame].test_region_rgb.x) <= EXTRACTOR_RGB_EPSILON &&
								fabs(test_region_rgb.y - WORDGAME_INFO[wordgame].test_region_rgb.y) <= EXTRACTOR_RGB_EPSILON &&
								fabs(test_region_rgb.z - WORDGAME_INFO[wordgame].test_region_rgb.z) <= EXTRACTOR_RGB_EPSILON
							)
							{
								struct BMP slot =
									{
										.dim.x = WORDGAME_INFO[wordgame].slot_dim,
										.dim.y = WORDGAME_INFO[wordgame].slot_dim,
									};
								alloc(&slot.data, slot.dim.x * slot.dim.y);

								for (i32 slot_coord_y = 0; slot_coord_y < WORDGAME_INFO[wordgame].board_dim_slots.y; slot_coord_y += 1)
								{
									for (i32 slot_coord_x = 0; slot_coord_x < WORDGAME_INFO[wordgame].board_dim_slots.x; slot_coord_x += 1)
									{
										b32 slot_coord_excluded = false;
										for (i32 i = 0; i < WORDGAME_INFO[wordgame].excluded_slot_coords_count; i += 1)
										{
											if
											(
												WORDGAME_INFO[wordgame].excluded_slot_coords[i].x == slot_coord_x &&
												WORDGAME_INFO[wordgame].excluded_slot_coords[i].y == slot_coord_y
											)
											{
												slot_coord_excluded = true;
												break;
											}
										}

										if (!slot_coord_excluded)
										{
											for (i32 slot_px_y = 0; slot_px_y < WORDGAME_INFO[wordgame].slot_dim; slot_px_y += 1)
											{
												for (i32 slot_px_x = 0; slot_px_x < WORDGAME_INFO[wordgame].slot_dim; slot_px_x += 1)
												{
													struct BMPPixel pixel =
														iterator_state.bmp.data
														[
															(WORDGAME_INFO[wordgame].board_pos.y + slot_coord_y * WORDGAME_INFO[wordgame].slot_stride + slot_px_y) * iterator_state.bmp.dim.x
																+ (WORDGAME_INFO[wordgame].board_pos.x + slot_coord_x * WORDGAME_INFO[wordgame].slot_stride + slot_px_x)
														];
													slot.data[slot_px_y * slot.dim.x + slot_px_x] = pixel;
												}
											}

											struct StrBuf slot_file_path = StrBuf(256);
											strbuf_str (&slot_file_path, cli.output_dir_path.str);
											strbuf_char(&slot_file_path, '\\');
											strbuf_str (&slot_file_path, WORDGAME_INFO[wordgame].print_name);
											strbuf_cstr(&slot_file_path, " (");
											strbuf_i64 (&slot_file_path, slot_coord_x);
											strbuf_cstr(&slot_file_path, ", ");
											strbuf_i64 (&slot_file_path, slot_coord_y);
											strbuf_cstr(&slot_file_path, ") (");
											for (i32 i = 0; i < 4; i += 1)
											{
												static const char CHARACTERS[] =
													{
														'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
														'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
														'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
														'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
														'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
														'-', '_'
													};
												strbuf_char(&slot_file_path, CHARACTERS[__rdtsc() % countof(CHARACTERS)]);
											}
											strbuf_cstr(&slot_file_path, ") ");
											strbuf_cstr(&slot_file_path, iterator_state.finder_data.cFileName);
											bmp_export(slot, slot_file_path.str);
										}
									}
								}


								identified_wordgame  = wordgame;
								slots_extracted     += WORDGAME_INFO[wordgame].board_dim_slots.x * WORDGAME_INFO[wordgame].board_dim_slots.y - WORDGAME_INFO[wordgame].excluded_slot_coords_count;

								free(slot.data);
								break;
							}
							else
							{
								unknown_game_test_region_rgbs[wordgame].x = test_region_rgb.x;
								unknown_game_test_region_rgbs[wordgame].y = test_region_rgb.y;
								unknown_game_test_region_rgbs[wordgame].z = test_region_rgb.z;
							}
						}

						//
						// Print details about the screenshot.
						//

						str game_name = {0};
						if (identified_wordgame < WordGame_COUNT)
						{
							game_name = WORDGAME_INFO[identified_wordgame].print_name;
						}
						else
						{
							game_name      = str("Unknown game");
							unknown_games += 1;

							for (enum WordGame wordgame = {0}; wordgame < WordGame_COUNT; wordgame += 1)
							{
								accumulated_unknown_game_test_region_rgbs[wordgame].x += unknown_game_test_region_rgbs[wordgame].x;
								accumulated_unknown_game_test_region_rgbs[wordgame].y += unknown_game_test_region_rgbs[wordgame].y;
								accumulated_unknown_game_test_region_rgbs[wordgame].z += unknown_game_test_region_rgbs[wordgame].z;
							}
						}

						screenshots_examined += 1;

						printf
						(
							"[extractor] % 4d : %.*s : \"%s\".\n",
							screenshots_examined,
							i32(game_name.length), game_name.data,
							iterator_state.finder_data.cFileName
						);
					}
				}

				//
				// Print final results.
				//

				if (screenshots_examined)
				{
					printf
					(
						"\n"
						"EXTRACTOR EXTRACTED FROM %d SCREENSHOTS!\n"
						"EXTRACTOR EXTRACTED %d SLOTS!!\n"
						"\n",
						screenshots_examined,
						slots_extracted
					);

					if (unknown_games)
					{
						i64 margin = 0;
						for (i32 wordgame = {0}; wordgame < WordGame_COUNT; wordgame += 1)
						{
							if (margin < WORDGAME_INFO[wordgame].print_name.length)
							{
								margin = WORDGAME_INFO[wordgame].print_name.length;
							}
						}
						margin += 1;

						printf("There were %d unknown games!\n", unknown_games);
						for (enum WordGame wordgame = {0}; wordgame < WordGame_COUNT; wordgame += 1)
						{
							printf
							(
								"Average Test Region RGB of %.*s%*s: (%.17g, %.17g, %.17g)\n",
								i32(WORDGAME_INFO[wordgame].print_name.length), WORDGAME_INFO[wordgame].print_name.data,
								i32(margin - WORDGAME_INFO[wordgame].print_name.length), "",
								accumulated_unknown_game_test_region_rgbs[wordgame].x / unknown_games,
								accumulated_unknown_game_test_region_rgbs[wordgame].y / unknown_games,
								accumulated_unknown_game_test_region_rgbs[wordgame].z / unknown_games
							);
						}
						printf("\n");
					}
				}
				else
				{
					printf
					(
						"EXTRACTOR AIN'T EXTRACTED NO SCREENSHOTS!!\n"
						"\n"
					);
				}
			} break;

			case CLIProgram_monochromize:
			{
				struct CLIProgram_monochromize_t cli              = cli_unknown.monochromize;
				i32                              images_processed = 0;

				if (!create_dir(cli.output_dir_path.str, cli.clear_output_dir) && !cli.clear_output_dir)
				{
					error("Output directory \"%s\" is not empty. Use (--clear-output-dir) to empty the directory for processing.\n", cli.output_dir_path.cstr);
				}

				struct DirBMPIteratorState iterator_state =
					{
						.dir_path = cli.input_dir_path.str
					};
				while (iterate_dir_bmp_alloc(&iterator_state))
				{
					for (i32 y = 0; y < iterator_state.bmp.dim.y; y += 1)
					{
						for (i32 x = 0; x < iterator_state.bmp.dim.x; x += 1)
						{
							struct BMPPixel* pixel = &iterator_state.bmp.data[y * iterator_state.bmp.dim.x + x];

							if ((pixel->r + pixel->g + pixel->b) / 3 <= MONOCHROMIZE_THRESHOLD)
							{
								*pixel = (struct BMPPixel) { .r = 255, .g = 255, .b = 255, .a = 255 };
							}
							else
							{
								*pixel = (struct BMPPixel) { .r = 0, .g = 0, .b = 0, .a = 255 };
							}
						}
					}

					images_processed += 1;

					struct StrBuf file_path = StrBuf(256);
					strbuf_str (&file_path, cli.output_dir_path.str);
					strbuf_char(&file_path, '\\');
					strbuf_cstr(&file_path, iterator_state.finder_data.cFileName);
					bmp_export(iterator_state.bmp, file_path.str);

					printf("[MONOCHROMIZE] %4d : \"%s\".\n", images_processed, iterator_state.finder_data.cFileName);
				}

				if (images_processed)
				{
					printf
					(
						"\n"
						"Monochromized %d images.\n"
						"Thank you.\n"
						"\n",
						images_processed
					);
				}
				else
				{
					printf
					(
						"No images were monochromized.\n"
						"Screw you.\n"
						"\n"
					);
				}
			} break;

			case CLIProgram_stretchie:
			{
				struct CLIProgram_stretchie_t cli              = cli_unknown.stretchie;
				i32                           images_processed = 0;

				struct BMP stretchied_bmp =
					{
						.dim.x = MASK_DIM,
						.dim.y = MASK_DIM,
					};
				alloc(&stretchied_bmp.data, stretchied_bmp.dim.x * stretchied_bmp.dim.y);

				if (!create_dir(cli.output_dir_path.str, cli.clear_output_dir) && !cli.clear_output_dir)
				{
					error("Output directory \"%s\" is not empty. Use (--clear-output-dir) to empty the directory for processing.\n", cli.output_dir_path.cstr);
				}

				struct DirBMPIteratorState iterator_state =
					{
						.dir_path = cli.input_dir_path.str
					};
				while (iterate_dir_bmp_alloc(&iterator_state))
				{
					for (i32 stretchied_y = 0; stretchied_y < stretchied_bmp.dim.y; stretchied_y += 1)
					{
						for (i32 stretchied_x = 0; stretchied_x < stretchied_bmp.dim.x; stretchied_x += 1)
						{
							stretchied_bmp.data[stretchied_y * stretchied_bmp.dim.x + stretchied_x] =
								iterator_state.bmp.data
								[
									i32(f64(stretchied_y) / f64(stretchied_bmp.dim.y) * f64(iterator_state.bmp.dim.y)) * iterator_state.bmp.dim.x
										+ i32(f64(stretchied_x) / f64(stretchied_bmp.dim.x) * f64(iterator_state.bmp.dim.x))
								];
						}
					}

					struct StrBuf file_path = StrBuf(256);
					strbuf_str (&file_path, cli.output_dir_path.str);
					strbuf_char(&file_path, '\\');
					strbuf_cstr(&file_path, iterator_state.finder_data.cFileName);
					bmp_export(stretchied_bmp, file_path.str);

					printf("[STRETCHIE] % 4d : \"%s\".\n", images_processed + 1, iterator_state.finder_data.cFileName);

					images_processed += 1;
				}

				if (images_processed)
				{
					printf
					(
						"\n"
						"Stretchie'd %d images!\n"
						"\n",
						images_processed
					);
				}
				else
				{
					printf
					(
						"No images were stretchie'd!\n"
						"\n"
					);
				}

				free(stretchied_bmp.data);
			} break;

			case CLIProgram_collectune:
			{
				struct CLIProgram_collectune_t cli              = cli_unknown.collectune;
				i32                            images_processed = 0;

				struct BMP masks[Letter_COUNT] = {0};
				for (enum Letter letter = {0}; letter < Letter_COUNT; letter += 1)
				{
					//
					// Create folder to collect best matching letters into.
					//

					{
						struct StrBuf letter_dir_path = StrBuf(256);
						strbuf_str (&letter_dir_path, cli.output_dir_path.str);
						strbuf_char(&letter_dir_path, '\\');
						strbuf_str (&letter_dir_path, LETTER_NAMES[letter]);
						create_dir(letter_dir_path.str, cli.clear_output_dir);
					}

					//
					// Load mask of the letter.
					//

					{
						struct StrBuf mask_file_path = StrBuf(256);
						strbuf_str (&mask_file_path, cli.mask_dir_path.str);
						strbuf_char(&mask_file_path, '\\');
						strbuf_str (&mask_file_path, LETTER_NAMES[letter]);
						strbuf_cstr(&mask_file_path, ".bmp");
						masks[letter] = bmp_alloc_read_file(mask_file_path.str);
						if (masks[letter].dim.x != MASK_DIM || masks[letter].dim.y != MASK_DIM)
						{
							error
							(
								"Mask \"%.*s\" does not have mask dimensions (%dx%d).\n",
								i32(cli.mask_dir_path.str.length), cli.mask_dir_path.str.data,
								MASK_DIM, MASK_DIM
							);
						}
					}
				}

				if (!create_dir(cli.output_dir_path.str, cli.clear_output_dir) && !cli.clear_output_dir)
				{
					error("Output directory \"%s\" is not empty. Use (--clear-output-dir) to empty the directory for processing.\n", cli.output_dir_path.cstr);
				}

				struct DirBMPIteratorState iterator_state =
					{
						.dir_path = cli.unsorted_dir_path.str
					};
				while (iterate_dir_bmp_alloc(&iterator_state))
				{
					if (iterator_state.bmp.dim.x == MASK_DIM && iterator_state.bmp.dim.y == MASK_DIM)
					{
						enum Letter best_matching_letter = Letter_COUNT;
						i32         best_matching_score  = 0;

						for (enum Letter letter = {0}; letter < Letter_COUNT; letter += 1)
						{
							i32 score = 0;
							for (i32 y = 0; y < iterator_state.bmp.dim.y; y += 1)
							{
								for (i32 x = 0; x < iterator_state.bmp.dim.x; x += 1)
								{
									if
									(
										iterator_state.bmp.data[y * iterator_state.bmp.dim.x + x].r == masks[letter].data[y * masks[letter].dim.x + x].r &&
										iterator_state.bmp.data[y * iterator_state.bmp.dim.x + x].g == masks[letter].data[y * masks[letter].dim.x + x].g &&
										iterator_state.bmp.data[y * iterator_state.bmp.dim.x + x].b == masks[letter].data[y * masks[letter].dim.x + x].b &&
										iterator_state.bmp.data[y * iterator_state.bmp.dim.x + x].a == masks[letter].data[y * masks[letter].dim.x + x].a
									)
									{
										score += 1;
									}
								}
							}

							if (best_matching_score < score)
							{
								best_matching_letter = letter;
								best_matching_score  = score;
							}
						}

						assert(best_matching_letter != Letter_COUNT);

						struct StrBuf file_path = StrBuf(256);
						strbuf_str (&file_path, cli.output_dir_path.str);
						strbuf_char(&file_path, '\\');
						strbuf_str (&file_path, LETTER_NAMES[best_matching_letter]);
						strbuf_char(&file_path, '\\');
						strbuf_cstr(&file_path, iterator_state.finder_data.cFileName);
						bmp_export(iterator_state.bmp, file_path.str);

						printf("[CollecTune(TM)] % 4d : \"%s\".\n", images_processed + 1, iterator_state.finder_data.cFileName);

						images_processed += 1;
					}
				}

				if (images_processed)
				{
					printf
					(
						"\n"
						"Collectune'd %d images~~ :D\n"
						"\n",
						images_processed
					);
				}
				else
				{
					printf
					(
						"No images were monochromized! :(\n"
						"\n"
					);
				}

				for (i32 i = 0; i < countof(masks); i += 1)
				{
					free(masks[i].data);
				}
			} break;

			case CLIProgram_meltingpot:
			{
				struct CLIProgram_meltingpot_t cli              = cli_unknown.meltingpot;
				i32                            images_processed = 0;
				i32                            images_skipped   = 0;
				struct BMP                     meltingpot       = {0};
				i32_4*                         weights          = 0;

				struct DirBMPIteratorState iterator_state =
					{
						.dir_path = cli.input_dir_path.str
					};
				while (iterate_dir_bmp_alloc(&iterator_state))
				{
					//
					// Initialize meltingpotted BMP based on first found BMP.
					//

					if (!meltingpot.data)
					{
						if (!cli.or_filter)
						{
							alloc(&weights, iterator_state.bmp.dim.x * iterator_state.bmp.dim.y);
						}

						meltingpot.dim.x = iterator_state.bmp.dim.x;
						meltingpot.dim.y = iterator_state.bmp.dim.y;
						alloc(&meltingpot.data, meltingpot.dim.x * meltingpot.dim.y);

						printf
						(
							"Using \"%s\" for bounding dimensions %dx%d.\n",
							iterator_state.finder_data.cFileName,
							meltingpot.dim.x,
							meltingpot.dim.y
						);
					}

					//
					// Add some diversity.
					//

					if (iterator_state.bmp.dim.x == meltingpot.dim.x && iterator_state.bmp.dim.y == meltingpot.dim.y)
					{
						for (i32 i = 0; i < iterator_state.bmp.dim.x * iterator_state.bmp.dim.y; i += 1)
						{
							if (cli.or_filter)
							{
								meltingpot.data[i].r |= iterator_state.bmp.data[i].r;
								meltingpot.data[i].g |= iterator_state.bmp.data[i].g;
								meltingpot.data[i].b |= iterator_state.bmp.data[i].b;
								meltingpot.data[i].a |= iterator_state.bmp.data[i].a;
							}
							else
							{
								weights[i].x += iterator_state.bmp.data[i].r;
								weights[i].y += iterator_state.bmp.data[i].g;
								weights[i].z += iterator_state.bmp.data[i].b;
								weights[i].w += iterator_state.bmp.data[i].a;
							}
						}

						printf("[MELTINGPOT] % 4d : \"%s\".\n", images_processed + 1, iterator_state.finder_data.cFileName);
						images_processed += 1;
					}
					else
					{
						printf("\t[MELTINGPOT] Deporting \"%s\".\n", iterator_state.finder_data.cFileName);
						images_skipped += 1;
					}
				}

				if (images_processed)
				{
					if (!cli.or_filter)
					{
						for (i32 i = 0; i < meltingpot.dim.x * meltingpot.dim.y; i += 1)
						{
							meltingpot.data[i] =
								(struct BMPPixel)
								{
									.r = u8(weights[i].x / images_processed),
									.g = u8(weights[i].y / images_processed),
									.b = u8(weights[i].z / images_processed),
									.a = u8(weights[i].w / images_processed),
								};
						}
					}

					bmp_export(meltingpot, cli.output_file_path.str);

					printf
					(
						"\n"
						"Meltingpotted %d images.\n"
						"\n",
						images_processed
					);
					if (images_skipped)
					{
						printf
						(
							"... but skipped %d images!\n"
							"\n",
							images_skipped
						);
					}
				}
				else
				{
					printf("No BMPs found.\n");
				}
			} break;

			case CLIProgram_maskiverse:
			{
				struct CLIProgram_maskiverse_t cli = cli_unknown.maskiverse;

				printf("[MASKIVERSE PHASE 1] Analyzing masks...\n");

				i64                             longest_letter_name_length = 0;
				struct BMP                      masks[Letter_COUNT]        = {0};
				struct { i32 bottom; i32 top; } empty_rows[Letter_COUNT]   = {0};
				for (enum Letter letter = {0}; letter < Letter_COUNT; letter += 1)
				{
					if (longest_letter_name_length < LETTER_NAMES[letter].length)
					{
						longest_letter_name_length = LETTER_NAMES[letter].length;
					}

					struct StrBuf mask_file_path = StrBuf(256);
					strbuf_str (&mask_file_path, cli.dir_path.str);
					strbuf_char(&mask_file_path, '\\');
					strbuf_str (&mask_file_path, LETTER_NAMES[letter]);
					strbuf_cstr(&mask_file_path, ".bmp");
					masks[letter] = bmp_alloc_read_file(mask_file_path.str);
					if (masks[letter].dim.x != MASK_DIM || masks[letter].dim.y != MASK_DIM)
					{
						error
						(
							"Mask \"%.*s\" does not have mask dimensions (%dx%d).\n",
							i32(cli.dir_path.str.length), cli.dir_path.str.data,
							MASK_DIM, MASK_DIM
						);
					}

					//
					// Count amount of empty bottom rows.
					//

					static_assert(MASK_DIM >= 16);
					for (i32 i = 0; i < 16; i += 1)
					{
						b32 row_empty = true;
						for (i32 x = 0; x < masks[letter].dim.x; x += 1)
						{
							if (masks[letter].data[i * masks[letter].dim.x + x].r)
							{
								row_empty = false;
								break;
							}
						}

						if (row_empty)
						{
							empty_rows[letter].bottom += 1;
							if (empty_rows[letter].bottom >= 16)
							{
								error("Too many empty bottom rows in \"%.*s\" for row-reduction!", i32(cli.dir_path.str.length), cli.dir_path.str.data);
							}
						}
						else
						{
							break;
						}
					}

					//
					// Count amount of top bottom rows.
					//

					static_assert(MASK_DIM >= 16);
					for (i32 i = 0; i < 16; i += 1)
					{
						b32 row_empty = true;
						for (i32 x = 0; x < masks[letter].dim.x; x += 1)
						{
							if (masks[letter].data[(masks[letter].dim.y - 1 - i) * masks[letter].dim.x + x].r)
							{
								row_empty = false;
								break;
							}
						}

						if (row_empty)
						{
							empty_rows[letter].top += 1;
							if (empty_rows[letter].top >= 16)
							{
								error("Too many empty top rows in \"%.*s\" for row-reduction!", i32(cli.dir_path.str.length), cli.dir_path.str.data);
							}
						}
						else
						{
							break;
						}
					}
				}

				printf("[MASKIVERSE PHASE 2] Generating row-reduced masks...\n");

				struct StrBuf c_source_file_path = StrBuf(256);
				strbuf_str (&c_source_file_path, cli.dir_path.str);
				strbuf_cstr(&c_source_file_path, "\\masks.h");
				HANDLE c_source_handle = create_file_writing_handle(c_source_file_path.str);

				struct StrBuf output_buf = StrBuf(256);

				for (enum Letter letter = {0}; letter < Letter_COUNT; letter += 1)
				{
					strbuf_cstr(&output_buf, "static const u8 ROW_REDUCED_MASK_");
					strbuf_str (&output_buf, LETTER_NAMES[letter]);
					strbuf_cstr(&output_buf, "[][MASK_DIM / 8] PROGMEM =\n");
					strbuf_cstr(&output_buf, "\t{\n");
					for
					(
						i32 y = empty_rows[letter].bottom;
						y < masks[letter].dim.y - empty_rows[letter].top;
						y += 1
					)
					{
						static_assert(MASK_DIM % 8 == 0);
						for
						(
							i32 x_major = 0;
							x_major < masks[letter].dim.x;
							x_major += 8
						)
						{
							if (!x_major)
							{
								strbuf_cstr(&output_buf, "\t\t{");
							}

							u8 literal = 0;
							for
							(
								i32 x_minor = 0;
								x_minor < 8 && x_major + x_minor < masks[letter].dim.x;
								x_minor += 1
							)
							{
								literal |= u8(!!masks[letter].data[y * masks[letter].dim.x + x_major + x_minor].r) << x_minor;
							}

							strbuf_cstr(&output_buf, " 0b");
							strbuf_8b  (&output_buf, literal);
							strbuf_char(&output_buf, ',');
						}
						strbuf_cstr(&output_buf, " },\n");
						write_flush_strbuf(c_source_handle, &output_buf);
					}
					strbuf_cstr(&output_buf, "\t};\n");
					write_flush_strbuf(c_source_handle, &output_buf);
				}

				printf("[MASKIVERSE PHASE 3] Generating entries...\n");

				strbuf_cstr(&output_buf, "const struct RowReducedMaskEntry ROW_REDUCED_MASK_ENTRIES[] PROGMEM =\n");
				strbuf_cstr(&output_buf, "\t{\n");
				for (enum Letter letter = {0}; letter < Letter_COUNT; letter += 1)
				{
					strbuf_cstr  (&output_buf, "\t\t[Letter_");
					strbuf_str   (&output_buf, LETTER_NAMES[letter]);
					strbuf_char_n(&output_buf, ' ', longest_letter_name_length - LETTER_NAMES[letter].length);
					strbuf_cstr  (&output_buf, "] = { .data = (const u8*) ROW_REDUCED_MASK_");

					strbuf_str   (&output_buf, LETTER_NAMES[letter]);
					strbuf_char_n(&output_buf, ' ', longest_letter_name_length - LETTER_NAMES[letter].length);
					strbuf_cstr  (&output_buf, ", .empty_rows = 0b");

					for (i32 i = 0; i < 4; i += 1)
					{
						strbuf_char(&output_buf, '0' + ((empty_rows[letter].top >> (3 - i) & 1)));
					}
					strbuf_char(&output_buf, '\'');
					for (i32 i = 0; i < 4; i += 1)
					{
						strbuf_char(&output_buf, '0' + ((empty_rows[letter].bottom >> (3 - i) & 1)));
					}

					strbuf_cstr(&output_buf, " },\n");

					write_flush_strbuf(c_source_handle, &output_buf);
				}
				strbuf_cstr(&output_buf, "\t};\n");
				write_flush_strbuf(c_source_handle, &output_buf);

				printf("[MASKIVERSE: ENDGAME] Complete!\n");

				close_file_writing_handle(c_source_handle);
				for (i32 i = 0; i < countof(masks); i += 1)
				{
					free(masks[i].data);
				}
			} break;

			case CLIProgram_COUNT:
			default:
			{
				assert(false);
			} break;
		}
	}

	return 0;
}

//
// Documentation.
//

/* [Overview].
	TODO
*/
