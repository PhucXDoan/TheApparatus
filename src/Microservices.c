#pragma warning(push)
#pragma warning(disable : 4255)
#define _CRT_SECURE_NO_DEPRECATE 1
#include <Windows.h>
#include <shlwapi.h>
#include <dbghelp.h>
#include <intrin.h>
#pragma warning(pop)
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <conio.h>
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
				(i32) mask_file_path.length, mask_file_path.data,
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

	printf("%.*s", (i32) pattern.length, pattern.data);
	result += (i32) pattern.length;

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
		(i32) exe_name.length, exe_name.data,
		(i32) program_info.name.length, program_info.name.data
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
		(i32) program_info.name.length, program_info.name.data,
		(i32) (margin - program_info.name.length), "",
		(i32) program_info.desc.length, program_info.desc.data
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
			(i32) (margin - cli_field_length), "",
			(i32) field_info.desc.length, field_info.desc.data
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

			printf("%.*s [program] ...", (i32) exe_name.length, exe_name.data);
			printf("\n");

			//
			// Executable description.
			//

			printf
			(
				"\t%.*s%*s| %s\n",
				(i32) exe_name.length, exe_name.data,
				(i32) (margin - exe_name.length), "",
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
					(i32) program_info.name.length, program_info.name.data,
					(i32) (margin - program_info.name.length), "",
					(i32) program_info.desc.length, program_info.desc.data
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
					(i32) cli_field_info.pattern.length, cli_field_info.pattern.data
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
								(i32) identified_wordgame_name.length, identified_wordgame_name.data,
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
						(i32) (WORDGAME_MAX_PRINT_NAME_LENGTH - WORDGAME_INFO[wordgame].print_name.length), "",
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
							(i32) (WORDGAME_MAX_PRINT_NAME_LENGTH - WORDGAME_INFO[wordgame].print_name.length), "",
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

			case CLIProgram_extractorv2: // See: [Mask Generation].
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
										if (!is_slot_excluded(wordgame, (u8) slot_coord_x, (u8) slot_coord_y))
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
															(i32) ((WORDGAME_INFO[wordgame].dim_slots.x - 1) * WORDGAME_INFO[wordgame].uncompressed_slot_stride + WORDGAME_INFO[wordgame].slot_dim),
															(i32) ((WORDGAME_INFO[wordgame].dim_slots.y - 1) * WORDGAME_INFO[wordgame].uncompressed_slot_stride + WORDGAME_INFO[wordgame].slot_dim),
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
															(f64) compressed_slot_abs_px_pos.x / compressed_game.x,
															(f64) compressed_slot_abs_px_pos.y / compressed_game.y,
														};
													struct BMPPixel pixel =
														iterator.bmp.data
														[
															( WORDGAME_INFO[wordgame].pos.y + (i32) (uv.y * uncompressed_bmp_dim.y)) * iterator.bmp.dim.x
															+ WORDGAME_INFO[wordgame].pos.x + (i32) (uv.x * uncompressed_bmp_dim.x)
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
								(i32) wordgame_name.length, wordgame_name.data,
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

			case CLIProgram_collectune: // See: [Mask Generation].
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
						"No images were collectune'd! :(\n"
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

			case CLIProgram_maskiversev2: // See: [Mask Generation].
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
							if (runlength > (u16) -1)
							{
								error("Run length too long!");
							}

							dary_push(&overflowed_runlengths, (u16[]) { (u16) runlength });
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

			case CLIProgram_wordy: // See: [Word Compression].
			{
				struct CLIProgram_wordy_t cli         = cli_unknown.wordy;
				i32                       total_flash = 0;
				u64                       stream_size = 0;

				strbuf c_source_file_path = strbuf(256);
				strbuf_str (&c_source_file_path, cli.dir_path.str);
				strbuf_cstr(&c_source_file_path, "/words_table_of_contents.h");
				HANDLE c_source_handle = create_file_writing_handle(c_source_file_path.str);
				strbuf output_buf      = strbuf(256);

				strbuf stream_file_path = strbuf(256);
				strbuf_str (&stream_file_path, cli.dir_path.str);
				strbuf_cstr(&stream_file_path, WORD_STREAM_NAME);
				strbuf_char(&stream_file_path, '.');
				strbuf_cstr(&stream_file_path, WORD_STREAM_EXTENSION);
				HANDLE stream_handle = create_file_writing_handle(stream_file_path.str);

				//
				// Process each language's dictionary file.
				//

				for (enum Language language = {0}; language < Language_COUNT; language += 1)
				{
					i32 max_word_length = 0;
					for (i32 i = 0; i < countof(WORDGAME_INFO); i += 1)
					{
						if (WORDGAME_INFO[i].language == language && max_word_length < WORDGAME_INFO[i].max_word_length)
						{
							max_word_length = WORDGAME_INFO[i].max_word_length;
						}
					}

					//
					// Generate wordsets consisting of word-makeups with first u16 being the subword bitfield.
					//

					static_assert(MIN_WORD_LENGTH > 0); // Zero-lengthed words doesn't make sense!

					struct Dary_u16 wordsets[ABSOLUTE_MAX_WORD_LENGTH - MIN_WORD_LENGTH + 1][MAX_ALPHABET_LENGTH] = {0};
					i32             word_makeup_count = 0;
					{
						strbuf dictionary_file_path = strbuf(256);
						strbuf_str (&dictionary_file_path, cli.dir_path.str);
						strbuf_char(&dictionary_file_path, '/');
						strbuf_str (&dictionary_file_path, LANGUAGE_INFO[language].name);
						strbuf_cstr(&dictionary_file_path, ".txt");
						str   stream = alloc_read_file(dictionary_file_path.str);
						char* reader = stream.data;

						printf("Processing \"%.*s\"...\n", (i32) dictionary_file_path.length, dictionary_file_path.data);

						while (reader < stream.data + stream.length)
						{
							//
							// Get word's letters.
							//

							u16 parsed_word_buffer[ABSOLUTE_MAX_WORD_LENGTH] = {0};
							i32 parsed_word_length                           = 0;
							b32 parsed_word_should_be_ignored                = false;

							while (true)
							{
								if (reader == stream.data + stream.length)
								{
									break;
								}
								else if (reader[0] == '\n')
								{
									reader += 1;
									break;
								}
								else if (reader + 1 < stream.data + stream.length && reader[0] == '\r' && reader[1] == '\n')
								{
									reader += 2;
									break;
								}
								else
								{
									//
									// Determine codepoint length.
									//

									i32 codepoint_bytes             = 0;
									u8  codepoint_initial_byte_mask = 0;

									if ((reader[0] & 0b1000'0000) == 0b0000'0000)
									{
										codepoint_bytes             = 1;
										codepoint_initial_byte_mask = 0b0111'1111;
									}
									else if ((reader[0] & 0b1110'0000) == 0b1100'0000)
									{
										codepoint_bytes             = 2;
										codepoint_initial_byte_mask = 0b0001'1111;
									}
									else if ((reader[0] & 0b1111'0000) == 0b1110'0000)
									{
										codepoint_bytes             = 3;
										codepoint_initial_byte_mask = 0b0000'1111;
									}
									else if ((reader[0] & 0b1111'1000) == 0b1111'0000)
									{
										codepoint_bytes             = 4;
										codepoint_initial_byte_mask = 0b0000'0111;
									}
									else
									{
										error("Unknown unicode byte 0x%2X.", (u32) (u8) reader[0]);
									}
									if (reader + codepoint_bytes > stream.data + stream.length)
									{
										error("Not enough bytes remaining for last UTF-8 codepoint.");
									}

									//
									// Get codepoint value.
									//

									u32 codepoint = reader[0] & codepoint_initial_byte_mask;
									reader += 1;

									for
									(
										i32 byte_index = 1;
										byte_index < codepoint_bytes;
										byte_index += 1
									)
									{
										if ((reader[0] & 0b1100'0000) != 0b1000'0000)
										{
											error("Expected continuation byte.");
										}

										codepoint <<= 6;
										codepoint  |= reader[0] & 0b0011'1111;
										reader     += 1;
									}

									//
									// Determine alphabet index of codepoint.
									//

									i32 codepoint_alphabet_index = -1;

									for (i32 alphabet_index = 0; alphabet_index < LANGUAGE_INFO[language].alphabet_length; alphabet_index += 1)
									{
										for (i32 alpahbet_codepoint_index = 0; alpahbet_codepoint_index < countof(LETTER_INFO[0].codepoints); alpahbet_codepoint_index += 1)
										{
											u32 alphabet_codepoint = LETTER_INFO[LANGUAGE_INFO[language].alphabet[alphabet_index]].codepoints[alpahbet_codepoint_index];

											if (!alphabet_codepoint)
											{
												break;
											}
											else if (alphabet_codepoint == codepoint)
											{
												codepoint_alphabet_index = alphabet_index;
												break;
											}
										}
										if (codepoint_alphabet_index != -1)
										{
											break;
										}
									}

									//
									// Determine if codepoint is blacklisted or unknown.
									//

									if (codepoint_alphabet_index == -1)
									{
										parsed_word_should_be_ignored = true;

										//
										// Is the codepoint blacklisted?
										//

										for (i32 i = 0; i < countof(BLACKLISTED_CODEPOINTS); i += 1)
										{
											if (BLACKLISTED_CODEPOINTS[i] == codepoint)
											{
												goto NOT_UNKNOWN;
											}
										}

										//
										// Does a letter have this codepoint?
										//

										for (enum Letter letter = {0}; letter < Letter_COUNT; letter += 1)
										{
											for (i32 letter_codepoint_index = 0; letter_codepoint_index < countof(LETTER_INFO[letter].codepoints); letter_codepoint_index += 1)
											{
												u32 letter_codepoint = LETTER_INFO[letter].codepoints[letter_codepoint_index];

												if (!letter_codepoint)
												{
													break;
												}
												else if (letter_codepoint == codepoint)
												{
													goto NOT_UNKNOWN;
												}
											}
										}

										printf("\tUnknown codepoint 0x%X!\n", codepoint);

										NOT_UNKNOWN:;
									}
									else // Append alphabet index to word buffer.
									{
										if (parsed_word_length < countof(parsed_word_buffer))
										{
											parsed_word_buffer[parsed_word_length] = (u16) codepoint_alphabet_index;
										}
										parsed_word_length += 1;
									}
								}
							}

							//
							// Process word.
							//

							if (!parsed_word_should_be_ignored && MIN_WORD_LENGTH <= parsed_word_length && parsed_word_length <= max_word_length)
							{
								b32 skip_appending   = false;
								u16 subword_bitfield = 1 << (parsed_word_length - MIN_WORD_LENGTH);

								//
								// Search for parenting word.
								//

								for
								(
									i32 parenting_word_length = max_word_length;
									parenting_word_length > parsed_word_length;
									parenting_word_length -= 1
								)
								{
									struct Dary_u16 parenting_wordset = wordsets[ABSOLUTE_MAX_WORD_LENGTH - parenting_word_length][parsed_word_buffer[0]];
									for
									(
										u16* parenting_word_makeup = parenting_wordset.data;
										parenting_word_makeup < parenting_wordset.data + parenting_wordset.length;
										parenting_word_makeup += parenting_word_length
									)
									{
										static_assert(sizeof(parsed_word_buffer[0]) == sizeof(parenting_word_makeup[0]));
										if
										(
											memeq
											(
												(parenting_word_makeup + 1), // Skipping subword bitfield.
												(parsed_word_buffer    + 1), // Skipping word's initial letter.
												(parsed_word_length    - 1) * sizeof(parsed_word_buffer[0])
											)
										)
										{
											skip_appending            = true;                                        // The word already has a parent.
											parenting_word_makeup[0] |= 1 << (parsed_word_length - MIN_WORD_LENGTH); // Set bit in the parent's subword bitfield.
										}
									}
								}

								//
								// Search for duplicate.
								//

								if (!skip_appending)
								{
									struct Dary_u16 doppelganger_wordset = wordsets[ABSOLUTE_MAX_WORD_LENGTH - parsed_word_length][parsed_word_buffer[0]];
									for
									(
										u16* doppelganger_word_makeup = doppelganger_wordset.data;
										doppelganger_word_makeup < doppelganger_wordset.data + doppelganger_wordset.length;
										doppelganger_word_makeup += parsed_word_length
									)
									{
										static_assert(sizeof(doppelganger_wordset.data[0]) == sizeof(parsed_word_buffer[0]));
										if
										(
											memeq
											(
												(doppelganger_word_makeup + 1), // Skipping subword field.
												(parsed_word_buffer       + 1), // Skipping word's initial letter.
												(parsed_word_length       - 1) * sizeof(parsed_word_buffer[0])
											)
										)
										{
											skip_appending = true; // Avoid having duplicated words. This may occur in German where lowercase/uppercase have meaning.
										}
									}
								}

								//
								// Introduce word into the set.
								//

								if (!skip_appending)
								{
									//
									// Search for subwords.
									//

									for
									(
										i32 subword_length = parsed_word_length - 1;
										subword_length >= MIN_WORD_LENGTH;
										subword_length -= 1
									)
									{
										struct Dary_u16* subword_wordset = &wordsets[ABSOLUTE_MAX_WORD_LENGTH - subword_length][parsed_word_buffer[0]];

										for
										(
											u16* subword_makeup = subword_wordset->data;
											subword_makeup < subword_wordset->data + subword_wordset->length;
										)
										{
											static_assert(sizeof(subword_makeup[0]) == sizeof(parsed_word_buffer[0]));
											if
											(
												memeq
												(
													(parsed_word_buffer + 1),
													(subword_makeup     + 1),
													(subword_length     - 1) * sizeof(subword_makeup[0])
												)
											)
											{
												subword_bitfield |= 1 << (subword_length - MIN_WORD_LENGTH);
												subword_bitfield |= subword_makeup[0]; // In case the subword had its own subwords.
												memmove
												(
													subword_makeup,
													subword_wordset->data + subword_wordset->length - subword_length,
													subword_length * sizeof(subword_makeup[0])
												);
												subword_wordset->length -= subword_length;
												word_makeup_count       -= 1;
											}
											else
											{
												subword_makeup += subword_length;
											}
										}
									}

									//
									// Add word to set.
									//

									static_assert(sizeof(parsed_word_buffer[0]) == sizeof(wordsets[0][0].data[0]));
									dary_push(&wordsets[ABSOLUTE_MAX_WORD_LENGTH - parsed_word_length][parsed_word_buffer[0]], &subword_bitfield);
									dary_push_n
									(
										&wordsets[ABSOLUTE_MAX_WORD_LENGTH - parsed_word_length][parsed_word_buffer[0]],
										parsed_word_buffer + 1,
										parsed_word_length - 1
									);
									word_makeup_count += 1;
								}
							}
						}

						free(stream.data);
					}
					printf("\t%d entries.\n", word_makeup_count);

					//
					// Find table values. First u16 of word-makeups will be replaced with subfield index.
					//

					i32             entry_counts  [ABSOLUTE_MAX_WORD_LENGTH - MIN_WORD_LENGTH + 1][MAX_ALPHABET_LENGTH] = {0};
					u32             length_offsets[ABSOLUTE_MAX_WORD_LENGTH - MIN_WORD_LENGTH + 1] = {0};
					struct Dary_u16 subfields = {0};

					for
					(
						i32 word_length = max_word_length;
						word_length >= MIN_WORD_LENGTH;
						word_length -= 1
					)
					{
						length_offsets[ABSOLUTE_MAX_WORD_LENGTH - word_length] = (u32) stream_size;

						for (i32 word_initial_alphabet_index = 0; word_initial_alphabet_index < LANGUAGE_INFO[language].alphabet_length; word_initial_alphabet_index += 1)
						{
							struct Dary_u16 wordset = wordsets[ABSOLUTE_MAX_WORD_LENGTH - word_length][word_initial_alphabet_index];
							for
							(
								u16* word_makeup = wordset.data;
								word_makeup < wordset.data + wordset.length;
								word_makeup += word_length
							)
							{
								//
								// Increment count.
								//

								entry_counts[ABSOLUTE_MAX_WORD_LENGTH - word_length][word_initial_alphabet_index] += 1;
								if (entry_counts[ABSOLUTE_MAX_WORD_LENGTH - word_length][word_initial_alphabet_index] > (u16) -1)
								{
									error("Entry count overflow! Too many words of the same length that begin with the same letter.");
								}

								//
								// Increment stream size.
								//

								stream_size += PACKED_WORD_SIZE(word_length);
								if (stream_size > (u32) -1)
								{
									error("Word stream too large!");
								}

								//
								// Determine if a subfield can be applied to the subword's bitfield.
								//

								b32 should_push = true;
								for (i32 subfield_index = 0; subfield_index < subfields.length; subfield_index += 1)
								{
									// The current word will never be longer than any word that was processed so far in the subfields, since
									// we are iterating through wordsets sorted in such a way that longer words would be processed first anyways.
									u16 common_mask = ((1 << (15 - __lzcnt16(word_makeup[0]))) - 1);
									if ((word_makeup[0] & common_mask) == (subfields.data[subfield_index] & common_mask))
									{
										word_makeup[0] = (u16) subfield_index;
										should_push    = false;
										break;
									}
								}

								if (should_push)
								{
									dary_push(&subfields, &word_makeup[0]);
									if (subfields.length > (u8) -1)
									{
										error("Subfield count overflow!");
									}

									word_makeup[0] = (u16) (subfields.length - 1);
								}
							}
						}
					}
					printf("\t%lld subfields.\n", subfields.length);

					//
					// Make table of the entry counts.
					//

					strbuf_cstr(&output_buf, "static const u16 WORDS_TABLE_OF_CONTENTS_ENTRY_COUNTS_");
					strbuf_str (&output_buf, LANGUAGE_INFO[language].name);
					strbuf_cstr(&output_buf, "[");
					strbuf_i64 (&output_buf, max_word_length - MIN_WORD_LENGTH + 1);
					strbuf_cstr(&output_buf, "][");
					strbuf_i64 (&output_buf, LANGUAGE_INFO[language].alphabet_length);
					strbuf_cstr(&output_buf, "] PROGMEM =\n");
					strbuf_cstr(&output_buf, "\t{\n");
					write_flush_strbuf(c_source_handle, &output_buf);
					for
					(
						i32 word_length = max_word_length;
						word_length >= MIN_WORD_LENGTH;
						word_length -= 1
					)
					{
						strbuf_cstr(&output_buf, "\t\t{ // ");
						strbuf_i64 (&output_buf, word_length);
						strbuf_cstr(&output_buf, "-lettered words.\n");
						write_flush_strbuf(c_source_handle, &output_buf);
						for (i32 alphabet_index = 0; alphabet_index < LANGUAGE_INFO[language].alphabet_length; alphabet_index += 1)
						{
							strbuf_cstr(&output_buf, "\t\t\t");
							strbuf_i64 (&output_buf, entry_counts[ABSOLUTE_MAX_WORD_LENGTH - word_length][alphabet_index]);
							strbuf_cstr(&output_buf, ", // \"");
							strbuf_str (&output_buf, LETTER_INFO[LANGUAGE_INFO[language].alphabet[alphabet_index]].name);
							strbuf_cstr(&output_buf, "\".\n");
							write_flush_strbuf(c_source_handle, &output_buf);
						}
						strbuf_cstr(&output_buf, "\t\t},\n");
						write_flush_strbuf(c_source_handle, &output_buf);
					}
					strbuf_cstr(&output_buf, "\t};\n\n");

					total_flash += (i32) (sizeof(u16) * LANGUAGE_INFO[language].alphabet_length * (max_word_length - MIN_WORD_LENGTH + 1));

					//
					// Make table of the length offsets.
					//

					strbuf_cstr(&output_buf, "static const u32 WORDS_TABLE_OF_CONTENTS_LENGTH_OFFSETS_");
					strbuf_str (&output_buf, LANGUAGE_INFO[language].name);
					strbuf_cstr(&output_buf, "[");
					strbuf_i64 (&output_buf, max_word_length - MIN_WORD_LENGTH + 1);
					strbuf_cstr(&output_buf, "] PROGMEM = { ");
					write_flush_strbuf(c_source_handle, &output_buf);
					for
					(
						i32 word_length = max_word_length;
						word_length >= MIN_WORD_LENGTH;
						word_length -= 1
					)
					{
						strbuf_u64 (&output_buf, length_offsets[ABSOLUTE_MAX_WORD_LENGTH - word_length]);
						strbuf_cstr(&output_buf, ", ");
						write_flush_strbuf(c_source_handle, &output_buf);
					}
					strbuf_cstr(&output_buf, "};\n\n");

					total_flash += (i32) (sizeof(u32) * (max_word_length - MIN_WORD_LENGTH + 1));

					//
					// Make table of the subfields.
					//

					strbuf_cstr(&output_buf, "static const u16 WORDS_TABLE_OF_CONTENTS_SUBFIELDS_");
					strbuf_str (&output_buf, LANGUAGE_INFO[language].name);
					strbuf_cstr(&output_buf, "[");
					strbuf_i64 (&output_buf, subfields.length);
					strbuf_cstr(&output_buf, "] PROGMEM = {");
					write_flush_strbuf(c_source_handle, &output_buf);
					for (i32 i = 0; i < subfields.length; i += 1)
					{
						strbuf_cstr(&output_buf, " 0b");
						strbuf_8b  (&output_buf, (u8) (subfields.data[i] >> 8));
						strbuf_char(&output_buf, '\'');
						strbuf_8b  (&output_buf, (u8) (subfields.data[i] >> 0));
						strbuf_cstr(&output_buf, ",");
						write_flush_strbuf(c_source_handle, &output_buf);
					}
					strbuf_cstr(&output_buf, " };\n\n");
					write_flush_strbuf(c_source_handle, &output_buf);

					total_flash += (i32) (sizeof(u16) * subfields.length);

					//
					// Generate stream for the language.
					//

					for
					(
						i32 word_length = max_word_length;
						word_length >= MIN_WORD_LENGTH;
						word_length -= 1
					)
					{
						for (i32 word_initial_alphabet_index = 0; word_initial_alphabet_index < LANGUAGE_INFO[language].alphabet_length; word_initial_alphabet_index += 1)
						{
							struct Dary_u16 wordset = wordsets[ABSOLUTE_MAX_WORD_LENGTH - word_length][word_initial_alphabet_index];
							for
							(
								u16* word_makeup = wordset.data;
								word_makeup < wordset.data + wordset.length;
								word_makeup += word_length
							)
							{
								static_assert(BITS_PER_ALPHABET_INDEX == 5); // Packing scheme assumes five bits per alphabet index here.

								// "APPLES" with word-makeup of { 0x**, 15, 15, 11, 4, 18 } or equivalently { 0x**, 0b01111, 0b01111, 0b01011, 0b00100, 0b10010 } will have
								// the 5-bit alphabet indices be packed tightly into the form { 0b*'01011'01111'01111, 0b*'*****'10010'00100 } where three indices can
								// be compressed into 2 bytes with 1 padding bit.

								u16 packed[((PACKED_WORD_SIZE(ABSOLUTE_MAX_WORD_LENGTH) - 1) + 1) / 2] = {0};

								for (i32 packed_index = 0; packed_index < ((word_length - 1) + 2) / 3; packed_index += 1)
								{
									u8 alphabet_indices_to_be_packed[3] =
										{
											1 + packed_index * 3 + 0 < word_length ? (u8) word_makeup[1 + packed_index * 3 + 0] : 0,
											1 + packed_index * 3 + 1 < word_length ? (u8) word_makeup[1 + packed_index * 3 + 1] : 0,
											1 + packed_index * 3 + 2 < word_length ? (u8) word_makeup[1 + packed_index * 3 + 2] : 0,
										};
									packed[packed_index] =
										(alphabet_indices_to_be_packed[0] << (BITS_PER_ALPHABET_INDEX * 0)) |
										(alphabet_indices_to_be_packed[1] << (BITS_PER_ALPHABET_INDEX * 1)) |
										(alphabet_indices_to_be_packed[2] << (BITS_PER_ALPHABET_INDEX * 2));
								}

								static_assert(LITTLE_ENDIAN);
								write_raw_data(stream_handle, &word_makeup[0], sizeof(u8)                       ); // Subfield index should be contained within a byte.
								write_raw_data(stream_handle, packed         , PACKED_WORD_SIZE(word_length) - 1);
							}
						}
					}

					//
					// Clean up.
					//

					free(subfields.data);
					for (i32 i = 0; i < countof(wordsets); i += 1)
					{
						for (i32 j = 0; j < countof(wordsets[i]); j += 1)
						{
							free(wordsets[i][j].data);
						}
					}
				}

				//
				// Make table of contents for all of the languages.
				//

				strbuf_cstr(&output_buf, "static const struct { const u16* entry_counts; const u16* length_offsets; const u16* subfields; u8 max_word_length; } WORDS_TABLE_OF_CONTENTS[] PROGMEM =\n");
				strbuf_cstr(&output_buf, "\t{\n");
				write_flush_strbuf(c_source_handle, &output_buf);
				for (enum Language language = {0}; language < Language_COUNT; language += 1)
				{
					strbuf_cstr(&output_buf, "\t\t[Language_");
					strbuf_str (&output_buf, LANGUAGE_INFO[language].name);
					strbuf_cstr(&output_buf, "] = { ");
					write_flush_strbuf(c_source_handle, &output_buf);

					strbuf_cstr(&output_buf, "(const u16*) WORDS_TABLE_OF_CONTENTS_ENTRY_COUNTS_");
					strbuf_str (&output_buf, LANGUAGE_INFO[language].name);
					strbuf_cstr(&output_buf, ", ");
					write_flush_strbuf(c_source_handle, &output_buf);

					strbuf_cstr(&output_buf, "(const u16*) WORDS_TABLE_OF_CONTENTS_LENGTH_OFFSETS_");
					strbuf_str (&output_buf, LANGUAGE_INFO[language].name);
					strbuf_cstr(&output_buf, ", ");
					write_flush_strbuf(c_source_handle, &output_buf);

					strbuf_cstr(&output_buf, "(const u16*) WORDS_TABLE_OF_CONTENTS_SUBFIELDS_");
					strbuf_str (&output_buf, LANGUAGE_INFO[language].name);
					strbuf_cstr(&output_buf, ", ");
					write_flush_strbuf(c_source_handle, &output_buf);

					strbuf_cstr(&output_buf, "countof(WORDS_TABLE_OF_CONTENTS_ENTRY_COUNTS_");
					strbuf_str (&output_buf, LANGUAGE_INFO[language].name);
					strbuf_cstr(&output_buf, ") + MIN_WORD_LENGTH - 1");
					write_flush_strbuf(c_source_handle, &output_buf);

					strbuf_cstr(&output_buf, "},\n");
					write_flush_strbuf(c_source_handle, &output_buf);
				}
				strbuf_cstr(&output_buf, "\t};\n");
				write_flush_strbuf(c_source_handle, &output_buf);

				total_flash += (sizeof(u16) * 3 + sizeof(u8)) * Language_COUNT;

				strbuf_cstr(&output_buf, "\n");
				strbuf_cstr(&output_buf, "// ");
				strbuf_i64 (&output_buf, total_flash);
				strbuf_cstr(&output_buf, " bytes total.\n");
				write_flush_strbuf(c_source_handle, &output_buf);

				printf("%d bytes of flash will be used.\n", total_flash);

				//
				// Clean up.
				//

				close_file_writing_handle(stream_handle);
				close_file_writing_handle(c_source_handle);
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
	confess, but it gets the job done!

	For what exactly this program achieves, see [Mask Generation] and [Word Compression].
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
	it! This applies to German too, and even in English where 'Q' doesn't seem to appear at all,
	but does for the 7-letter version. Strange. I highly suspect that Game Pigeon is doing some
	weird RNG stuff to get a good set of letters that'll be enjoyable to play.

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
	flash of the Diplomat. As explained thoroughly in Diplomat_usb.c, the compression that is done
	is essentially run-length encoding that begins on the top-left corner of the first letter, to
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

/* [Word Compression].
	Nerd is the MCU that performs all the computation to figure out what words are valid in the
	provided wordgame and how to submit it. Nerd could just read a dictionary text file, but this
	would be massively bottlenecked by the communication between the storage medium and the MCU.
	The less time that spent in the data transfer, the better. Thus several techniques are
	employed to achieve better information density.

		- Lengthed words.

			If Nerd were to read the dictionary as raw bytes, it'd look something like:

			// "ACTINON\nACTINONS\nACTINOPOD\nACTINOPODS\nACTINOTHERAPIES\n..."

			To get the first word, Nerd would have to keep scanning until it finally
			encounters the '\n' byte. If the storage medium is divded into sectors, which the SD
			card is, then this '\n' would not appear until the next chunk, making this process a
			bit more awkward than it really needs to be. What can be done instead is have each
			word be prepended with its length instead, and drop the '\n' byte entirely.
			For example:

			// "(7)ACTINON(8)ACTINONS(9)ACTINOPOD(10)ACTINOPODS(15)ACTINOTHERAPIES..."

			Where (N) represents the raw byte of value N.

			Now Nerd immediately knows the length of the word, and if the word is too long for
			a particular wordgame (e.g. can't make 15-lettered word in Anagrams), then Nerd
			can skip it entirely and move onto the next word. Knowing whether or not the entire
			word is within the sector is now also a bit easier to handle.

			But it can be further improved! If we sort words by their lengths, specifically from
			longest to shortest, then we can omit most of these length-bytes entirely. What we'd
			have instead is essentially a header that describes how many N-lettered words there
			are, and then how many (N-1)-lettered words after that, and so on.

			// [Length Header].
			//
			//     ...
			//     (2) : 11-lettered words.
			//     (7) : 10-lettered words.
			//     (5) : 9-lettered words.
			//     (3) : 8-lettered words.
			//     (1) : 7-lettered words.
			//     (2) : 6-lettered words.
			//     (0) : 5-lettered words.
			//     ...
			//
			// [Continuous Stream of Letters].
			//
			//     ACTINOZOANSACTIVATIONS...
			//     ACTINOZOANACTIONABLEACTIONABLYACTIONISTSACTIONLESSACTIVATINGACTIVATION...
			//     ACTIONERSACTIONINGACTIONISTACTIVATEDACTIVATES...
			//     ACTIONEDACTIONERACTIVATE...
			//     ACTIONS...
			//     ACTINSACTION...

			This pretty much factors out all the common bytes of words into one place, like
			the distributive property. Another huge benefit of this scheme is that the Nerd
			can now skip an entire family of words that are too long for the current wordgame,
			rather than skip on a word-by-word basis.

		- Initial letters.

			Consider this sequence of 5-lettered words:

				"BYRESBYRLSBYSSIBYTESBYWAYCAAEDCABALCABASCABBYCABERCABINCABLECABOB..."

			Like with the word lengths, we can factor out the common bytes of these words,
			specifically the letter they start with. Thus, we can redefine the header to details
			how many N-letter words that begin with some particular letter X, for all possible
			combinations of N and X. By doing this, we can also omit the initial letter of each
			word now, since it's just information that the Nerd can recover.

			// [Length & Initial Header].
			//
			//     ...
			//     (5) 5-letter words that begin with 'B'.
			//     (8) 5-letter words that begin with 'C'.
			//     ...
			//
			// [Continuous Stream of Letters].
			//
			//     YRESYRLSYSSIYTESYWAYAAEDABALABASABBYABERABINABLEABOB...

			So if English has an alphabet of 26 letters and the maximum and minimum word lengths
			we will allow to store are 15 and 3 respectively, then we should expect 26*(15-3+1)
			or 338 counts in the header.

			This optimization on top of the previous length-based optimization allows for huge
			amount of redundant information in the dictionary file to be removed, but also allow
			for Nerd to skip even more family of words if needed. For example, if the wordbank
			have letters (A, N, D, E, I, M), but the family of words are 4-lettered words beginning
			with H, then that entire chunk of words be skippped entirely, since the wordbank does
			not provide an H anyways.

		- Packed alphabet indices.

			Russian has an alphabet of 33 letters, largest of any of the other languages that we
			support so far, but some of them are never actually used (or just so rare that I have
			never actually encountered them...). Thus, we can assume that no alphabet will be
			larger than 32 letters, and there  can use a 5-bit index to refer to the letters of the
			alphabet. This also addresses the issue of UTF-8, since Cyrillic characters are two
			bytes.

			5 is a pretty ugly number, and it would've been great if it was 4 bits, but
			this turns out to be a futile attempt anyways (see: [Deadend of Partitions]).

			Groups of three letters in a word can be packed together into 15 bits, and by having 1
			padding bit, this makes up two bytes.

			// A B C -> aaaaabbb'bbccccc*

			Not all words are multiples of 3s, so if there's two letters leftover, then there'll
			just be some more padding bits.

			// A B -> aaaaabbb'bb******

			If there's just one letter leftover, then we just use one byte.

			// A -> aaaaa***

			Since we save a byte for every three letters of a word, this is about a 33% reduction.
			Of course, we're saving a byte for every word already due to the previous optimization.

		- Subwords.

			Consider the following words:

			// INTERRELATIONSHIPS
			// INTERRELATIONSHIP
			// INTERRELATIONS
			// INTERRELATION
			// INTER
			// IN
			// RELATIONSHIPS
			// RELATIONSHIP
			// RELATIONS
			// RELATION
			// SHIPS
			// SHIP

			All of these words are essentially a subset of the parenting word "INTERRELATIONSHIPS".
			A very simple optimization we can do to address most of this redundancy is by having a
			bitfield that indicate where a word can be truncated but be still left a valid word
			that exists in the dictionary. For example:

			//           INTER
			//           |       INTERRELATION
			//           |       |INTERRELATIONS
			//           |       ||  INTERRELATIONSHIP
			//           |       ||  |INTERRELATIONSHIPS
			//           |       ||  ||
			//           v       vv  vv
			// LSb ->  0010000000110011 <- MSb // Bits omitted on LHS assuming MIN_WORD_LENGTH=3.
			//       INTERRELATIONSHIPS

			The bitfield can only describe subwords that also begins with the parenting word (thus
			"RELATIONSHIP" is not encoded despite still being technically a valid subword of
			"INTERRELATIONSHIPS"), but this is mostly fine, since this simple optimization allows
			for us to encode "INTERRELATIONSHIPS" and 5 other words at the same time with the
			simple flip of a bit! Asking for any more overcomplicates the scheme.

			The thing we have to be aware about this optimization is that we have to process for
			subwords in such a way that we won't have an entry for a word twice in the compressed
			dictionary file. For example, "EXIT" could be encompassed by both "EXITS" and
			"EXITING", but we will only actually assign it to one of them. This avoid redudant
			processing and submissions on the Nerd's end.

			Another quirk that the optimization brings is that the family of "12-lettered words
			beginning with the letter 'B'" no longer consists of entirely 12-lettered words anymore,
			since any of those 12-lettered words may have a valid subword ranging from
			MIN_WORD_LENGTH to 11 letters. This isn't exactly significant, but it means that Nerd
			may not iterate through ALL the longest words first. For example, English Anagrams
			have a word limit of 6 letters (7 in the other mode), so Nerd would skim through the
			entire dictionary file until it finds the family of words that are 6 letters long.
			However, there are definitely some family of words that are 12-lettered but have valid
			subwords that would be reproducible within the Anagrams game. Again, this is not too
			significant, it would just mean that Nerd cannot gaurantee it'll submit ALL the longest
			words FIRST, but it will nonetheless get the majority of them!

		- Subfields.

			The subword optimization itself needs to be optimized, because there's currently no
			good way to store this subword bitfield information in a convenient manner.
			For 14-lettered words, you'd need at least 11 bits to encode all the potential 11
			subwords (assuming MIN_WORD_LENGTH=3 and the 14-lettered word itself is already known).
			This means two bytes is needed. For 5-lettered words, however, only 2 bits are
			required! So a byte is sufficient and, if anything, inefficient...

			Thus what we can do is first get the set of all unique subword bitfields from words of
			all different lengths. For example, consider the set:

			// A: --1--1--1
			// B: -1---111
			// C: -1---1
			// D: --1--1
			// E: -----1
			// F: --1

			(Zeros are replaced with dashes for readability).
			Then if we group the subword bitfields like so...

			// (0) A: --1--1--1
			//     D: --1--1
			//     F: --1
			//
			// (1) B: -1---111
			//     C: -1---1
			//
			// (2) E: -----1

			Each group (namely (0), (1), and (2)) will have some longest subword bitfield that
			encompasses all the shorter subword bitfields in that same group. The insight here
			is that the shorter subword bitfields can be completely replaced with the longest
			subword bitfield and have it where the extra bits are just ignored. Thus we can
			instead have a set of "subfields" and replace the original subword bitfields of each
			word with indices to this set of subfields.

			// [Subfields].
			//     (0) --1--1--1
			//     (1) -1---111
			//     (2) -----1
			//
			//  A: (0) -> (--1--1--1)
			//  B: (1) -> (-1---111 )
			//  C: (1) -> (-1---1** )
			//  D: (0) -> (--1--1***)
			//  E: (2) -> (-----1   )
			//  F: (0) -> (--1******)

			We know how many bits to ignore since we already know the length of the word.

			Since English would have the largest set of words (being language that's used for all
			Game Pigeon wordgames), it's quite fortunate that there are actually less than 256
			subfields, so we can actually just use a single byte as an index to indicate all the
			word's valid subwords without having to worry about varied-lengthed subword bitfields.
			We can just prepend the subfield index right before the packed letters of the word.

	The majority of the data pertaining to the composition of the words is stored as a single
	binary file on an SD card that the Nerd has access to. Data for how this file is laid out
	(the amount of N-lettered words beginning with the letter X, what the actual subfield values
	are, etc.) is outputted as a header file that gets compiled in with Nerd and then stored in
	flash.
*/

/* [Deadend of Partitions].
	In the base Game Pigeon wordgames, WordHunt (4x4) is where you could hypothetically create the
	longest word of 16 letters. WordHunt (5x5) pushes this to 25, but chances are you can't even
	come up with a word longer than 16 and also have it practically appear in a random game. Thus
	16 is a reasonable upper-limit on the length of words we should care about and process for.
	However, this upper bound of 16 also means that for all words, only up to 16 unique, distinct
	letters will be used out of, say, the 26 letters of the English alphabet.

	This insight has huge implications, because if we can get away with using only 4 bits per
	letter of a word, which 16 happens to be conveniently a power of two of, then we can pack two
	letters for every byte! We could end up with a compressed dictionary with potentially no
	padding bits at all and a much simplier decoding scheme.

	Of course, if we only use 4 bits and not 5 bits, we will be unable to represent certain letters
	of the alphabet, and therefore unable to encode entire words that utilize those letters. The
	trick here is to essentially have "betabets" that are subsets of the parenting alphabet, with
	each betabet containing 16 letters at most. Then by specifying which betabet we are using
	preemptively, we can use this betabet as if it was the alphabet. If a particular word can't be
	written with a particular betabet, then a different or a new one will be made to accomdate that
	word.

	Thus the problem boils down to coming up with a set of betabets that are highly optimized (to
	which I called them sigmabets) where the set will be of minimum size and yet still be able to
	represent every single word that we'd be saving in the compressed dictionary file.

	Consider the following words and their smallest betabets:

	//              A B C D E F G H I J K L M N O P Q R S T U V W X Y Z
	//
	// APPLECIDER : A   C D E       I     L       P   R
	// CANDLE     : A   C D E             L
	// PACER      : A   C   E                     P
	// CHRISTMAS  : A   C         H I       M         R S T
	// DIODE      :       D E       I           O
	// WHISKERS   :         E     H I   K             R S       W
	// WINDMAKER  : A     D E       I   K   M N       R         W

	These words can all be represented by a single sigmabet.

	//              A C D E H I K L M N O P R S T W
	//
	// APPLECIDER : A C D E   I   L       P R
	// CANDLE     : A C D E       L
	// PACER      : A C   E               P
	// CHRISTMAS  : A C     H I     M       R S T
	// DIODE      :     D E   I         O
	// WHISKERS   :       E H I K           R S   W
	// WINDMAKER  : A   D E   I K   M N     R     W

	Because this sigmabet uses only 16 letters of the 26-lettered alphabet of the English language,
	only a 4-bit index is needed for each word to describe its letters. The word "ZOO", however,
	cannot be encoded by the above sigmabet since the sigmabet does not include 'Z'. Thus a
	separate sigmabet must be created to handle words like "ZOO".

	But how exactly do we find the minimum amount of sigmabets that'll encode every single word
	we'll care about? Well, I'm actually not too sure if it's possible... to do efficiently.

	See, this problem seems to be related to the NP-hard problem of set covering, where you have a
	set of subsets that you must filter out as much as you can without leaving any gaps in the
	union. The sigmabet generation problem seems related because we can imagine the sigmabets
	themselves as "subsets" of all of the word's betabets, and we must weed out as many as we can
	to find the most optimal collection of sigmabets. In short, we should be expecting to not be
	able to find a good algorithm that'll compute the most optimal set of sigmabets in polynomial
	time, any time at all.

	Nonetheless, we can still try. See, this sigmabet generation problem comes down to finding the
	most optimal partitioning of all recognized words. In each partition, we convert each word into
	its respective betabet, and "OR" all of the betabets in the partition together. The OR'd
	betabets will be the sigmabet that encode all of the words in that particular partition. Thus
	for any partitioning of the dictionary to be valid, each partition cannot use more than 16
	distinct letters, lest its sigmabets will end up with more than 16 letters and be unindexable
	with 4 bits.

	So to find the most optimal collection of sigmabets, we just simply iterate through all
	possible partitions of the dictionary, which contains over 250K words. Yikes! Luckily, we can
	trim this down to something more managable. Since we're going to convert the words into
	betabets, and betabets will be OR'd together, we can just simply first generate the collection
	of all betabets where no two betabets are subsets of each other. This collection comes out to
	being around a hair short of 7K entries, so much more managable. It's still brute-forcing, but
	managable brute-forcing... maybe.

	Now all we have to do is just partition all these betabets in this 7K collection in such a way
	that they generate the most optimal set of sigmabets. To first grasp the sheer scale of this
	operation, consider how many possible partitioning there can be for a set of size N. If we
	were talking about all possible combinations, you'd use exponentiation; if we were doing
	permutations, we'd use factorials. So what will we use for partitionings? Well, there's
	actually no known closed-form solution for this computation... At best, we have an algorithm
	similar to Pascal's triangle. With this algorithm, we can generate Bell's numbers, which is
	what the counts of all possible partitionings are called. Here are the first 32 Bell's numbers:

	//  1:                                   1
	//  2:                                   2
	//  3:                                   5
	//  4:                                  15
	//  5:                                  52
	//  6:                                 203
	//  7:                                 877
	//  8:                               4 140
	//  9:                              21 147
	// 10:                             115 975
	// 11:                             678 570
	// 12:                           4 213 597
	// 13:                          27 644 437
	// 14:                         190 899 322
	// 15:                       1 382 958 545
	// 16:                      10 480 142 147
	// 17:                      82 864 869 804
	// 18:                     682 076 806 159
	// 19:                   5 832 742 205 057
	// 20:                  51 724 158 235 372
	// 21:                 474 869 816 156 751
	// 22:               4 506 715 738 447 323
	// 23:              44 152 005 855 084 346
	// 24:             445 958 869 294 805 289
	// 25:           4 638 590 332 229 999 353
	// 26:          49 631 246 523 618 756 274
	// 27:         545 717 047 936 059 989 389
	// 28:       6 160 539 404 599 934 652 455
	// 29:      71 339 801 938 860 275 191 172
	// 30:     846 749 014 511 809 332 450 147
	// 31:  10 293 358 946 226 376 485 095 653
	// 32: 128 064 670 049 908 713 818 925 644

	This means for a set of 32 elements, there are over 100 goddamn septillion partitionings! It
	may look like the sequence is growing exponentially, and it is, but it is in fact FASTER than
	exponential growth (1). With that in mind, now try to imagine the amount of partitionings of
	7000 elements! Since it grows faster than any exponential growth, we can confidently say it's
	more than 2^7000. We'd likely need several orders of magnitude multiples of of 875 bytes to
	even represent this humongous number!

	But let's not be swayed so easily: a majority of partitionings are not valid canidates to
	becoming the most optimal collection of sigmabets anyways, since there's this constraint that
	sigmabets cannot have more than 16 letters. If iterate through all possible partitions in such
	a way that can essentially prune large amounts of dead-ends, the iteration can become somewhat
	quick.

	This is what I essentially did for a week. I wrote a pretty convoluted algorithm that iterated
	through all possible partitionings of ~7K elements where it skipped a whole lot of dead-ends.
	I got pretty far, I'll admit; I'd probably even implement multi-threading into the search if I
	was paid at all to do this stupid project, but what I had at the end was sufficient.

	After possibly several hundred trillion partitionings considered and iterated over, I have
	found that, given my dataset of the English wordset, a collection of 187 sigmabets is
	sufficient in describing all words we'd be processing for*. This is not a definitive answer,
	though. Chances are the program would never finish until the heat death of the universe, but
	187 sigmabets is pretty damn good.

	Except... we did all of this just to shave off a single bit of a 5-bit index. Consider the
	current optimization for the compression of the word "ELECTROMAGNETISM":

	// Implicit.
	// |   Subfield index.
	// |    |       Packed into 2 bytes.
	// |    |                |
	// |    |    ____________|____________
	// |    |    |     |     |     |     |
	// v  vvvv  vvv   vvv   vvv   vvv   vvv
	// E  XXXX (LEC) (TRO) (MAG) (NET) (ISM)
	//     1B    2B    2B    2B    2B    2B
	//
	// Total: 11 bytes.

	Now consider if we managed to squeeze it down to 4-bits per letter:

	// Implicit.
	// |   Subfield index.
	// |    |      Packed into 2 bytes.
	// |    |               |
	// |    |    ___________|__________
	// |    |    |      |      |      |
	// v  vvvv  vvvv   vvvv   vvvv   vvvv
	// E  XXXX (LECT) (ROMA) (GNET) (ISM*)
	//     1B    2B     2B     2B     2B
	//
	// Total: 9 bytes.

	For the longest word, we'd save three bytes! Wonderful! But how exactly do we indicate which
	sigmabet we're actually using...? Hmm, okay, what if we do it like with the subwords where we
	had a byte-index that'll point to the specific sigmabet to use? Okay, that'll be an extra byte
	overhead for each word now, making the savings for "ELECTROMAGNETISM" go from 3 bytes to 2
	bytes. No big deal; it adds up when we have thousands of words!

	Except... there just aren't that many words that long. Most words are about 5-8 letters long,
	and you don't actually lose or gain any bytes from this optimization... For example, "LOSERS":

	// Implicit.
	// |   Subfield index.
	// |    | Packed into 2 bytes.
	// |    |       |
	// |    |    ___|___
	// |    |    |     |
	// v  vvvv  vvv   vvv
	// L  XXXX (OSE) (RS*)
	//     1B    2B    2B
	//
	// Total: 5 bytes.
	//
	// Implicit.
	// |   Subfield index.
	// |    | Sigmabet index.
	// |    |    | Packed into 2 bytes.
	// |    |    |    | Stored as 1 byte.
	// |    |    |    |     |
	// v  vvvv vvvv  vvvv   v
	// L  XXXX YYYY (OSER) (S)
	//     1B   1B    2B    1B
	//
	// Total: 5 bytes.

	It's even worse for 4-lettered words actually! Like for "LOSE":

	// Implicit.
	// |   Subfield index.
	// |    | Packed into 2 bytes.
	// |    |    |
	// |    |    |
	// v  vvvv  vvv
	// L  XXXX (OSE)
	//     1B    2B
	//
	// Total: 3 bytes.
	//
	// Implicit.
	// |   Subfield index.
	// |    | Sigmabet index.
	// |    |    | Packed into 2 bytes.
	// |    |    |    |
	// |    |    |    |
	// v  vvvv vvvv  vvvv
	// L  XXXX YYYY (OSE*)
	//     1B   1B    2B
	//
	// Total: 4 bytes.

	Yikes. I mean, sure, perhaps we could save a couple kilobytes in the net sum, but this was the
	final nail in the coffin for me: decoding it just might be as difficult as having 5-bit
	indices. Since we are now using a sigmabet that's just a subset of the alphabet, we might have
	an extra layer of indirection penaltized onto us in the decoding process. All just to shave
	off one extra bit!

	There's no hope in having the sigmabet indices be stored in some sort of header or glossary.
	With the lengthed and initial letter glossary, there's already 338 entries for English. If
	there's 187 sigmabets, then this explodes to 63,206! All of this is just WAY too complicated
	and much for something as simple as "shaving off one extra bit". This is just a deadend.

	It wasn't wasted effort though; I learned quite a lot. For other languages besides English,
	the amount of sigmabets were quite few. Can't exactly recall, but it might've been around the
	magnitude of 64. This is mostly in due part of the fact that foreign languages are only found
	within Anagrams which have a word length limit of 6 letters. If it turns out that the most
	optimal sigmabet collection is exceptionally tiny (which very well may be true since I had to
	eventually terminate the search that'll likely last the entire to heat death of the universe),
	then this optimaization would be more a lot more practical.

	But alas, it was futile.

	* The sigmabet generation ignores the first letter of words, since we performed an optimization
	that'd that letter implicit anyways.
	(1) Bell numbers @ Site(johndcook.com/blog/2018/06/05/bell-numbers).
*/
