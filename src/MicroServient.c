#define _CRT_SECURE_NO_DEPRECATE 1
#include <Windows.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "defs.h"
#include "misc.c"
#include "MicroServient_strbuf.c"

static str
str_cstr(char* cstr)
{
	str result = { cstr, strlen(cstr) };
	return result;
}

static b32
str_ends_with_caseless(str src, str ending)
{
	b32 result = true;

	if (src.length >= ending.length)
	{
		for (i64 i = 0; i < ending.length; i += 1)
		{
			if (to_lower(src.data[src.length - ending.length + i]) != to_lower(ending.data[i]))
			{
				result = false;
				break;
			}
		}
	}
	else
	{
		result = false;
	}

	return result;
}

#define error(STRLIT, ...) error_abort("(\"%.*s\") :: " STRLIT, i32(file_path.length), file_path.data,##__VA_ARGS__)
static str
malloc_read_file(str file_path)
{
	str result = {0};

	//
	// Turn lengthed string into null-terminated string.
	//

	char file_path_cstr[256] = {0};
	if (file_path.length >= countof(file_path_cstr))
	{
		error("File path too long.");
	}
	memmove(file_path_cstr, file_path.data, file_path.length);

	//
	// Get file length.
	//

	FILE* file = fopen(file_path_cstr, "rb");
	if (!file)
	{
		error("`fopen` failed. Does the file exist?");
	}

	if (fseek(file, 0, SEEK_END))
	{
		error("`fseek` failed.");
	}

	result.length = ftell(file);
	if (result.length == -1)
	{
		error("`ftell` failed.");
	}

	//
	// Get file data.
	//

	if (result.length)
	{
		result.data = malloc(result.length);
		if (!result.data)
		{
			error("Failed to allocate enough memory");
		}

		if (fseek(file, 0, SEEK_SET))
		{
			error("`fseek` failed.");
		}

		if (fread(result.data, result.length, 1, file) != 1)
		{
			error("`fread` failed.");
		}
	}

	//
	// Clean up.
	//

	if (fclose(file))
	{
		error("`fclose` failed.");
	}

	return result;
}
#undef error

static void
free_read_file(str* src)
{
	free(src->data);
	*src = (str) {0};
}

