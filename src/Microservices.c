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

static b32
is_mask_compliant(struct BMP bmp)
{
	b32 result = true;

	if (bmp.dim.x != MASK_DIM || bmp.dim.y != MASK_DIM)
	{
		result = false;
	}
	else
	{
		for (i32 i = 0; i < bmp.dim.x * bmp.dim.y; i += 1)
		{
			if
			(
				(0 < bmp.data[i].r && bmp.data[i].r < 255) ||
				(0 < bmp.data[i].g && bmp.data[i].g < 255) ||
				(0 < bmp.data[i].b && bmp.data[i].b < 255) ||
				bmp.data[i].a != 255
			)
			{
				result = false;
				break;
			}
		}
	}

	return result;
}

static void
alloc_load_masks(struct BMP* dst_masks, str dir_path)
{
	dst_masks[0] = (struct BMP) {0};
	for (enum Letter letter = {1}; letter < Letter_COUNT; letter += 1)
	{
		strbuf mask_file_path = strbuf(256);
		strbuf_str (&mask_file_path, dir_path);
		strbuf_char(&mask_file_path, '\\');
		strbuf_str (&mask_file_path, LETTER_INFO[letter].name);
		strbuf_cstr(&mask_file_path, ".bmp");
		dst_masks[letter] = bmp_alloc_read_file(mask_file_path.str);

		if (!is_mask_compliant(dst_masks[letter]))
		{
			error
			(
				"Mask \"%.*s\" has the wrong properties; Must be %dx%d and monochrome.\n",
				i32(mask_file_path.length), mask_file_path.data,
				MASK_DIM, MASK_DIM
			);
		}
	}
}

