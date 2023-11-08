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
				i32(dir_path.length), dir_path.data,
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
	b32 slot_coord_excluded = false;
	for (i32 i = 0; i < WORDGAME_BOARD_INFO[board].excluded_slot_coords_count; i += 1)
	{
		if
		(
			WORDGAME_BOARD_INFO[board].excluded_slot_coords[i].x == x &&
			WORDGAME_BOARD_INFO[board].excluded_slot_coords[i].y == y
		)
		{
			slot_coord_excluded = true;
			break;
		}
	}
	return slot_coord_excluded;
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

static enum WordGameBoard // WordGameBoard_COUNT if no matching test region could be identified. In this case, "opt_dst_unknown_game_test_region_rgbs" will be filled if provided.
identify_wordgame_board
(
	struct BMP bmp,
	f64_3*     opt_dst_unknown_game_test_region_rgbs // Null, or at least WordGameBoard_COUNt.
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

							f64_3         unknown_game_test_region_rgbs[WordGameBoard_COUNT] = {0};
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
							"Average Test Region RGB of %.*s%*s: Delta(%.4f, %.4f, %.4f) : Expected(%.4f, %.4f, %.4f) @ (%d, %d) (%dx%d).\n",
							i32(WORDGAME_BOARD_INFO[board].name.length), WORDGAME_BOARD_INFO[board].name.data,
							i32(margin - WORDGAME_BOARD_INFO[board].name.length), "",
							fabs(accumulated_unknown_game_test_region_rgbs[board].x / unknown_games - WORDGAME_BOARD_INFO[board].test_region_rgb.x),
							fabs(accumulated_unknown_game_test_region_rgbs[board].y / unknown_games - WORDGAME_BOARD_INFO[board].test_region_rgb.y),
							fabs(accumulated_unknown_game_test_region_rgbs[board].z / unknown_games - WORDGAME_BOARD_INFO[board].test_region_rgb.z),
							accumulated_unknown_game_test_region_rgbs[board].x / unknown_games,
							accumulated_unknown_game_test_region_rgbs[board].y / unknown_games,
							accumulated_unknown_game_test_region_rgbs[board].z / unknown_games,
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

			case CLIProgram_extractor:
			{
				struct CLIProgram_extractor_t cli                  = cli_unknown.extractor;
				i32                           screenshots_examined = 0;
				i32                           unknown_games        = 0;
				i32                           slots_extracted      = 0;

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
						"(%d/%lld) EXTRACTOR EXTRACTIN' FROM \"%s\"!!\n",
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
								struct BMP slot =
									{
										.dim.x = WORDGAME_BOARD_INFO[board].slot_dim,
										.dim.y = WORDGAME_BOARD_INFO[board].slot_dim,
									};
								alloc(&slot.data, slot.dim.x * slot.dim.y);

								//
								// Create a BMP of each slot.
								//

								for (i32 slot_coord_y = 0; slot_coord_y < WORDGAME_BOARD_INFO[board].dim_slots.y; slot_coord_y += 1)
								{
									for (i32 slot_coord_x = 0; slot_coord_x < WORDGAME_BOARD_INFO[board].dim_slots.x; slot_coord_x += 1)
									{
										if (!slot_coord_excluded(board, slot_coord_x, slot_coord_y))
										{
											for (i32 slot_px_y = 0; slot_px_y < WORDGAME_BOARD_INFO[board].slot_dim; slot_px_y += 1)
											{
												for (i32 slot_px_x = 0; slot_px_x < WORDGAME_BOARD_INFO[board].slot_dim; slot_px_x += 1)
												{
													struct BMPPixel pixel =
														iterator_state.bmp.data
														[
															(WORDGAME_BOARD_INFO[board].pos.y + slot_coord_y * WORDGAME_BOARD_INFO[board].uncompressed_slot_stride + slot_px_y) * iterator_state.bmp.dim.x
																+ (WORDGAME_BOARD_INFO[board].pos.x + slot_coord_x * WORDGAME_BOARD_INFO[board].uncompressed_slot_stride + slot_px_x)
														];
													slot.data[slot_px_y * slot.dim.x + slot_px_x] = pixel;
												}
											}

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
											bmp_export(slot, slot_file_path.str);
										}
									}
								}

								slots_extracted +=
									WORDGAME_BOARD_INFO[board].dim_slots.x * WORDGAME_BOARD_INFO[board].dim_slots.y
										- WORDGAME_BOARD_INFO[board].excluded_slot_coords_count;

								free(slot.data);
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
								"[extractor] % 4d : %.*s : \"%s\".\n",
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
			} break;

			case CLIProgram_monochromize:
			{
				struct CLIProgram_monochromize_t cli              = cli_unknown.monochromize;
				i32                              images_processed = 0;

				//
				// Clear output directory.
				//

				if (!create_dir(cli.output_dir_path.str, cli.clear_output_dir) && !cli.clear_output_dir)
				{
					error("Output directory \"%s\" is not empty. Use (--clear-output-dir) to empty the directory for processing.\n", cli.output_dir_path.cstr);
				}

				//
				// Iterate through each image.
				//

				struct DirBMPIteratorState iterator_state =
					{
						.dir_path = cli.input_dir_path.str
					};
				while (iterate_dir_bmp_alloc(&iterator_state))
				{
					//
					// Create monochrome BMP based on some threshold.
					//

					for (i32 y = 0; y < iterator_state.bmp.dim.y; y += 1)
					{
						for (i32 x = 0; x < iterator_state.bmp.dim.x; x += 1)
						{
							struct BMPPixel* pixel = &iterator_state.bmp.data[y * iterator_state.bmp.dim.x + x];

							if (pixel->r <= MASK_ACTIVATION_THRESHOLD)
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

					//
					// Export BMP.
					//

					struct StrBuf file_path = StrBuf(256);
					strbuf_str (&file_path, cli.output_dir_path.str);
					strbuf_char(&file_path, '\\');
					strbuf_cstr(&file_path, iterator_state.finder_data.cFileName);
					bmp_export(iterator_state.bmp, file_path.str);

					printf("[MONOCHROMIZE] %4d : \"%s\".\n", images_processed, iterator_state.finder_data.cFileName);
				}

				//
				// Output processing results.
				//

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

				//
				// Initalize output BMP memory.
				//

				struct BMP stretchied_bmp =
					{
						.dim.x = MASK_DIM,
						.dim.y = MASK_DIM,
					};
				alloc(&stretchied_bmp.data, stretchied_bmp.dim.x * stretchied_bmp.dim.y);

				//
				// Clear output directory.
				//

				if (!create_dir(cli.output_dir_path.str, cli.clear_output_dir) && !cli.clear_output_dir)
				{
					error("Output directory \"%s\" is not empty. Use (--clear-output-dir) to empty the directory for processing.\n", cli.output_dir_path.cstr);
				}

				//
				// Iterate through each image in directory.
				//

				struct DirBMPIteratorState iterator_state =
					{
						.dir_path = cli.input_dir_path.str
					};
				while (iterate_dir_bmp_alloc(&iterator_state))
				{
					//
					// Do basic sampling.
					//

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

					//
					// Export scaled image.
					//

					images_processed += 1;

					struct StrBuf file_path = StrBuf(256);
					strbuf_str (&file_path, cli.output_dir_path.str);
					strbuf_char(&file_path, '\\');
					strbuf_cstr(&file_path, iterator_state.finder_data.cFileName);
					bmp_export(stretchied_bmp, file_path.str);

					printf("[STRETCHIE] % 4d : \"%s\".\n", images_processed, iterator_state.finder_data.cFileName);
				}

				//
				// Output processing result.
				//

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

				//
				// Clean up.
				//

				free(stretchied_bmp.data);
			} break;

			case CLIProgram_collectune:
			{
				struct CLIProgram_collectune_t cli              = cli_unknown.collectune;
				i32                            images_processed = 0;

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
			} break;

			case CLIProgram_meltingpot:
			{
				struct CLIProgram_meltingpot_t cli              = cli_unknown.meltingpot;
				i32                            images_processed = 0;
				i32                            images_skipped   = 0;
				struct BMP                     meltingpot       = {0};
				i32_4*                         weights          = 0;

				//
				// Iterate through each image that'll be merged.
				//

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
					// Add some diversity into the meltingpot!
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

						images_processed += 1;
						printf("[MELTINGPOT] % 4d : \"%s\".\n", images_processed, iterator_state.finder_data.cFileName);
					}
					else
					{
						images_skipped += 1;
						printf("\t[MELTINGPOT] Deporting \"%s\".\n", iterator_state.finder_data.cFileName);
					}
				}

				//
				// Report processing results.
				//

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

				//
				// Load masks.
				//

				struct BMP masks[Letter_COUNT] = {0};
				alloc_load_masks(masks, cli.dir_path.str);

				//
				// Iterate through each mask.
				//

				printf("[MASKIVERSE PHASE 1] Row reducing masks...\n");

				struct { i32 bottom; i32 top; } empty_rows[Letter_COUNT] = {0};
				i32                             bytes_of_flash_used      = (MASK_DIM * MASK_DIM / 8 + 3) * Letter_COUNT;
				i32                             total_bytes_saved        = 0;
				for (enum Letter letter = {1}; letter < Letter_COUNT; letter += 1)
				{
					//
					// Count amount of empty bottom rows.
					//

					while (empty_rows[letter].bottom < masks[letter].dim.y)
					{
						b32 row_used = false;
						for (i32 x = 0; x < masks[letter].dim.x; x += 1)
						{
							if (masks[letter].data[empty_rows[letter].bottom * masks[letter].dim.x + x].r)
							{
								row_used = true;
								break;
							}
						}
						if (row_used)
						{
							break;
						}
						else
						{
							empty_rows[letter].bottom += 1;
						}
					}

					if (empty_rows[letter].bottom >= ROW_REDUCTION_SIZE)
					{
						error("Too many empty bottom rows in \"%.*s\" for row-reduction!", i32(cli.dir_path.str.length), cli.dir_path.str.data);
					}

					//
					// Count amount of empty top rows.
					//

					while (empty_rows[letter].top < masks[letter].dim.y)
					{
						b32 row_used = false;
						for (i32 x = 0; x < masks[letter].dim.x; x += 1)
						{
							if (masks[letter].data[(masks[letter].dim.y - 1 - empty_rows[letter].top) * masks[letter].dim.x + x].r)
							{
								row_used = true;
								break;
							}
						}
						if (row_used)
						{
							break;
						}
						else
						{
							empty_rows[letter].top += 1;
						}
					}

					if (empty_rows[letter].top >= ROW_REDUCTION_SIZE)
					{
						error("Too many empty top rows in \"%.*s\" for row-reduction!", i32(cli.dir_path.str.length), cli.dir_path.str.data);
					}

					//
					// Report memory saved.
					//

					i32 bytes_saved = (empty_rows[letter].bottom + empty_rows[letter].top) * masks[letter].dim.x / 8;

					total_bytes_saved += bytes_saved;

					printf
					(
						"\t\'%.*s'%*s : %3d bytes reduced.\n",
						i32(LETTER_NAMES[letter].length), LETTER_NAMES[letter].data,
						i32(LETTER_MAX_NAME_LENGTH - LETTER_NAMES[letter].length), "",
						bytes_saved
					);

					bytes_of_flash_used -= bytes_saved;
				}

				//
				// Begin to generate C data of each row-reduced mask.
				//

				printf("[MASKIVERSE PHASE 2] Generating row-reduced masks...\n");

				struct StrBuf c_source_file_path = StrBuf(256);
				strbuf_str (&c_source_file_path, cli.dir_path.str);
				strbuf_cstr(&c_source_file_path, "\\masks.h");
				HANDLE c_source_handle = create_file_writing_handle(c_source_file_path.str);

				struct StrBuf output_buf = StrBuf(256);

				for (enum Letter letter = {1}; letter < Letter_COUNT; letter += 1)
				{
					//
					// Declaration.
					//

					strbuf_cstr(&output_buf, "static const u8 ROW_REDUCED_MASK_");
					strbuf_str (&output_buf, LETTER_NAMES[letter]);
					strbuf_cstr(&output_buf, "[][MASK_DIM / 8] PROGMEM =\n");
					strbuf_cstr(&output_buf, "\t{\n");

					//
					// Actual values.
					//

					for
					(
						i32 y = empty_rows[letter].bottom;
						y < masks[letter].dim.y - empty_rows[letter].top;
						y += 1
					)
					{
						static_assert(MASK_DIM >= ROW_REDUCTION_SIZE);
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

					//
					// Closing.
					//

					strbuf_cstr(&output_buf, "\t};\n");
				}
				write_flush_strbuf(c_source_handle, &output_buf);

				//
				// Generate C data for the entries of the look-up table for the row-reduced masks.
				//

				printf("[MASKIVERSE PHASE 3] Generating entries...\n");

				strbuf_cstr(&output_buf, "static const struct RowReducedMaskEntry ROW_REDUCED_MASK_ENTRIES[] PROGMEM =\n");
				strbuf_cstr(&output_buf, "\t{\n");
				for (enum Letter letter = {1}; letter < Letter_COUNT; letter += 1)
				{
					strbuf_cstr  (&output_buf, "\t\t[Letter_");
					strbuf_str   (&output_buf, LETTER_NAMES[letter]);
					strbuf_char_n(&output_buf, ' ', LETTER_MAX_NAME_LENGTH - LETTER_NAMES[letter].length);
					strbuf_cstr  (&output_buf, "] = { .data = (const u8*) ROW_REDUCED_MASK_");
					strbuf_str   (&output_buf, LETTER_NAMES[letter]);
					strbuf_char_n(&output_buf, ' ', LETTER_MAX_NAME_LENGTH - LETTER_NAMES[letter].length);
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

				//
				// Report results.
				//

				printf
				(
					"[MASKIVERSE: ENDGAME] Complete! Row-reduced to %d bytes out of %d bytes.\n",
					bytes_of_flash_used,
					bytes_of_flash_used + total_bytes_saved
				);

				//
				// Clean up.
				//

				close_file_writing_handle(c_source_handle);

				for (i32 i = 0; i < countof(masks); i += 1)
				{
					free(masks[i].data);
				}
			} break;

			case CLIProgram_catchya:
			{
				struct CLIProgram_catchya_t cli = cli_unknown.catchya;

				//
				// Load masks.
				//

				struct BMP masks[Letter_COUNT] = {0};
				alloc_load_masks(masks, cli.mask_dir_path.str);

				//
				// Load screenshot.
				//

				printf("BEEP BOOP. CATCHYA INSPECTING \"%s\"...\n", cli.screenshot_file_path.cstr);

				struct BMP screenshot = bmp_alloc_read_file(cli.screenshot_file_path.str);
				if (!(screenshot.dim.x == SCREENSHOT_DIM_X && screenshot.dim.y == SCREENSHOT_DIM_Y))
				{
					error
					(
						"BEEP BOOP. SCREENSHOT \"%s\" IS NOT OF EXPECTED DIMENSIONS: %dx%d.\n",
						cli.screenshot_file_path.cstr,
						SCREENSHOT_DIM_X, SCREENSHOT_DIM_Y
					);
				}

				//
				// Identify word game.
				//

				enum WordGameBoard board = identify_wordgame_board(screenshot, 0);
				if (board == WordGameBoard_COUNT)
				{
					error("BEEP BOOP. UNKNOWN WORD GAME BOARD.\n");
				}

				printf("I KNOW THIS GAME... IT IS %.*s!\n", i32(WORDGAME_BOARD_INFO[board].name.length), WORDGAME_BOARD_INFO[board].name.data);


				//
				// Begin identifying slots.
				//

				for (i32 i = 0; i < WORDGAME_BOARD_INFO[board].dim_slots.x * (LETTER_MAX_NAME_LENGTH + 1) + 1; i += 1)
				{
					printf("-");
				}
				printf("\n");

				for
				(
					i32 slot_coord_y = WORDGAME_BOARD_INFO[board].dim_slots.y - 1;
					slot_coord_y >= 0;
					slot_coord_y -= 1
				)
				{
					//
					// Determine letter of slots in the row.
					//

					printf("|");

					for (i32 slot_coord_x = 0; slot_coord_x < WORDGAME_BOARD_INFO[board].dim_slots.x; slot_coord_x += 1)
					{
						//
						// Determine best letter of this slot.
						//

						enum Letter best_matching_letter = Letter_null;
						i32         best_matching_score  = 0;

						if (!slot_coord_excluded(board, slot_coord_x, slot_coord_y))
						{
							for (enum Letter letter = {1}; letter < Letter_COUNT; letter += 1)
							{
								//
								// Determine amount of differing pixels.
								//

								b32 found_activation = false;
								i32 score            = 0;

								for (i32 mask_y = 0; mask_y < masks[letter].dim.y; mask_y += 1)
								{
									for (i32 mask_x = 0; mask_x < masks[letter].dim.x; mask_x += 1)
									{
										i32_2 base_offset =
											{
												WORDGAME_BOARD_INFO[board].pos.x + slot_coord_x * WORDGAME_BOARD_INFO[board].uncompressed_slot_stride,
												WORDGAME_BOARD_INFO[board].pos.y + slot_coord_y * WORDGAME_BOARD_INFO[board].uncompressed_slot_stride,
											};
										i32_2 slot_pixel_delta =
											{
												i32(f64(mask_x) / f64(masks[letter].dim.x) * WORDGAME_BOARD_INFO[board].slot_dim),
												i32(f64(mask_y) / f64(masks[letter].dim.y) * WORDGAME_BOARD_INFO[board].slot_dim),
											};
										struct BMPPixel pixel = screenshot.data
											[
												(base_offset.y + slot_pixel_delta.y) * screenshot.dim.x
													+ (base_offset.x + slot_pixel_delta.x)
											];
										b32 activation = pixel.r <= MASK_ACTIVATION_THRESHOLD;

										if (activation)
										{
											found_activation = true;
										}

										if ((masks[letter].data[mask_y * masks[letter].dim.x + mask_x].r == 255) == activation)
										{
											score += 1;
										}
									}
								}

								//
								// Keep best.
								//

								if (found_activation && best_matching_score < score)
								{
									best_matching_letter = letter;
									best_matching_score  = score;
								}
							}
						}

						//
						// Report identified letter.
						//

						if (best_matching_letter)
						{
							printf("%*s", LETTER_MAX_NAME_LENGTH, LETTER_NAMES[best_matching_letter].data);
						}
						else
						{
							printf("%*s", LETTER_MAX_NAME_LENGTH, "");
						}

						printf("|");
					}

					//
					// Onto the row below.
					//

					printf("\n");

					for (i32 i = 0; i < WORDGAME_BOARD_INFO[board].dim_slots.x * (LETTER_MAX_NAME_LENGTH + 1) + 1; i += 1)
					{
						printf("-");
					}
					printf("\n");
				}

				//
				// Clean up.
				//

				free(screenshot.data);

				for (i32 i = 0; i < countof(masks); i += 1)
				{
					free(masks[i].data);
				}
			} break;

			case CLIProgram_intelliteck:
			{
				struct CLIProgram_intelliteck_t cli = cli_unknown.intelliteck;
				u16                             crc = 0;

				//
				// Iterate pixels.
				//

				struct BMP bmp = bmp_alloc_read_file(cli.input_file_path.str);

				for
				(
					i32 y = bmp.dim.y - 1;
					y >= 0;
					y -= 1
				)
				{
					for (i32 x = 0; x < bmp.dim.x; x += 1)
					{
						if (bmp.data[y * bmp.dim.x + x].r <= MASK_ACTIVATION_THRESHOLD)
						{
							crc = _crc16_update(crc, (x >> 0) & 0xFF);
							crc = _crc16_update(crc, (x >> 8) & 0xFF);
							crc = _crc16_update(crc, (y >> 0) & 0xFF);
							crc = _crc16_update(crc, (y >> 8) & 0xFF);
						}
					}
				}

				//
				// Output.
				//

				printf("intelliTeck : 0x%4X.\n", u32(crc));

				//
				// Clean up.
				//

				free(bmp.data);
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