#define error(STRLIT, ...) error_abort("(\"%.*s\") :: " STRLIT, i32(file_path.length), file_path.data,##__VA_ARGS__)
static struct BMP
bmp_malloc_read_file(str file_path)
{
	struct BMP result = {0};

	str file_content = malloc_read_file(file_path);

	if (file_content.length < sizeof(struct BMPFileHeader))
	{
		error("File too small for BMP file header.");
	}
	struct BMPFileHeader* file_header = (struct BMPFileHeader*) file_content.data;

	if (!(file_header->bfType == (u16('B') | (u16('M') << 8)) && file_header->bfSize == file_content.length))
	{
		error("Invalid BMP file header.");
	}

	union BMPDIBHeader* dib_header = (union BMPDIBHeader*) &file_header[1];
	if
	(
		u64(file_content.length) < sizeof(struct BMPFileHeader) + sizeof(dib_header->size) ||
		u64(file_content.length) < sizeof(struct BMPFileHeader) + dib_header->size
	)
	{
		error("File too small for BMP DIB header.");
	}

	switch (dib_header->size)
	{
		case sizeof(struct BMPDIBHeaderCore):
		{
			error("BMPDIBHeaderCore not supported... yet.");
		} break;

		case sizeof(struct BMPDIBHeaderInfo):
		{
			error("BMPDIBHeaderInfo not supported... yet.");
		} break;

		case sizeof(struct BMPDIBHeaderV4):
		{
			error("BMPDIBHeaderV4 not supported... yet.");
		} break;

		case sizeof(struct BMPDIBHeaderV5):
		{
			if (!(dib_header->v5.bV5Planes == 1))
			{
				error("BMPDIBHeaderV5 with invalid field(s).");
			}

			switch (dib_header->v5.bV5Compression)
			{
				case BMPCompression_BI_RGB:
				{
					if
					(
						!(
							dib_header->v5.bV5Width > 0 &&
							dib_header->v5.bV5Height != 0 &&
							dib_header->v5.bV5Planes == 1
						)
					)
					{
						error("Bitmap DIB header (BITMAPV5HEADER) with an invalid field.");
					}

					if
					(
						dib_header->v5.bV5BitCount == 24 &&
						dib_header->v5.bV5Compression == BMPCompression_BI_RGB &&
						dib_header->v5.bV5Height > 0
					)
					{
						u32 bytes_per_row = (dib_header->v5.bV5Width * (dib_header->v5.bV5BitCount / bitsof(u8)) + 3) / 4 * 4;
						if (dib_header->v5.bV5SizeImage != bytes_per_row * dib_header->v5.bV5Height)
						{
							error("Bitmap DIB header (BITMAPV5HEADER) configured with an invalid field.");
						}

						result =
							(struct BMP)
							{
								.dim_x = dib_header->v5.bV5Width,
								.dim_y = dib_header->v5.bV5Height,
								.data  = malloc(dib_header->v5.bV5Width * dib_header->v5.bV5Height * sizeof(struct BMPPixel)),
							};

						if (!result.data)
						{
							error("Failed to allocate.");
						}

						for (i64 y = 0; y < dib_header->v5.bV5Height; y += 1)
						{
							for (i64 x = 0; x < dib_header->v5.bV5Width; x += 1)
							{
								u8* channels =
									((u8*) file_content.data) + file_header->bfOffBits
										+ y * bytes_per_row
										+ x * (dib_header->v5.bV5BitCount / bitsof(u8));

								// It's probably somewhere, but couldn't find where the byte ordering was explicitly described in the documentation.
								result.data[y * result.dim_x + x] =
									(struct BMPPixel)
									{
										.r = channels[2],
										.g = channels[1],
										.b = channels[0],
										.a = 255,
									};
							}
						}
					}
					else
					{
						error("Bitmap with DIB header (BITMAPV5HEADER) configuration that's not yet supported.");
					}
				} break;

				case BMPCompression_BI_RLE8:
				{
					error("BI_RLE8 not handled... yet.");
				} break;

				case BMPCompression_BI_RLE4:
				{
					error("BI_RLE4 not handled... yet.");
				} break;

				case BMPCompression_BI_BITFIELDS:
				{
					error("BI_BITFIELDS not handled... yet.");
				} break;

				case BMPCompression_BI_JPEG:
				{
					error("BI_JPEG not handled... yet.");
				} break;

				case BMPCompression_BI_PNG:
				{
					error("BI_PNG not handled... yet.");
				} break;

				case BMPCompression_BI_CMYK:
				{
					error("BI_CMYK not handled... yet.");
				} break;

				case BMPCompression_BI_CMYKRLE8:
				{
					error("BI_CMYKRLE8 not handled... yet.");
				} break;

				case BMPCompression_BI_CMYKRLE4:
				{
					error("BI_CMYKRLE4 not handled... yet.");
				} break;

				default:
				{
					error("Unknown compression method 0x%02X.", dib_header->v5.bV5Compression);
				} break;
			}
		} break;
	}

	free_read_file(&file_content);
	return result;
}
#undef error