// WordGame_COUNT              : No matching test region was found.
// WordGame_anagrams_english_6 : Identified as 6-lettered Anagrams, but not necessairly English.
static enum WordGame
identify_wordgame(struct BMP bmp, f64_3* opt_dst_test_region_rgbs) // "opt_dst_test_region_rgbs" : Null or at least WordGame_COUNT; only fully filled out when the BMP could not be identified.
{
	assert(bmp.dim.x == SCREENSHOT_DIM_X && bmp.dim.y == SCREENSHOT_DIM_Y);

	enum WordGame identified_wordgame = WordGame_COUNT;

	if (opt_dst_test_region_rgbs)
	{
		memset(opt_dst_test_region_rgbs, 0, sizeof(f64_3) * WordGame_COUNT);
	}

	for (enum WordGame wordgame = {0}; wordgame < WordGame_COUNT; wordgame += 1)
	{
		if (WORDGAME_INFO[wordgame].test_region_dim.x)
		{
			//
			// Compute average RGB of test region.
			//

			f64_3 avg_rgb = {0};
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
					avg_rgb.x += bmp.data[y * bmp.dim.x + x].r;
					avg_rgb.y += bmp.data[y * bmp.dim.x + x].g;
					avg_rgb.z += bmp.data[y * bmp.dim.x + x].b;
				}
			}
			avg_rgb.x /= 256.0 * WORDGAME_INFO[wordgame].test_region_dim.x * WORDGAME_INFO[wordgame].test_region_dim.y;
			avg_rgb.y /= 256.0 * WORDGAME_INFO[wordgame].test_region_dim.x * WORDGAME_INFO[wordgame].test_region_dim.y;
			avg_rgb.z /= 256.0 * WORDGAME_INFO[wordgame].test_region_dim.x * WORDGAME_INFO[wordgame].test_region_dim.y;

			if (opt_dst_test_region_rgbs)
			{
				opt_dst_test_region_rgbs[wordgame] = avg_rgb;
			}

			//
			// Compare average found with average expected.
			//

			if
			(
				fabs(avg_rgb.x - WORDGAME_INFO[wordgame].test_region_rgb.x) <= TEST_REGION_RGB_EPSILON &&
				fabs(avg_rgb.y - WORDGAME_INFO[wordgame].test_region_rgb.y) <= TEST_REGION_RGB_EPSILON &&
				fabs(avg_rgb.z - WORDGAME_INFO[wordgame].test_region_rgb.z) <= TEST_REGION_RGB_EPSILON
			)
			{
				identified_wordgame = wordgame;
				break;
			}
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

struct DirBMPIterator
{
	str              dir_path;
	struct BMP       bmp;
	HANDLE           finder_handle;
	WIN32_FIND_DATAA finder_data;
	b32              inited;
};
static b32 // Found BMP.
iterate_bmp_in_dir(struct DirBMPIterator* iterator)
{
	b32 found_bmp = false;

	//
	// Initialize.
	//

	if (!iterator->inited)
	{
		strbuf wildcard_path = strbuf(256);
		strbuf_str (&wildcard_path, iterator->dir_path);
		strbuf_cstr(&wildcard_path, "\\*");
		strbuf_char(&wildcard_path, '\0');

		iterator->finder_handle = FindFirstFileA(wildcard_path.data, &iterator->finder_data);
		if (iterator->finder_handle == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_NOT_FOUND)
		{
			error("`FindFirstFileA` failed on \"%s\".", wildcard_path.data);
		}
	}

	//
	// Iterate.
	//

	if (iterator->finder_handle != INVALID_HANDLE_VALUE)
	{
		free(iterator->bmp.data);

		while (true)
		{
			//
			// Find next file.
			//

			if (iterator->inited)
			{
				if (!FindNextFileA(iterator->finder_handle, &iterator->finder_data))
				{
					if (GetLastError() != ERROR_NO_MORE_FILES)
					{
						error("`FindNextFileA` failed.");
					}

					if (!FindClose(iterator->finder_handle))
					{
						error("`FindClose` failed.");
					}

					break;
				}
			}
			else
			{
				iterator->inited = true;
			}

			//
			// Check file is BMP.
			//

			if
			(
				!(iterator->finder_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
				str_ends_with_caseless(str_cstr(iterator->finder_data.cFileName), str(".bmp"))
			)
			{
				strbuf file_path = strbuf(256);
				strbuf_str (&file_path, iterator->dir_path);
				strbuf_char(&file_path, '\\');
				strbuf_cstr(&file_path, iterator->finder_data.cFileName);

				iterator->bmp = bmp_alloc_read_file(file_path.str);

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
				struct CLIProgram_eaglepeek_t cli                             = cli_unknown.eaglepeek;
				i32                           examined_screenshots            = 0;
				i32                           wordgame_counts[WordGame_COUNT] = {0};
				i32                           unknown_wordgames               = 0;
				f64_3                         accumulated_unknown_wordgame_test_region_rgbs[WordGame_COUNT] = {0};

				printf("Hello. My name is Eagle Peek. How can I help you?\n");

				//
				// Iterate through each directory,
				//

				for (i32 input_dir_path_index = 0; input_dir_path_index < cli.input_dir_paths.length; input_dir_path_index += 1)
				{
					CLIFieldTyping_string_t input_dir_path = cli.input_dir_paths.data[input_dir_path_index];

					printf("You want me to go through \"%s\"? Sure thing bud!\n", input_dir_path.cstr);

					//
					// Iterate each screenshot in directory.
					//

					struct DirBMPIterator iterator = { .dir_path = input_dir_path.str };
					while (iterate_bmp_in_dir(&iterator))
					{
						if (iterator.bmp.dim.x == SCREENSHOT_DIM_X && iterator.bmp.dim.y == SCREENSHOT_DIM_Y)
						{
							//
							// Identify wordgame.
							//

							f64_3         unknown_wordgame_test_region_rgbs[WordGame_COUNT] = {0};
							enum WordGame identified_wordgame = identify_wordgame(iterator.bmp, unknown_wordgame_test_region_rgbs);

							//
							// Accumulate result.
							//

							str identified_wordgame_name = {0};

							if (identified_wordgame == WordGame_COUNT)
							{
								identified_wordgame_name  = str("Unknown wordgame");
								unknown_wordgames        += 1;

								for (enum WordGame wordgame = {0}; wordgame < WordGame_COUNT; wordgame += 1)
								{
									accumulated_unknown_wordgame_test_region_rgbs[wordgame].x += unknown_wordgame_test_region_rgbs[wordgame].x;
									accumulated_unknown_wordgame_test_region_rgbs[wordgame].y += unknown_wordgame_test_region_rgbs[wordgame].y;
									accumulated_unknown_wordgame_test_region_rgbs[wordgame].z += unknown_wordgame_test_region_rgbs[wordgame].z;
								}
							}
							else
							{
								if (identified_wordgame == WordGame_anagrams_english_6)
								{
									identified_wordgame_name = str(ANAGRAMS_GENERIC_6_PRINT_NAME);
								}
								else
								{
									identified_wordgame_name = WORDGAME_INFO[identified_wordgame].print_name;
								}
								wordgame_counts[identified_wordgame] += 1;
							}

							//
							// Output the result for this one screenshot.
							//

							examined_screenshots += 1;

							printf
							(
								"[EaglePeek] % 4d : %.*s : \"%s\".\n",
								examined_screenshots,
								i32(identified_wordgame_name.length), identified_wordgame_name.data,
								iterator.finder_data.cFileName
							);
						}
					}

					printf("\n");
				}

				//
				// Output amount of known identified games.
				//

				for (enum WordGame wordgame = {0}; wordgame < WordGame_COUNT; wordgame += 1)
				{
					printf
					(
						"%s%*s : %d\n",
						WORDGAME_INFO[wordgame].print_name_cstr,
						i32(WORDGAME_MAX_PRINT_NAME_LENGTH - WORDGAME_INFO[wordgame].print_name.length), "",
						wordgame_counts[wordgame]
					);
				}

				//
				// Output unknown games.
				//

				if (unknown_wordgames)
				{
					printf
					(
						"\n"
						"Eagle Peek found %d unknown games!\n",
						unknown_wordgames
					);
					for (enum WordGame wordgame = {0}; wordgame < WordGame_COUNT; wordgame += 1)
					{
						printf
						(
							"Average Test Region RGB of %s%*s : Delta(%.4f, %.4f, %.4f) : Found(%.4f, %.4f, %.4f) : Expected(%.4f, %.4f, %.4f) @ (%d, %d) (%dx%d).\n",
							WORDGAME_INFO[wordgame].print_name_cstr,
							i32(WORDGAME_MAX_PRINT_NAME_LENGTH - WORDGAME_INFO[wordgame].print_name.length), "",
							fabs(accumulated_unknown_wordgame_test_region_rgbs[wordgame].x / unknown_wordgames - WORDGAME_INFO[wordgame].test_region_rgb.x),
							fabs(accumulated_unknown_wordgame_test_region_rgbs[wordgame].y / unknown_wordgames - WORDGAME_INFO[wordgame].test_region_rgb.y),
							fabs(accumulated_unknown_wordgame_test_region_rgbs[wordgame].z / unknown_wordgames - WORDGAME_INFO[wordgame].test_region_rgb.z),
							accumulated_unknown_wordgame_test_region_rgbs[wordgame].x / unknown_wordgames,
							accumulated_unknown_wordgame_test_region_rgbs[wordgame].y / unknown_wordgames,
							accumulated_unknown_wordgame_test_region_rgbs[wordgame].z / unknown_wordgames,
							WORDGAME_INFO[wordgame].test_region_rgb.x,
							WORDGAME_INFO[wordgame].test_region_rgb.y,
							WORDGAME_INFO[wordgame].test_region_rgb.z,
							WORDGAME_INFO[wordgame].test_region_pos.x,
							WORDGAME_INFO[wordgame].test_region_pos.y,
							WORDGAME_INFO[wordgame].test_region_dim.x,
							WORDGAME_INFO[wordgame].test_region_dim.y
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
				i32                             examined_screenshots = 0;
				i32                             unknown_wordgames    = 0;
				i32                             extracted_slots      = 0;

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

					struct DirBMPIterator iterator = { .dir_path = input_dir_path.str };
					while (iterate_bmp_in_dir(&iterator))
					{
						if (iterator.bmp.dim.x == SCREENSHOT_DIM_X && iterator.bmp.dim.y == SCREENSHOT_DIM_Y)
						{
							//
							// Determine wordgame.
							//

							enum WordGame wordgame = identify_wordgame(iterator.bmp, 0);
							if (wordgame != WordGame_COUNT)
							{
								//
								// Create a BMP of each slot.
								//

								for (i32 slot_coord_y = 0; slot_coord_y < WORDGAME_INFO[wordgame].dim_slots.y; slot_coord_y += 1)
								{
									for (i32 slot_coord_x = 0; slot_coord_x < WORDGAME_INFO[wordgame].dim_slots.x; slot_coord_x += 1)
									{
										if (!is_slot_excluded(wordgame, u8(slot_coord_x), u8(slot_coord_y)))
										{
											b8 found_activated_pixel = false;

											//
											// Compress and monochromize the slot.
											//

											for (i32 compressed_slot_px_y = 0; compressed_slot_px_y < MASK_DIM; compressed_slot_px_y += 1)
											{
												for (i32 compressed_slot_px_x = 0; compressed_slot_px_x < MASK_DIM; compressed_slot_px_x += 1)
												{
													i32_2 uncompressed_bmp_dim =
														{
															i32((WORDGAME_INFO[wordgame].dim_slots.x - 1) * WORDGAME_INFO[wordgame].uncompressed_slot_stride + WORDGAME_INFO[wordgame].slot_dim),
															i32((WORDGAME_INFO[wordgame].dim_slots.y - 1) * WORDGAME_INFO[wordgame].uncompressed_slot_stride + WORDGAME_INFO[wordgame].slot_dim),
														};
													i32_2 compressed_slot_abs_px_pos =
														{
															slot_coord_x * WORDGAME_INFO[wordgame].compressed_slot_stride + compressed_slot_px_x,
															slot_coord_y * WORDGAME_INFO[wordgame].compressed_slot_stride + compressed_slot_px_y,
														};
													i32_2 compressed_game =
														{
															(WORDGAME_INFO[wordgame].dim_slots.x - 1) * WORDGAME_INFO[wordgame].compressed_slot_stride + MASK_DIM,
															(WORDGAME_INFO[wordgame].dim_slots.y - 1) * WORDGAME_INFO[wordgame].compressed_slot_stride + MASK_DIM,
														};
													f64_2 uv =
														{
															f64(compressed_slot_abs_px_pos.x) / f64(compressed_game.x),
															f64(compressed_slot_abs_px_pos.y) / f64(compressed_game.y),
														};
													struct BMPPixel pixel =
														iterator.bmp.data
														[
															( WORDGAME_INFO[wordgame].pos.y + i32(uv.y * uncompressed_bmp_dim.y)) * iterator.bmp.dim.x
															+ WORDGAME_INFO[wordgame].pos.x + i32(uv.x * uncompressed_bmp_dim.x)
														];

													u8 value = 0;
													if (pixel.r <= MASK_ACTIVATION_THRESHOLD)
													{
														found_activated_pixel = true;
														value                 = 255;
													}
													compressed_slot.data[compressed_slot_px_y * compressed_slot.dim.x + compressed_slot_px_x] = (struct BMPPixel) { value, value, value, 255 };
												}
											}

											//
											// Output BMP of slot if there was at least one activated pixel.
											//

											if (found_activated_pixel)
											{
												strbuf slot_file_path = strbuf(256);
												strbuf_str (&slot_file_path, cli.output_dir_path.str);
												strbuf_char(&slot_file_path, '\\');
												strbuf_cstr(&slot_file_path, (char*) WORDGAME_INFO[wordgame].print_name_cstr);
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
												strbuf_cstr(&slot_file_path, iterator.finder_data.cFileName);
												bmp_export(compressed_slot, slot_file_path.str);

												extracted_slots += 1;
											}
										}
									}
								}
							}

							//
							// Print details about the screenshot.
							//

							str wordgame_name = {0};
							if (wordgame == WordGame_COUNT)
							{
								wordgame_name      = str("Unknown wordgame");
								unknown_wordgames += 1;
							}
							else if (wordgame == WordGame_anagrams_english_6)
							{
								wordgame_name = str(ANAGRAMS_GENERIC_6_PRINT_NAME);
							}
							else
							{
								wordgame_name = WORDGAME_INFO[wordgame].print_name;
							}

							examined_screenshots += 1;

							printf
							(
								"[extractorv2] % 4d : %.*s : \"%s\".\n",
								examined_screenshots,
								i32(wordgame_name.length), wordgame_name.data,
								iterator.finder_data.cFileName
							);
						}
					}

					printf("\n");
				}

				//
				// Output extraction results.
				//

				if (examined_screenshots)
				{
					printf
					(
						"EXTRACTOR EXTRACTED FROM %lld DIRECTORIES!\n"
						"EXTRACTOR EXTRACTED FROM %d SCREENSHOTS!\n"
						"EXTRACTOR EXTRACTED %d SLOTS!!\n"
						"\n",
						cli.input_dir_paths.length,
						examined_screenshots,
						extracted_slots
					);

					if (unknown_wordgames)
					{
						printf
						(
							"BUT.............. THERE WERE %d UNKNOWN WORDGAMES??!!?!?!?!\n"
							"\n",
							unknown_wordgames
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
				i32                            processed_images                    = 0;
				i32                            matched_letter_counts[Letter_COUNT] = {0};
				i32*                           accumulated_mask_pixels             = 0;
				alloc(&accumulated_mask_pixels, MASK_DIM * MASK_DIM * Letter_COUNT);

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
					strbuf letter_dir_path = strbuf(256);
					strbuf_str (&letter_dir_path, cli.output_dir_path.str);
					strbuf_char(&letter_dir_path, '\\');
					strbuf_str (&letter_dir_path, LETTER_INFO[letter].name);
					create_dir(letter_dir_path.str, cli.clear_output_dir);
				}

				//
				// Iterate through each image that needs to be sorted.
				//

				struct DirBMPIterator iterator = { .dir_path = cli.unsorted_dir_path.str };
				while (iterate_bmp_in_dir(&iterator))
				{
					if (!is_mask_compliant(iterator.bmp))
					{
						error("BMP found to have the wrong properties; Must be %dx%d and monochrome.\n", MASK_DIM, MASK_DIM);
					}

					//
					// Determine best matching letter.
					//

					enum Letter best_matching_letter = {0};
					i32         best_matching_score  = 0;

					for (enum Letter letter = {1}; letter < Letter_COUNT; letter += 1)
					{
						i32 score = 0;
						for (i32 i = 0; i < MASK_DIM * MASK_DIM; i += 1)
						{
							score += (~(iterator.bmp.data[i].r ^ masks[letter].data[i].r)) & 1;
						}

						if (best_matching_score < score)
						{
							best_matching_letter = letter;
							best_matching_score  = score;
						}
					}

					assert(best_matching_letter);

					//
					// Add pixels to the accumulation.
					//

					matched_letter_counts[best_matching_letter] += 1;
					for (i32 i = 0; i < MASK_DIM * MASK_DIM; i += 1)
					{
						accumulated_mask_pixels[MASK_DIM * MASK_DIM * best_matching_letter + i] += iterator.bmp.data[i].r & 1;
					}

					//
					// Copy BMP into the appropriate subfolder of the letter.
					//

					strbuf file_path = strbuf(256);
					strbuf_str (&file_path, cli.output_dir_path.str);
					strbuf_char(&file_path, '\\');
					strbuf_str (&file_path, LETTER_INFO[best_matching_letter].name);
					strbuf_char(&file_path, '\\');
					strbuf_cstr(&file_path, iterator.finder_data.cFileName);
					bmp_export(iterator.bmp, file_path.str);

					processed_images += 1;

					printf("[CollecTune(TM)] % 4d : \"%s\".\n", processed_images, iterator.finder_data.cFileName);
				}

				//
				// Output new average mask of each letter.
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
						new_avg_mask.data[i] =
							accumulated_mask_pixels[MASK_DIM * MASK_DIM * letter + i] > matched_letter_counts[letter] / 2
								? (struct BMPPixel) { 255, 255, 255, 255 }
								: (struct BMPPixel) { 0, 0, 0, 255 };
					}

					strbuf file_path = strbuf(256);
					strbuf_str (&file_path, cli.output_dir_path.str);
					strbuf_char(&file_path, '\\');
					strbuf_str (&file_path, LETTER_INFO[letter].name);
					strbuf_cstr(&file_path, ".bmp");
					bmp_export(new_avg_mask, file_path.str);
				}

				//
				// Output sorting results.
				//

				if (processed_images)
				{
					printf
					(
						"\n"
						"Collectune'd %d images~~ :D\n"
						"\n",
						processed_images
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
				free(accumulated_mask_pixels);
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
				strbuf output_buf      = strbuf(256);

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

				strbuf_cstr(&output_buf, "\n");
				strbuf_cstr(&output_buf, "// ");
				strbuf_u64 (&output_buf, run_count * sizeof(u8));
				strbuf_cstr(&output_buf, " bytes for byte-sized run-lengths.\n");
				strbuf_cstr(&output_buf, "// ");
				strbuf_u64 (&output_buf, overflowed_runlengths.length * sizeof(u16));
				strbuf_cstr(&output_buf, " bytes for word-sized run-lengths.\n");
				strbuf_cstr(&output_buf, "// ");
				strbuf_u64 (&output_buf, run_count * sizeof(u8) + overflowed_runlengths.length * sizeof(u16));
				strbuf_cstr(&output_buf, " bytes total.\n");
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

			case CLIProgram_wordy:
			{
				struct CLIProgram_wordy_t cli = cli_unknown.wordy;

				//
				// Process each language's dictionary file.
				//

				for (enum Language language = {0}; language < Language_COUNT; language += 1)
				//enum Language language = Language_russian;
				{
					struct Dary_u32 wordset[MAX_WORD_LENGTH - MIN_WORD_LENGTH + 1][Letter_COUNT] = {0};

					strbuf dictionary_file_path = strbuf(256);
					strbuf_str (&dictionary_file_path, cli.dir_path.str);
					strbuf_char(&dictionary_file_path, '/');
					strbuf_str (&dictionary_file_path, LANGUAGE_INFO[language].name);
					strbuf_cstr(&dictionary_file_path, ".txt");
					str dictionary_file_content = alloc_read_file(dictionary_file_path.str);

					strbuf output_file_path = strbuf(256);
					strbuf_str (&output_file_path, cli.dir_path.str);
					strbuf_char(&output_file_path, '/');
					strbuf_str (&output_file_path, LANGUAGE_INFO[language].name);
					strbuf_cstr(&output_file_path, ".dat");
					HANDLE output_handle = create_file_writing_handle(output_file_path.str);
					strbuf output_buf    = strbuf(256);

					//
					// Parse dictionary for words.
					//

					i64 processed_amount = 0;
					while (processed_amount < dictionary_file_content.length)
					{
						//
						// Get word's letters.
						//

						enum Letter parsed_word_buffer[MAX_WORD_LENGTH] = {0};
						i32         parsed_word_length                  = 0;
						b32         has_blacklisted_codepoint           = false;

						while (true)
						{
							if (processed_amount == dictionary_file_content.length)
							{
								break;
							}
							else if (dictionary_file_content.data[processed_amount] == '\n')
							{
								processed_amount += 1;
								break;
							}
							else if
							(
								processed_amount + 1 < dictionary_file_content.length &&
								dictionary_file_content.data[processed_amount    ] == '\r' &&
								dictionary_file_content.data[processed_amount + 1] == '\n'
							)
							{
								processed_amount += 2;
								break;
							}
							else
							{
								//
								// Get codepoint.
								//

								u8 codepoint_length = 0;
								u8 initial_mask     = 0;

								if ((dictionary_file_content.data[processed_amount] & 0b1000'0000) == 0b0000'0000)
								{
									codepoint_length = 1;
									initial_mask     = 0b0111'1111;
								}
								else if ((dictionary_file_content.data[processed_amount] & 0b1110'0000) == 0b1100'0000)
								{
									codepoint_length = 2;
									initial_mask     = 0b0001'1111;
								}
								else if ((dictionary_file_content.data[processed_amount] & 0b1111'0000) == 0b1110'0000)
								{
									codepoint_length = 3;
									initial_mask     = 0b0000'1111;
								}
								else if ((dictionary_file_content.data[processed_amount] & 0b1111'1000) == 0b1111'0000)
								{
									codepoint_length = 4;
									initial_mask     = 0b0000'0111;
								}
								else
								{
									error("Unknown unicode byte 0x%2X.", (u32) (u8) dictionary_file_content.data[processed_amount]);
								}

								if (processed_amount + codepoint_length > dictionary_file_content.length)
								{
									error("Not enough bytes remaining for last UTF-8 codepoint.");
								}

								u32 codepoint = dictionary_file_content.data[processed_amount] & initial_mask;
								processed_amount += 1;
								for
								(
									i32 byte_index = 1;
									byte_index < codepoint_length;
									byte_index += 1
								)
								{
									codepoint        <<= 6;
									codepoint         |= dictionary_file_content.data[processed_amount] & 0b0011'1111;
									processed_amount  += 1;
								}

								//
								// Determine letter of codepoint.
								//

								enum Letter codepoint_letter = {0};

								for
								(
									i32 alphabet_index = 0;
									alphabet_index < LANGUAGE_INFO[language].alphabet_length && !codepoint_letter;
									alphabet_index += 1
								)
								{
									enum Letter letter = LANGUAGE_INFO[language].alphabet[alphabet_index];
									for
									(
										i32 letter_codepoint_index = 0;
										letter_codepoint_index < countof(LETTER_INFO[letter].codepoints) && LETTER_INFO[letter].codepoints[letter_codepoint_index];
										letter_codepoint_index += 1
									)
									{
										if (LETTER_INFO[letter].codepoints[letter_codepoint_index] == codepoint)
										{
											codepoint_letter = letter;
											break;
										}
									}
								}

								//
								// Determine if letter is blacklisted.
								//

								if (!codepoint_letter)
								{
									b32 blacklisted = false;
									for (i32 i = 0; i < countof(BLACKLISTED_CODEPOINTS); i += 1)
									{
										if (BLACKLISTED_CODEPOINTS[i] == codepoint)
										{
											blacklisted               = true;
											has_blacklisted_codepoint = true;
											break;
										}
									}
									if (!blacklisted)
									{
										//error("Unhandled codepoint 0x%X.", codepoint);
										printf("Unhandled codepoint 0x%X.\n", codepoint);
									}
								}

								//
								// Append letter to word buffer.
								//

								if (parsed_word_length < countof(parsed_word_buffer))
								{
									parsed_word_buffer[parsed_word_length] = codepoint_letter;
								}
								parsed_word_length += 1;
							}
						}

						//
						// Process word.
						//

						if (!has_blacklisted_codepoint && MIN_WORD_LENGTH <= parsed_word_length && parsed_word_length <= countof(parsed_word_buffer))
						{
							static_assert(MIN_WORD_LENGTH > 0);

							b32 skip     = false;
							u32 subwords = 0;

							//
							// Search for parenting word.
							//

							for
							(
								i32 parenting_word_length = MAX_WORD_LENGTH;
								parenting_word_length > parsed_word_length;
								parenting_word_length -= 1
							)
							{
								assert(wordset[MAX_WORD_LENGTH - parenting_word_length][parsed_word_buffer[0]].length % parenting_word_length == 0);
								for
								(
									i32 parenting_word_index = 0;
									parenting_word_index < wordset[MAX_WORD_LENGTH - parenting_word_length][parsed_word_buffer[0]].length / parenting_word_length;
									parenting_word_index += 1
								)
								{
									u32* parenting_word = wordset[MAX_WORD_LENGTH - parenting_word_length][parsed_word_buffer[0]].data + parenting_word_index * parenting_word_length;
									if
									(
										!memcmp
										(
											parenting_word + 1,
											parsed_word_buffer + 1,
											(parsed_word_length - 1) * sizeof(parsed_word_buffer[0])
										)
									)
									{
										skip               = true;
										parenting_word[0] |= 1 << (parsed_word_length - MIN_WORD_LENGTH);
									}
								}
							}

							//
							// Search for duplicate.
							//

							if (!skip)
							{
								assert(wordset[MAX_WORD_LENGTH - parsed_word_length][parsed_word_buffer[0]].length % parsed_word_length == 0);
								for
								(
									i32 duplicate_word_index = 0;
									duplicate_word_index < wordset[MAX_WORD_LENGTH - parsed_word_length][parsed_word_buffer[0]].length / parsed_word_length;
									duplicate_word_index += 1
								)
								{
									u32* duplicate_word = wordset[MAX_WORD_LENGTH - parsed_word_length][parsed_word_buffer[0]].data + duplicate_word_index * parsed_word_length;
									if
									(
										!memcmp
										(
											duplicate_word + 1,
											parsed_word_buffer + 1,
											(parsed_word_length - 1) * sizeof(parsed_word_buffer[0])
										)
									)
									{
										skip = true;
									}
								}
							}

							//
							// Search for children.
							//

							for
							(
								i32 child_word_length = parsed_word_length - 1;
								child_word_length >= MIN_WORD_LENGTH;
								child_word_length -= 1
							)
							{
								assert(wordset[MAX_WORD_LENGTH - child_word_length][parsed_word_buffer[0]].length % child_word_length == 0);

								i32 child_word_index = 0;
								while (child_word_index < wordset[MAX_WORD_LENGTH - child_word_length][parsed_word_buffer[0]].length / child_word_length)
								{
									u32* child_word = wordset[MAX_WORD_LENGTH - child_word_length][parsed_word_buffer[0]].data + child_word_index * child_word_length;
									if
									(
										!memcmp
										(
											parsed_word_buffer + 1,
											child_word + 1,
											(child_word_length - 1) * sizeof(child_word[0])
										)
									)
									{
										memmove
										(
											child_word,
											wordset[MAX_WORD_LENGTH - child_word_length][parsed_word_buffer[0]].data + wordset[MAX_WORD_LENGTH - child_word_length][parsed_word_buffer[0]].length - child_word_length,
											child_word_length * sizeof(child_word[0])
										);
										wordset[MAX_WORD_LENGTH - child_word_length][parsed_word_buffer[0]].length -= child_word_length;
										subwords                                                                   |= 1 << (child_word_length - MIN_WORD_LENGTH);
									}
									else
									{
										child_word_index += 1;
									}
								}
							}

							//
							// Add word to set.
							//

							if (!skip)
							{
								static_assert(sizeof(parsed_word_buffer[0]) == sizeof(wordset[0][0].data[0]));
								dary_push(&wordset[MAX_WORD_LENGTH - parsed_word_length][parsed_word_buffer[0]], &subwords);
								dary_push_n
								(
									&wordset[MAX_WORD_LENGTH - parsed_word_length][parsed_word_buffer[0]],
									(u32*) (parsed_word_buffer + 1),
									parsed_word_length - 1
								);
							}
						}
					}

					//
					//
					//

					for
					(
						i32 word_length = MAX_WORD_LENGTH;
						word_length >= MIN_WORD_LENGTH;
						word_length -= 1
					)
					{
						for (i32 alphabet_index = 0; alphabet_index < LANGUAGE_INFO[language].alphabet_length; alphabet_index += 1)
						{
							enum Letter word_initial = LANGUAGE_INFO[language].alphabet[alphabet_index];

							for
							(
								i32 word_index = 0;
								word_index < wordset[MAX_WORD_LENGTH - word_length][word_initial].length / word_length;
								word_index += 1
							)
							{
								u32* word = wordset[MAX_WORD_LENGTH - word_length][word_initial].data + word_index * word_length;

								printf("%.*s", i32(LETTER_INFO[word_initial].name.length), LETTER_INFO[word_initial].name.data);
								for
								(
									i32 i = 1;
									i < word_length;
									i += 1
								)
								{
									printf(" %.*s", i32(LETTER_INFO[word[i]].name.length), LETTER_INFO[word[i]].name.data);
								}
								printf("\n");

								for
								(
									i32 subword_length = MIN_WORD_LENGTH;
									subword_length < word_length;
									subword_length += 1
								)
								{
									if (word[0] & (1 << (subword_length - MIN_WORD_LENGTH)))
									{
										printf("\t- %.*s", i32(LETTER_INFO[word_initial].name.length), LETTER_INFO[word_initial].name.data);
										for
										(
											i32 i = 1;
											i < subword_length;
											i += 1
										)
										{
											printf(" %.*s", i32(LETTER_INFO[word[i]].name.length), LETTER_INFO[word[i]].name.data);
										}
										printf("\n");
									}
								}
							}
						}
					}

					//
					// Clean up.
					//

					debug_halt();

					for (i32 word_length = 0; word_length < countof(wordset); word_length += 1)
					{
						for (i32 word_initial = 0; word_initial < LANGUAGE_INFO[language].alphabet_length; word_initial += 1)
						{
							free(wordset[word_length][word_initial].data);
						}
					}

					free(dictionary_file_content.data);
					close_file_writing_handle(output_handle);
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
	This program is essentially an accumulation of small scripts (which I call microservices) with
	the purpose of data processing. Admittedly I probably should've wrote it in something like
	Python, but hey, at least all the configuration values are kept in defs.h and changing it is
	pretty trivial. This part of the project is also not the cleanest or most efficient, I'll
	confess, but it got the job done!

	For what exactly this program achieves, see [Mask Generation].
*/

/* [Mask Generation].
	The Diplomat will need mask data in order to identify the closest matching letter when
	performing OCR. To be able to generate this mask data, we first collect 18.2 GB of uncompressed
	BMP screenshots of each Game Pigeon game! This will essentially be our "training data", as the
	fancy AI people will call it.

	Now this part is kinda tricky, because what exactly are the bounding boxes of the slots in the
	word games? We want it to be as tight as possible, so that bounding box will contain the most
	amount of information it can about what letter that slot could contain, but if it's too
	small, or too off centered, then we risk misidentifying. So what ended up happening is that I
	first took a guess at the bounding box, use that to extract each slot of the screenshot BMP,
	and then adjusted the bounding box again to make sure no letters were getting cropped or
	getting too off-centered. In the end, I don't think it really matters all that much if the
	bounding box, say, cropped the letter 'A' slightly, but better safe than sorry!

	Once we determine the bounding box of a slot in a wordgame, we then determine the bounding box
	of the entire wordgame board. A board is just the area where all the letters will be found in,
	since luckily all the Game Pigeon wordgames can be described as uniform grids of slots,
	with some excluded slots here and there.

	With the bounding box of the wordgame board, we then crop the screenshot BMP with that and
	scale it down so each slot will now all have the same dimensions as defined by MASK_DIM. This
	is a necessary step since each wordgame have a different sizes for the slots, where Anagrams
	have the letters at their largest and WordBites have it at their smallest. I initially assumed
	this size difference was negligible, but it just made the OCR really sucky and unreliable, so
	resizing was pretty much mandatory.

	From this cropped and resized BMP of the screenshot, we can then actually extract out the
	individual slots that contains a letter, and while at it, turn the colored slot BMP into a
	monochrome one. Since all the letters are black surrounded by wood brown, having full RGB
	color space for mask data isn't really necessary. We achieve this by applying a threshold
	level (MASK_ACTIVATION_THRESHOLD) on the red channel of pixels, as this happens to be the most
	convenient implementation-wise.

	This is entire process is essentially what extractorv2 does, and also what the Shortcuts
	program on the phone will also do before sending the image to the Diplomat. In short, the
	entire process is this:

		1. Crop screenshot BMP to bounding box of the wordgame's board.
		2. Scale the cropped BMP down so that each slot's bounding box is now MASK_DIM x MASK_DIM.
		3. Convert RGBA pixels into monochrome based on threshold of red channel.
		4. Export each slot.

	We can ensure the extraction went well by comparing the BMPs of the same letters. For example,
	if the 'A's in Anagrams are all noticeably shifting a lot between each other, then we need to
	reconfigure the slot stride on that board until it's negligible. Once they're consistent, we
	then make sure all Anagrams 'A's are lined up with all the other 'A's from the other games.
	Doing this makes sure we reduce the amount of error of the correctly identified letter for the
	OCR process.

	But of course, this is insanely tedious to do when you have almost 20K of BMPs to sift through!
	Thus we'll need to go to the next microservice: collectunev2.

	Collectunev2 first needs a mask of each letter we have defined. The Game Pigeon wordgames are
	mostly English, using the traditional Latin alphabet of 'A' thru 'Z'. Anagrams, howoever, have
	other languages, with Russian being particularly the most "unique" as it uses the Cyrillic
	alphabet. Now, I don't know a lot of Russian (as in none) but it appears that some letters are
	just not used at all. I've collected gigabytes of screenshots of Russian Anagrams and not once
	a particular character appeared. Some are so rare that I literally only have one screenshot of
	it! This applies to German too, and even in English where 'Q' doesn't seem to appear at all, but
	does for the 7-letter version. Strange. I highly suspect that Game Pigeon is doing some weird
	RNG stuff to get a good set of letters that'll be enjoyable to play.

	Anyways, we sift through the huge pile of unsorted monochrome slot BMPs that extractorv2
	generated and find a BMP of each letter. These BMPs will essentially act as the
	"first generation" masks that collectunev2 will be using to sort the extracted slots. After
	sorting, the next generation of masks are generated, representing the average letter. For
	example, all the BMPs that collectunev2 identified as 'A' will be put into the 'A' folder, and
	when all BMPs have been sorted, a new 'A' mask will be created based on the average of all the
	'A' BMPs in the 'A' folder. This process should pretty be much 100% accurate we have configured
	the bounding box and strides of extractorv2 well, but of course it's important to check each
	directory of the letters and ensure nothing was misidentified. This is especially important to
	do for letters like 'F' and 'E' or 'P' and 'B' where the next generation of masks may not make
	this misidentification obvious.

	Alright, once we have what appears to be pretty good monochrome MASK_DIM x MASK_DIM BMPs
	representing each letter, we have to compress it into a format that won't gobble up all the
	flash of the Diplomat. As explained thoroughly in usb.c, the compression that is done is
	essentially run-length encoding that begins on the top-left corner of the first letter, to
	the top-right corner of the same letter, and then going to the next letter, and continuing to
	the very last letter. Once at the very last letter, the run-length of pixels then start from
	the second row from the top now at the first letter again, and then repeat until we reach the
	bottom-right pixel of the very last letter. Doing the run-length encoding scheme like this
	conforms to how the OCR will actually perform the data-access better than when the compression
	is done through a letter-by-letter basis.

	And that's it! Somethings could be improved and made less sloppy, I'll agree with that. But
	overall the data-processing of this should essentially only be done "once", so it doesn't
	matter if things are a bit slow.
*/
