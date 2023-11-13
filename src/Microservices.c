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

static void
alloc_load_masks(struct BMP* dst_masks, str dir_path)
{
	dst_masks[0] = (struct BMP) {0};
	for (enum Letter letter = {1}; letter < Letter_COUNT; letter += 1)
	{
		struct StrBuf mask_file_path = StrBuf(256);
		strbuf_str (&mask_file_path, dir_path);
		strbuf_char(&mask_file_path, '\\');
		strbuf_str (&mask_file_path, LETTER_NAMES[letter]);
		strbuf_cstr(&mask_file_path, ".bmp");
		dst_masks[letter] = bmp_alloc_read_file(mask_file_path.str);
		if (dst_masks[letter].dim.x != MASK_DIM || dst_masks[letter].dim.y != MASK_DIM)
		{
			error
			(
				"Mask \"%.*s\" does not have mask dimensions (%dx%d).\n",
				i32(mask_file_path.length), mask_file_path.data,
				MASK_DIM, MASK_DIM
			);
		}
		for (i32 i = 0; i < dst_masks[letter].dim.x * dst_masks[letter].dim.y; i += 1)
		{
			if
			(
				(0 < dst_masks[letter].data[i].r && dst_masks[letter].data[i].r < 255) ||
				(0 < dst_masks[letter].data[i].g && dst_masks[letter].data[i].g < 255) ||
				(0 < dst_masks[letter].data[i].b && dst_masks[letter].data[i].b < 255) ||
				dst_masks[letter].data[i].a != 255
			)
			{
				error("Mask \"%.*s\" is not monochrome.\n", i32(dir_path.length), dir_path.data);
			}
		}
	}
}

static b32
slot_coord_excluded(enum WordGameBoard board, i32 x, i32 y)
{
	assert(board < WordGameBoard_COUNT);
	b32 result = false;
	for (i32 i = 0; i < WORDGAME_BOARD_INFO[board].excluded_slot_coords_count; i += 1)
	{
		if
		(
			WORDGAME_BOARD_INFO[board].excluded_slot_coords[i].x == x &&
			WORDGAME_BOARD_INFO[board].excluded_slot_coords[i].y == y
		)
		{
			result = true;
			break;
		}
	}
	return result;
}