#define error(STRLIT, ...) error_abort("(\"%.*s\") :: " STRLIT, i32(file_path.length), file_path.data,##__VA_ARGS__)
static void
bmp_export(struct BMP src, str file_path)
{
	char file_path_cstr[256] = {0};
	if (file_path.length >= countof(file_path_cstr))
	{
		error("File path too long.");
	}
	memmove(file_path_cstr, file_path.data, file_path.length);

	FILE* file = fopen(file_path_cstr, "wb");
	if (!file)
	{
		error("`fopen` failed.");
	}

	struct BMPDIBHeaderV4 dib_header =
		{
			.bV4Size          = sizeof(dib_header),
			.bV4Width         = src.dim_x,
			.bV4Height        = src.dim_y,
			.bV4Planes        = 1,
			.bV4BitCount      = bitsof(struct BMPPixel),
			.bV4Compression   = BMPCompression_BI_BITFIELDS,
			.bV4RedMask       = u32(0xFF) << (offsetof(struct BMPPixel, r) * 8),
			.bV4GreenMask     = u32(0xFF) << (offsetof(struct BMPPixel, g) * 8),
			.bV4BlueMask      = u32(0xFF) << (offsetof(struct BMPPixel, b) * 8),
			.bV4AlphaMask     = u32(0xFF) << (offsetof(struct BMPPixel, a) * 8),
			.bV4CSType        = BMPColorSpace_LCS_CALIBRATED_RGB,
		};
	struct BMPFileHeader file_header =
		{
			.bfType    = u16('B') | (u16('M') << 8),
			.bfSize    = sizeof(file_header) + sizeof(dib_header) + src.dim_x * src.dim_y * sizeof(struct BMPPixel),
			.bfOffBits = sizeof(file_header) + sizeof(dib_header),
		};

	if (fwrite(&file_header, sizeof(file_header), 1, file) != 1)
	{
		error("Failed to write BMP file header.");
	}

	if (fwrite(&dib_header, sizeof(dib_header), 1, file) != 1)
	{
		error("Failed to write DIB header.");
	}

	if (fwrite(src.data, src.dim_x * src.dim_y * sizeof(*src.data), 1, file) != 1)
	{
		error("Failed to write pixel data.");
	}

	if (fclose(file))
	{
		error("`fclose` failed.");
	}
}
#undef error

static void
bmp_free_read_file(struct BMP* bmp)
{
	free(bmp->data);
	*bmp = (struct BMP) {0};
}

static void
fwrite_str(FILE* file, str src)
{
	if (src.length && fwrite(src.data, src.length, 1, file) != 1)
	{
		error_abort("`fwrite_str` failed.");
	}
}

int
main(int argc, char** argv)
{
	b32 err = 0;

	//
	// Parse CLI arguments.
	//

	b32        cli_parsed = false;
	struct CLI cli        = {0};

	if (argc >= 2)
	{
		b32 cli_field_parsed[CLIField_COUNT] = {0};

		//
		// Parse each argument passed by the user.
		//

		for
		(
			i32 arg_index = 1;
			arg_index < argc;
			arg_index += 1
		)
		{
			for (enum CLIField cli_field = {0}; cli_field < CLIField_COUNT; cli_field += 1)
			{
				if (!cli_field_parsed[cli_field])
				{
					switch (CLI_FIELD_METADATA[cli_field].typing)
					{
						case CLIFieldTyping_str:
						{
							*(str*) (((u8*) &cli) + CLI_FIELD_METADATA[cli_field].offset) =
								(str)
								{
									.data   = argv[arg_index],
									.length = strlen(argv[arg_index]),
								};
						} break;
					}

					cli_field_parsed[cli_field] = true;
					break;
				}
			}
		}

		//
		// Verify all required arguments were fulfilled.
		//

		cli_parsed = true;
		for (enum CLIField cli_field = {0}; cli_field < CLIField_COUNT; cli_field += 1)
		{
			if (!cli_field_parsed[cli_field])
			{
				cli_parsed = false;
				err        = true;

				printf
				(
					"Required argument [%.*s] not provided.\n",
					i32(CLI_FIELD_METADATA[cli_field].name.length), CLI_FIELD_METADATA[cli_field].name.data
				);

				break;
			}
		}
	}

	//
	// Carry out CLI execution.
	//

	if (cli_parsed)
	{
		struct StrBuf input_wildcard_path = StrBuf(256);
		strbuf_str (&input_wildcard_path, cli.input_wildcard_path);
		strbuf_char(&input_wildcard_path, '\0');

		str input_dir_path = input_wildcard_path.str;
		while (input_dir_path.length && (input_dir_path.data[input_dir_path.length - 1] != '/' || input_dir_path.data[input_dir_path.length - 1] == '\\'))
		{
			input_dir_path.length -= 1;
		}

		WIN32_FIND_DATAA finder_data   = {0};
		HANDLE           finder_handle = FindFirstFileA(input_wildcard_path.data, &finder_data);

		if (finder_handle == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_NOT_FOUND)
		{
			error_abort("`FindFirstFileA` failed with \"%s\".", input_wildcard_path.data);
		}

		struct StrBuf output_json_file_path = StrBuf(256);
		strbuf_str (&output_json_file_path, cli.output_json_file_path);
		strbuf_char(&output_json_file_path, '\0');

		FILE* output_json_file = fopen(output_json_file_path.data, "wb");
		if (!output_json_file)
		{
			error_abort("`fopen` failed with \"%s\".", output_json_file_path.data);
		}

		struct StrBuf json_output_buf = StrBuf(256);

		if
		(
			fprintf
			(
				output_json_file,
				"{\n\t\"avg_screenshot_rgbs\":\n\t\t[\n"
			) < 0
		)
		{
			error_abort("`fwrite` failed.");
		}

		if (finder_handle != INVALID_HANDLE_VALUE)
		{
			b32 first = true;

			do
			{
				//
				// Fetch file.
				//

				struct StrBuf input_file_path = StrBuf(256);
				strbuf_str(&input_file_path, input_dir_path);
				strbuf_str(&input_file_path, str_cstr(finder_data.cFileName));

				if
				(
					!(finder_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
					str_ends_with_caseless(input_file_path.str, str(".bmp"))
				)
				{
					struct BMP bmp = bmp_malloc_read_file(input_file_path.str);

					if (bmp.dim_x == PHONE_DIM_X && bmp.dim_y == PHONE_DIM_Y)
					{
						f64_3 avg_rgb = {0};
						for (i32 i = 0; i < bmp.dim_x * bmp.dim_y; i += 1)
						{
							avg_rgb.x += bmp.data[i].r;
							avg_rgb.y += bmp.data[i].g;
							avg_rgb.z += bmp.data[i].b;
						}
						avg_rgb.x /= 256.0 * bmp.dim_x * bmp.dim_y;
						avg_rgb.y /= 256.0 * bmp.dim_x * bmp.dim_y;
						avg_rgb.z /= 256.0 * bmp.dim_x * bmp.dim_y;

						printf
						(
							"%.*s : AvgRGB(%3d, %3d, %3d)\n",
							i32(input_file_path.length), input_file_path.data,
							i32(avg_rgb.x * 256.0),
							i32(avg_rgb.y * 256.0),
							i32(avg_rgb.z * 256.0)
						);

						if (first)
						{
							first = false;
						}
						else
						{
							if
							(
								fprintf
								(
									output_json_file,
									",\n"
								) < 0
							)
							{
								error_abort("`fwrite` failed.");
							}
						}

						if
						(
							fprintf
							(
								output_json_file,
								"\t\t\t{ \"name\": \"%s\", \"avg_rgb\": { \"r\": %f, \"g\": %f, \"b\": %f } }",
								finder_data.cFileName,
								avg_rgb.x,
								avg_rgb.y,
								avg_rgb.z
							) < 0
						)
						{
							error_abort("`fwrite` failed.");
						}
					}

					bmp_free_read_file(&bmp);
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
						error_abort("`FindNExtFileA` failed.");
					}
				}
			}
			while (true);

			if (!FindClose(finder_handle))
			{
				error_abort("`FindClose` failed.");
			}
		}

		if
		(
			fprintf
			(
				output_json_file,
				"\n\t\t]\n}\n"
			) < 0
		)
		{
			error_abort("`fwrite` failed.");
		}

		if (fclose(output_json_file))
		{
			error_abort("`fclose` failed with \"%s\".", output_json_file_path.data);
		}
	}
	else // Failed to parse CLI arguments.
	{
		str exe_name = argc ? str_cstr(argv[0]) : CLI_EXE_NAME;

		i64 margin_length = exe_name.length;
		for (enum CLIField cli_field = {0}; cli_field < CLIField_COUNT; cli_field += 1)
		{
			margin_length = i64_max(margin_length, CLI_FIELD_METADATA[cli_field].name.length);
		}

		//
		// CLI call structure.
		//

		printf("%.*s", i32(exe_name.length), exe_name.data);
		for (enum CLIField cli_field = {0}; cli_field < CLIField_COUNT; cli_field += 1)
		{
			printf(" [%.*s]", i32(CLI_FIELD_METADATA[cli_field].name.length), CLI_FIELD_METADATA[cli_field].name.data);
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
			printf
			(
				"\t[%.*s]%*s | %.*s\n",
				i32(CLI_FIELD_METADATA[cli_field].name.length), CLI_FIELD_METADATA[cli_field].name.data,
				i32(margin_length - (CLI_FIELD_METADATA[cli_field].name.length + 2)), "",
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
const WORDBITES_BOARD_SLOTS_X = 8;
const WORDBITES_BOARD_SLOTS_Y = 9;

let slots = [];

// wordbites_0.bmp
// let slots_of_interest = [{ x: 0, y: 2 }, { x: 0, y: 3 }, { x: 0, y: 6 }, { x: 1, y: 8 }, { x: 2, y: 2 }, { x: 2, y: 6 }, { x: 3, y: 0 }, { x: 3, y: 2 }, { x: 3, y: 6 }, { x: 4, y: 4 }, { x: 4, y: 8 }, { x: 5, y: 6 }, { x: 5, y: 8 }, { x: 6, y: 2 }, { x: 6, y: 4 }, { x: 6, y: 6 }].map(slot => slot.y * WORDBITES_BOARD_SLOTS_X + slot.x);

let slots_of_interest = [];

slots_of_interest_indices = slots_of_interest.map(slot => slot.y * WORDBITES_BOARD_SLOTS_X + slot.x)

let state = Calc.getState();
state.expressions =
	{
		list:
			[
				{
					type: "expression",
					latex: `P = \\left[${slots_of_interest_indices}\\right]`
				},
				{
					type: "expression",
					latex: `P_n = \\left[${new Array(WORDBITES_BOARD_SLOTS_X * WORDBITES_BOARD_SLOTS_Y).fill(null).map((_, i) => i).filter(x => slots_of_interest_indices.indexOf(x) == -1)}\\right]`
				},
				{
					type: "expression",
					color: "#FF0000",
					latex: `y = \\operatorname{mean}\\left(R\\left[P_n + 1\\right]\\right)`
				},
				{
					type: "expression",
					color: "#00FF00",
					latex: `y = \\operatorname{mean}\\left(G\\left[P_n + 1\\right]\\right)`
				},
				{
					type: "expression",
					color: "#0000FF",
					latex: `y = \\operatorname{mean}\\left(B\\left[P_n + 1\\right]\\right)`
				},
				{
					type: "table",
					columns:
						[
							{ values: slots.map((_, i) => i.toString()), latex: "I" },
							{ values: slots.map(slot => slot.r.toString()), color: "#FF0000", latex: "R" },
							{ values: slots.map(slot => slot.g.toString()), color: "#00FF00", latex: "G" },
							{ values: slots.map(slot => slot.b.toString()), color: "#0000FF", latex: "B" },
						]
				},
				{
					type: "expression",
					color: "#FF0000",
					pointStyle: "OPEN",
					latex: `\\left(P, R\\left[P + 1\\right]\\right)`
				},
				{
					type: "expression",
					color: "#00FF00",
					pointStyle: "OPEN",
					latex: `\\left(P, G\\left[P + 1\\right]\\right)`
				},
				{
					type: "expression",
					color: "#0000FF",
					pointStyle: "OPEN",
					latex: `\\left(P, B\\left[P + 1\\right]\\right)`
				}
			]
	};
Calc.setState(state)

///////////////////////////////

*/