static enum WordGameBoard // WordGameBoard_COUNT if no matching test region could be identified. In this case, "opt_dst_unknown_game_test_region_rgbs" will be filled if provided.
identify_wordgame_board
(
	struct BMP bmp,
	f64_3*     opt_dst_unknown_game_test_region_rgbs // Null, or at least WordGameBoard_COUNT.
)
{
	assert(bmp.dim.x == SCREENSHOT_DIM_X && bmp.dim.y == SCREENSHOT_DIM_Y);

	enum WordGameBoard identified_wordgame = WordGameBoard_COUNT;

	if (opt_dst_unknown_game_test_region_rgbs)
	{
		memset(opt_dst_unknown_game_test_region_rgbs, 0, sizeof(f64_3) * WordGameBoard_COUNT);
	}

	for (enum WordGameBoard board = {0}; board < WordGameBoard_COUNT; board += 1)
	{
		//
		// Compute average RGB of test region.
		//

		f64_3 test_region_rgb = {0};
		for
		(
			i32 y = WORDGAME_BOARD_INFO[board].test_region_pos.y;
			y < WORDGAME_BOARD_INFO[board].test_region_pos.y + WORDGAME_BOARD_INFO[board].test_region_dim.y;
			y += 1
		)
		{
			for
			(
				i32 x = WORDGAME_BOARD_INFO[board].test_region_pos.x;
				x < WORDGAME_BOARD_INFO[board].test_region_pos.x + WORDGAME_BOARD_INFO[board].test_region_dim.x;
				x += 1
			)
			{
				test_region_rgb.x += bmp.data[y * bmp.dim.x + x].r;
				test_region_rgb.y += bmp.data[y * bmp.dim.x + x].g;
				test_region_rgb.z += bmp.data[y * bmp.dim.x + x].b;
			}
		}
		test_region_rgb.x /= 256.0 * WORDGAME_BOARD_INFO[board].test_region_dim.x * WORDGAME_BOARD_INFO[board].test_region_dim.y;
		test_region_rgb.y /= 256.0 * WORDGAME_BOARD_INFO[board].test_region_dim.x * WORDGAME_BOARD_INFO[board].test_region_dim.y;
		test_region_rgb.z /= 256.0 * WORDGAME_BOARD_INFO[board].test_region_dim.x * WORDGAME_BOARD_INFO[board].test_region_dim.y;

		//
		// Extract slots.
		//

		if
		(
			fabs(test_region_rgb.x - WORDGAME_BOARD_INFO[board].test_region_rgb.x) <= EXTRACTOR_RGB_EPSILON &&
			fabs(test_region_rgb.y - WORDGAME_BOARD_INFO[board].test_region_rgb.y) <= EXTRACTOR_RGB_EPSILON &&
			fabs(test_region_rgb.z - WORDGAME_BOARD_INFO[board].test_region_rgb.z) <= EXTRACTOR_RGB_EPSILON
		)
		{
			identified_wordgame  = board;
			break;
		}
		else if (opt_dst_unknown_game_test_region_rgbs)
		{
			opt_dst_unknown_game_test_region_rgbs[board].x = test_region_rgb.x;
			opt_dst_unknown_game_test_region_rgbs[board].y = test_region_rgb.y;
			opt_dst_unknown_game_test_region_rgbs[board].z = test_region_rgb.z;
		}
	}

	return identified_wordgame;
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
			case CLIProgram_eaglepeek:
			{
				struct CLIProgram_eaglepeek_t cli                  = cli_unknown.eaglepeek;
				i32                           screenshots_examined = 0;
				i32                           unknown_games        = 0;
				f64_3                         accumulated_unknown_game_test_region_rgbs[WordGameBoard_COUNT] = {0};
				i32                           wordgame_count[WordGameBoard_COUNT] = {0};

				printf("Hello. My name is Eagle Peek. How can I help you?\n");

				//
				// Iterate through each directory,
				//

				for (i32 input_dir_path_index = 0; input_dir_path_index < cli.input_dir_paths.length; input_dir_path_index += 1)
				{
					CLIFieldTyping_string_t input_dir_path = cli.input_dir_paths.data[input_dir_path_index];

					printf("You want me to go through \"%s\"? Sure thing bud!\n", input_dir_path.cstr);

					//
					// Iterate each screenshot.
					//

					struct DirBMPIteratorState iterator_state =
						{
							.dir_path = input_dir_path.str
						};
					while (iterate_dir_bmp_alloc(&iterator_state))
					{
						if (iterator_state.bmp.dim.x == SCREENSHOT_DIM_X && iterator_state.bmp.dim.y == SCREENSHOT_DIM_Y)
						{
							//
							// Identify game.
							//

							f64_3              unknown_game_test_region_rgbs[WordGameBoard_COUNT] = {0};
							enum WordGameBoard identified_wordgame = identify_wordgame_board(iterator_state.bmp, unknown_game_test_region_rgbs);

							//
							// Respond to identification result.
							//

							str game_name = {0};
							if (identified_wordgame < WordGameBoard_COUNT)
							{
								game_name                            = WORDGAME_BOARD_INFO[identified_wordgame].name;
								wordgame_count[identified_wordgame] += 1;
							}
							else
							{
								game_name      = str("Unknown game");
								unknown_games += 1;

								for (enum WordGameBoard board = {0}; board < WordGameBoard_COUNT; board += 1)
								{
									accumulated_unknown_game_test_region_rgbs[board].x += unknown_game_test_region_rgbs[board].x;
									accumulated_unknown_game_test_region_rgbs[board].y += unknown_game_test_region_rgbs[board].y;
									accumulated_unknown_game_test_region_rgbs[board].z += unknown_game_test_region_rgbs[board].z;
								}
							}

							//
							// Output this screenshot's identification result.
							//

							screenshots_examined += 1;

							printf
							(
								"[EaglePeek] % 4d : %.*s : \"%s\".\n",
								screenshots_examined,
								i32(game_name.length), game_name.data,
								iterator_state.finder_data.cFileName
							);
						}
					}

					printf("\n");
				}

				//
				// Output amount of known identified games.
				//

				i64 margin = 0;
				for (enum WordGameBoard board = {0}; board < WordGameBoard_COUNT; board += 1)
				{
					if (margin < WORDGAME_BOARD_INFO[board].name.length)
					{
						margin = WORDGAME_BOARD_INFO[board].name.length;
					}
				}
				margin += 1;

				for (enum WordGameBoard board = {0}; board < WordGameBoard_COUNT; board += 1)
				{
					printf
					(
						"%.*s%*s: %d\n",
						i32(WORDGAME_BOARD_INFO[board].name.length), WORDGAME_BOARD_INFO[board].name.data,
						i32(margin - WORDGAME_BOARD_INFO[board].name.length), "",
						wordgame_count[board]
					);
				}

				//
				// Output unknown games.
				//

				if (unknown_games)
				{
					printf
					(
						"\n"
						"Eagle Peek found %d unknown games!\n",
						unknown_games
					);
					for (enum WordGameBoard board = {0}; board < WordGameBoard_COUNT; board += 1)
					{
						printf
						(
							"Average Test Region RGB of %.*s%*s: Delta(%.4f, %.4f, %.4f) : Found(%.4f, %.4f, %.4f) : Expected(%.4f, %.4f, %.4f) @ (%d, %d) (%dx%d).\n",
							i32(WORDGAME_BOARD_INFO[board].name.length), WORDGAME_BOARD_INFO[board].name.data,
							i32(margin - WORDGAME_BOARD_INFO[board].name.length), "",
							fabs(accumulated_unknown_game_test_region_rgbs[board].x / unknown_games - WORDGAME_BOARD_INFO[board].test_region_rgb.x),
							fabs(accumulated_unknown_game_test_region_rgbs[board].y / unknown_games - WORDGAME_BOARD_INFO[board].test_region_rgb.y),
							fabs(accumulated_unknown_game_test_region_rgbs[board].z / unknown_games - WORDGAME_BOARD_INFO[board].test_region_rgb.z),
							accumulated_unknown_game_test_region_rgbs[board].x / unknown_games,
							accumulated_unknown_game_test_region_rgbs[board].y / unknown_games,
							accumulated_unknown_game_test_region_rgbs[board].z / unknown_games,
							WORDGAME_BOARD_INFO[board].test_region_rgb.x,
							WORDGAME_BOARD_INFO[board].test_region_rgb.y,
							WORDGAME_BOARD_INFO[board].test_region_rgb.z,
							WORDGAME_BOARD_INFO[board].test_region_pos.x,
							WORDGAME_BOARD_INFO[board].test_region_pos.y,
							WORDGAME_BOARD_INFO[board].test_region_dim.x,
							WORDGAME_BOARD_INFO[board].test_region_dim.y
						);
					}
					printf("\n");
				}
				else
				{
					printf
					(
						"\n"
						"Eagle Peek identified all games!"
						"\n"
					);
				}
			} break;

			case CLIProgram_extractorv2:
			{
				struct CLIProgram_extractorv2_t cli                  = cli_unknown.extractorv2;
				i32                             screenshots_examined = 0;
				i32                             unknown_games        = 0;
				i32                             slots_extracted      = 0;

				struct BMP compressed_slot =
					{
						.dim.x = MASK_DIM,
						.dim.y = MASK_DIM,
					};
				alloc(&compressed_slot.data, compressed_slot.dim.x * compressed_slot.dim.y);

				//
				// Clear output directory.
				//

				if (!create_dir(cli.output_dir_path.str, cli.clear_output_dir) && !cli.clear_output_dir)
				{
					error("Output directory \"%s\" is not empty. Use (--clear-output-dir) to empty the directory for processing.\n", cli.output_dir_path.cstr);
				}

				//
				// Iterate directories of screenshots to extract slots from.
				//

				for (i32 input_dir_path_index = 0; input_dir_path_index < cli.input_dir_paths.length; input_dir_path_index += 1)
				{
					CLIFieldTyping_string_t input_dir_path = cli.input_dir_paths.data[input_dir_path_index];

					printf
					(
						"(%d/%lld) EXTRACTORv2 EXTRACTIN' FROM \"%s\"!!\n",
						input_dir_path_index + 1,
						cli.input_dir_paths.length,
						input_dir_path.cstr
					);

					struct DirBMPIteratorState iterator_state =
						{
							.dir_path = input_dir_path.str
						};
					while (iterate_dir_bmp_alloc(&iterator_state))
					{
						if (iterator_state.bmp.dim.x == SCREENSHOT_DIM_X && iterator_state.bmp.dim.y == SCREENSHOT_DIM_Y)
						{
							//
							// Determine game.
							//

							enum WordGameBoard board = identify_wordgame_board(iterator_state.bmp, 0);
							if (board != WordGameBoard_COUNT)
							{
								//
								// Create a BMP of each slot.
								//

								for (i32 slot_coord_y = 0; slot_coord_y < WORDGAME_BOARD_INFO[board].dim_slots.y; slot_coord_y += 1)
								{
									for (i32 slot_coord_x = 0; slot_coord_x < WORDGAME_BOARD_INFO[board].dim_slots.x; slot_coord_x += 1)
									{
										b8 found_activated_pixel = false;
										if (!slot_coord_excluded(board, slot_coord_x, slot_coord_y))
										{
											for (i32 compressed_slot_px_y = 0; compressed_slot_px_y < MASK_DIM; compressed_slot_px_y += 1)
											{
												for (i32 compressed_slot_px_x = 0; compressed_slot_px_x < MASK_DIM; compressed_slot_px_x += 1)
												{
													i32_2 uncompressed_board_bmp_dim =
														{
															i32((WORDGAME_BOARD_INFO[board].dim_slots.x - 1) * WORDGAME_BOARD_INFO[board].uncompressed_slot_stride + WORDGAME_BOARD_INFO[board].slot_dim),
															i32((WORDGAME_BOARD_INFO[board].dim_slots.y - 1) * WORDGAME_BOARD_INFO[board].uncompressed_slot_stride + WORDGAME_BOARD_INFO[board].slot_dim),
														};
													i32_2 compressed_slot_abs_px_pos =
														{
															slot_coord_x * WORDGAME_BOARD_INFO[board].compressed_slot_stride + compressed_slot_px_x,
															slot_coord_y * WORDGAME_BOARD_INFO[board].compressed_slot_stride + compressed_slot_px_y,
														};
													i32_2 compressed_board_bmp_dim =
														{
															(WORDGAME_BOARD_INFO[board].dim_slots.x - 1) * WORDGAME_BOARD_INFO[board].compressed_slot_stride + MASK_DIM,
															(WORDGAME_BOARD_INFO[board].dim_slots.y - 1) * WORDGAME_BOARD_INFO[board].compressed_slot_stride + MASK_DIM,
														};
													f64_2 uv =
														{
															f64(compressed_slot_abs_px_pos.x) / f64(compressed_board_bmp_dim.x),
															f64(compressed_slot_abs_px_pos.y) / f64(compressed_board_bmp_dim.y),
														};
													struct BMPPixel pixel =
														iterator_state.bmp.data
														[
															( WORDGAME_BOARD_INFO[board].pos.y + i32(uv.y * uncompressed_board_bmp_dim.y)) * iterator_state.bmp.dim.x
															+ WORDGAME_BOARD_INFO[board].pos.x + i32(uv.x * uncompressed_board_bmp_dim.x)
														];

													u8 value = 0;
													if (pixel.r <= MASK_ACTIVATION_THRESHOLD)
													{
														found_activated_pixel = true;
														value                 = 255;
													}
													compressed_slot.data[compressed_slot_px_y * compressed_slot.dim.x + compressed_slot_px_x] =
														(struct BMPPixel) { value, value, value, 255 };
												}
											}

											if (found_activated_pixel)
											{
												struct StrBuf slot_file_path = StrBuf(256);
												strbuf_str (&slot_file_path, cli.output_dir_path.str);
												strbuf_char(&slot_file_path, '\\');
												strbuf_str (&slot_file_path, WORDGAME_BOARD_INFO[board].name);
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
												bmp_export(compressed_slot, slot_file_path.str);
											}
										}
									}
								}

								slots_extracted +=
									WORDGAME_BOARD_INFO[board].dim_slots.x * WORDGAME_BOARD_INFO[board].dim_slots.y
										- WORDGAME_BOARD_INFO[board].excluded_slot_coords_count;
							}

							//
							// Print details about the screenshot.
							//

							str wordgame_name = {0};
							if (board < WordGameBoard_COUNT)
							{
								wordgame_name = WORDGAME_BOARD_INFO[board].name;
							}
							else
							{
								wordgame_name  = str("Unknown game");
								unknown_games += 1;
							}

							screenshots_examined += 1;

							printf
							(
								"[extractorv2] % 4d : %.*s : \"%s\".\n",
								screenshots_examined,
								i32(wordgame_name.length), wordgame_name.data,
								iterator_state.finder_data.cFileName
							);
						}
					}

					printf("\n");
				}

				//
				// Output extraction results.
				//

				if (screenshots_examined)
				{
					printf
					(
						"EXTRACTOR EXTRACTED FROM %lld DIRECTORIES!\n"
						"EXTRACTOR EXTRACTED FROM %d SCREENSHOTS!\n"
						"EXTRACTOR EXTRACTED %d SLOTS!!\n"
						"\n",
						cli.input_dir_paths.length,
						screenshots_examined,
						slots_extracted
					);

					if (unknown_games)
					{
						printf
						(
							"BUT.............. THERE WERE %d UNKNOWN GAMES??!!?!?!?!\n"
							"\n",
							unknown_games
						);
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

				//
				// Clean up.
				//

				free(compressed_slot.data);
			} break;

			case CLIProgram_collectune:
			{
				struct CLIProgram_collectune_t cli                                 = cli_unknown.collectune;
				i32                            images_processed                    = 0;
				i32                            matched_letter_counts[Letter_COUNT] = {0};
				i32*                           merged_mask_weights                 = 0;
				alloc(&merged_mask_weights, MASK_DIM * MASK_DIM * Letter_COUNT);

				//
				// Load masks.
				//

				struct BMP masks[Letter_COUNT] = {0};
				alloc_load_masks(masks, cli.mask_dir_path.str);

				//
				// Create output directory with a subfolder for each letter.
				//

				if (!create_dir(cli.output_dir_path.str, cli.clear_output_dir) && !cli.clear_output_dir)
				{
					error("Output directory \"%s\" is not empty. Use (--clear-output-dir) to empty the directory for processing.\n", cli.output_dir_path.cstr);
				}

				for (enum Letter letter = {1}; letter < Letter_COUNT; letter += 1)
				{
					struct StrBuf letter_dir_path = StrBuf(256);
					strbuf_str (&letter_dir_path, cli.output_dir_path.str);
					strbuf_char(&letter_dir_path, '\\');
					strbuf_str (&letter_dir_path, LETTER_NAMES[letter]);
					create_dir(letter_dir_path.str, cli.clear_output_dir);
				}

				//
				// Iterate through each image that needs to be sorted.
				//

				struct DirBMPIteratorState iterator_state =
					{
						.dir_path = cli.unsorted_dir_path.str
					};
				while (iterate_dir_bmp_alloc(&iterator_state))
				{
					if (iterator_state.bmp.dim.x == MASK_DIM && iterator_state.bmp.dim.y == MASK_DIM)
					{
						//
						// Determine best matching letter.
						//

						enum Letter best_matching_letter = {0};
						i32         best_matching_score  = 0;

						for (enum Letter letter = {1}; letter < Letter_COUNT; letter += 1)
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

						assert(best_matching_letter);

						//
						// Add weight to merged masks.
						//

						matched_letter_counts[best_matching_letter] += 1;
						for (i32 y = 0; y < MASK_DIM; y += 1)
						{
							for (i32 x = 0; x < MASK_DIM; x += 1)
							{
								if
								(
									iterator_state.bmp.data[y * iterator_state.bmp.dim.x + x].r &&
									iterator_state.bmp.data[y * iterator_state.bmp.dim.x + x].g &&
									iterator_state.bmp.data[y * iterator_state.bmp.dim.x + x].b &&
									iterator_state.bmp.data[y * iterator_state.bmp.dim.x + x].a
								)
								{
									merged_mask_weights[MASK_DIM * MASK_DIM * best_matching_letter + y * MASK_DIM + x] += 1;
								}
							}
						}

						//
						// Copy BMP into the appropriate subfolder of the letter.
						//

						struct StrBuf file_path = StrBuf(256);
						strbuf_str (&file_path, cli.output_dir_path.str);
						strbuf_char(&file_path, '\\');
						strbuf_str (&file_path, LETTER_NAMES[best_matching_letter]);
						strbuf_char(&file_path, '\\');
						strbuf_cstr(&file_path, iterator_state.finder_data.cFileName);
						bmp_export(iterator_state.bmp, file_path.str);

						images_processed += 1;

						printf("[CollecTune(TM)] % 4d : \"%s\".\n", images_processed, iterator_state.finder_data.cFileName);
					}
				}

				//
				// Output average mask of each letter.
				//

				struct BMP new_avg_mask =
					{
						.dim.x = MASK_DIM,
						.dim.y = MASK_DIM,
					};
				alloc(&new_avg_mask.data, new_avg_mask.dim.x * new_avg_mask.dim.y);

				for (enum Letter letter = {1}; letter < Letter_COUNT; letter += 1)
				{
					for (i32 i = 0; i < MASK_DIM * MASK_DIM; i += 1)
					{
						if (merged_mask_weights[MASK_DIM * MASK_DIM * letter + i] > matched_letter_counts[letter] / 2)
						{
							new_avg_mask.data[i] = (struct BMPPixel) { 255, 255, 255, 255 };
						}
						else
						{
							new_avg_mask.data[i] = (struct BMPPixel) { 0, 0, 0, 255 };
						}
					}

					struct StrBuf file_path = StrBuf(256);
					strbuf_str (&file_path, cli.output_dir_path.str);
					strbuf_char(&file_path, '\\');
					strbuf_str (&file_path, LETTER_NAMES[letter]);
					strbuf_cstr(&file_path, ".bmp");
					bmp_export(new_avg_mask, file_path.str);
				}

				//
				// Output sorting results.
				//

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

				//
				// Clean up.
				//

				for (i32 i = 0; i < countof(masks); i += 1)
				{
					free(masks[i].data);
				}

				free(new_avg_mask.data);
				free(merged_mask_weights);
			} break;

			case CLIProgram_maskiversev2:
			{
				struct CLIProgram_maskiversev2_t cli = cli_unknown.maskiversev2;

				//
				// Load masks.
				//

				struct BMP masks[Letter_COUNT] = {0};
				alloc_load_masks(masks, cli.dir_path.str);

				//
				// Generate stream data.
				//

				HANDLE c_source_handle = create_file_writing_handle(cli.output_file_path.str);

				struct StrBuf output_buf = StrBuf(256);

				strbuf_cstr(&output_buf, "static const u8 MASK_U8_STREAM[] PROGMEM = { ");
				write_flush_strbuf(c_source_handle, &output_buf);

				struct Dary_u16 overflowed_runlengths = {0};
				i32_2           pos                   = { 0, MASK_DIM - 1 };
				enum Letter     curr_letter           = {1};
				i32             runlength             = 0;
				i32             run_count             = 0;
				while (true)
				{
					b32 flush    = false;
					b32 finished = false;

					//
					// Check if the stream would end.
					//

					if (pos.x == MASK_DIM)
					{
						pos.x        = 0;
						curr_letter += 1;

						if (curr_letter == Letter_COUNT)
						{
							curr_letter  = (enum Letter) {1};
							pos.y  -= 1;

							if (pos.y == -1)
							{
								finished = true;
								flush    = true;
							}
						}
					}

					//
					// Increase runlength if pixel matches.
					//

					if (!finished)
					{
						if ((masks[curr_letter].data[pos.y * MASK_DIM + pos.x].r & 1) == (run_count & 1))
						{
							runlength += 1;
							pos.x     += 1;
						}
						else
						{
							flush = true;
						}
					}

					//
					// Flush.
					//

					if (flush)
					{
						if (0 < runlength && runlength < 255)
						{
							strbuf_u64(&output_buf, runlength);
						}
						else
						{
							if (runlength > u16(-1))
							{
								error("Run length too long!");
							}

							dary_push(&overflowed_runlengths, &(u16) { u16(runlength) });
							strbuf_cstr(&output_buf, "0x00");
						}

						strbuf_cstr(&output_buf, ", "),
						write_flush_strbuf(c_source_handle, &output_buf);

						runlength  = 0;
						run_count += 1;
					}

					//
					// Finish.
					//

					if (finished)
					{
						break;
					}
				}

				strbuf_cstr
				(
					&output_buf,
					"};\n"
					"\n"
					"static const u16 MASK_U16_STREAM[] PROGMEM = { "
				);
				write_flush_strbuf(c_source_handle, &output_buf);

				for (i32 i = 0; i < overflowed_runlengths.length; i += 1)
				{
					strbuf_u64 (&output_buf, overflowed_runlengths.data[i]);
					strbuf_cstr(&output_buf, ", "),
					write_flush_strbuf(c_source_handle, &output_buf);
				}

				strbuf_cstr(&output_buf, " };\n");
				write_flush_strbuf(c_source_handle, &output_buf);

				//
				// Report results.
				//

				printf
				(
					"[MASKIVERSE] Complete!\n"
					"%zu bytes used for byte run-lengths.\n"
					"%zu bytes used for word run-lengths.\n"
					"%zu bytes of flash will be used total.\n",
					run_count * sizeof(u8),
					overflowed_runlengths.length * sizeof(u16),
					run_count * sizeof(u8) + overflowed_runlengths.length * sizeof(u16)
				);

				//
				// Clean up.
				//

				free(overflowed_runlengths.data);

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
