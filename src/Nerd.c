#define F_CPU                16'000'000
#define PROGRAM_NERD         true
#define BOARD_MEGA_2560_REV3 true
#define PIN_MATRIX_SS        2
#define PIN_SD_SS            3
#define SPI_PRESCALER        SPIPrescaler_2
#define USART_N              3
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <util/crc16.h>
#include <stdint.h>
#include <string.h>
#include "defs.h"
#include "W:/data/words_table_of_contents.h" // Generated by Microservices.c.
#include "pin.c"
#include "misc.c"
#include "usart.c"
#include "spi.c"
#include "sd.c"
#include "timer.c"
#if DEBUG
	#include "matrix.c"
#endif

#if DEBUG
	static void
	debug_chars(char* data, u16 length)
	{
		for (u16 i = 0; i < length; i += 1)
		{
			while (!(UCSR0A & (1 << UDRE0))); // Wait for USART transmission buffer to free up.
			UDR0 = data[i];
		}
	}

	static void
	debug_dump(char* file_path, u16 line_number)
	{
		u64 bits            = ((u64) line_number) << 48;
		u8  file_name_index = 0;
		for (u8 i = 0; file_path[i]; i += 1)
		{
			if (file_path[i] == '/' || file_path[i] == '\\')
			{
				file_name_index = i + 1;
			}
		}
		for
		(
			u8 i = 0;
			i < 6 && file_path[file_name_index + i];
			i += 1
		)
		{
			bits |= ((u64) file_path[file_name_index + i]) << (i * 8);
		}
		matrix_init(); // Reinitialize matrix in case the dump occurred mid communication or we haven't had it initialized yet.
		matrix_set(bits);
		debug_cstr("\nDUMPED @ File(");
		debug_cstr(file_path);
		debug_cstr(") @ Line(");
		debug_u64(line_number);
		debug_cstr(").\n");
	}
#endif

#define WORDBITES_MAX_DUOS    5
#define ALPHABET_INDEX_TAKEN (1 << 7)
static_assert(BITS_PER_ALPHABET_INDEX < 8);

struct WordBitesPiece
{
	u8_2 position;
	u8   alphabet_indices[2];
};

struct WordBitesPieces
{
	struct WordBitesPiece unos[6];
	struct WordBitesPiece horts[WORDBITES_MAX_DUOS];
	struct WordBitesPiece verts[WORDBITES_MAX_DUOS];
	u8                    horts_count;
	u8                    verts_count;
};

static void
debug_piece(struct WordBitesPiece piece)
{
	debug_char('(');
	debug_u64(piece.position.x);
	debug_char(',');
	debug_u64(piece.position.y);
	debug_char(',');
	if (piece.alphabet_indices[0] & ALPHABET_INDEX_TAKEN)
	{
		debug_char(' ');
	}
	else
	{
		debug_char('A' + piece.alphabet_indices[0]);
	}
	if (piece.alphabet_indices[1] & ALPHABET_INDEX_TAKEN)
	{
		debug_char(' ');
	}
	else
	{
		debug_char('A' + piece.alphabet_indices[1]);
	}
	debug_char(')');
}

static void
debug_wordbites_board(u8 (*board_alphabet_indices)[WORDGAME_MAX_DIM_SLOTS_Y][WORDGAME_MAX_DIM_SLOTS_X], struct WordBitesPieces pieces)
{
	for
	(
		u8 y = pgm_u8(WORDGAME_INFO[WordGame_wordbites].dim_slots.y);
		y--;
	)
	{
		for (u8 x = 0; x < pgm_u8(WORDGAME_INFO[WordGame_wordbites].dim_slots.x); x += 1)
		{
			if ((*board_alphabet_indices)[y][x] & ALPHABET_INDEX_TAKEN)
			{
				debug_char('*');
			}
			else
			{
				debug_char('A' + (*board_alphabet_indices)[y][x]);
			}
			debug_char(' ');
		}
		debug_char('\n');
	}

	debug_cstr("UNOS  : ");
	for (u8 i = 0; i < countof(pieces.unos); i += 1)
	{
		debug_piece(pieces.unos[i]);
		debug_char(' ');
	}
	debug_char('\n');

	debug_cstr("HORTS : ");
	for (u8 i = 0; i < pieces.horts_count; i += 1)
	{
		debug_piece(pieces.horts[i]);
		debug_char(' ');
	}
	debug_char('\n');

	debug_cstr("VERTS : ");
	for (u8 i = 0; i < pieces.verts_count; i += 1)
	{
		debug_piece(pieces.verts[i]);
		debug_char(' ');
	}
	debug_char('\n');
}

static_assert(WORDGAME_MAX_DIM_SLOTS_X <= 8);
static_assert(WORDGAME_MAX_DIM_SLOTS_Y <= 15); // Can't be 16, since command of 0xFF is special.
/*
	"command" : 0bS'XXX'YYYY
	"S"       : In Anagrams and WordHunt, this should be set for the last letter of the word in order to submit it; irrelevant for WordBites.
	"X"       : X coordinate of the slot to select.
	"Y"       : Y coordinate of the slot to select.

	If command is NERD_COMMAND_COMPLETE, then this is a signal to the Diplomat that the Nerd is done processing.
	In WordBites, commands should be "paired up", where the first command is where the mouse should pick up the piece, and the second command is where the piece should be
	dropped off.
*/
static void
push_command(u8 command)
{
	if ((u8) (command_writer + 1) == command_reader) // Any open space in command buffer?
	{
		usart_rx();                               // Wait for the ready signal from the Diplomat.
		usart_tx(command_buffer[command_reader]); // Send out command.
		command_reader += 1;                      // A space opened up!
	}

	command_buffer[command_writer]  = command;
	command_writer                 += 1;
}

int
main(void)
{
	sei();

	#if DEBUG // Configure USART0 to have 250Kbps. See: Source(27) @ Table(22-12) @ Page(226).
		UCSR0A = 1 << U2X0;
		UBRR0  = 7;
		UCSR0B = 1 << TXEN0;
		debug_cstr("\n\n--------------------------------\n\n"); // If terminal is opened early, lots of junk could appear due to noise; this helps separate.
	#endif

	usart_init();
	spi_init();
	timer_init();

	#if DEBUG // 8x8 LED dot matrix to have visual debug info.
		matrix_init();
	#endif

	sd_init();

	//
	// Find sectors of the word stream binary file.
	//

	#if DEBUG
		matrix_set
		(
			(((u64) 0b00000000) << (8 * 0)) |
			(((u64) 0b00000000) << (8 * 1)) |
			(((u64) 0b01000010) << (8 * 2)) |
			(((u64) 0b01111110) << (8 * 3)) |
			(((u64) 0b01000010) << (8 * 4)) |
			(((u64) 0b00000000) << (8 * 5)) |
			(((u64) 0b00000000) << (8 * 6)) |
			(((u64) 0b00000000) << (8 * 7))
		);
	#endif

	u32 word_stream_cluster_addresses[32] = {0}; // Zero is never a valid cluster address.
	u8  sectors_per_cluster_exp           = 0;   // Max value is 7.

	{
		//
		// Read master boot record.
		//

		sd_read(0);
		struct MasterBootRecord* mbr = (struct MasterBootRecord*) &sd_sector;
		static_assert(sizeof(*mbr) == sizeof(sd_sector));

		if (mbr->partitions[0].partition_type != MASTER_BOOT_RECORD_PARTITION_TYPE_FAT32_LBA)
		{
			error(); // Unknown partition type..!
		}

		u32 boot_sector_address = mbr->partitions[0].fst_sector_address;

		//
		// Read FAT32 boot sector.
		//

		sd_read(boot_sector_address);
		struct FAT32BootSector* boot_sector = (struct FAT32BootSector*) &sd_sector;
		static_assert(sizeof(*boot_sector) == sizeof(sd_sector));

		if
		(
			!(
				boot_sector->BPB_BytsPerSec == FAT32_SECTOR_SIZE
				&& (boot_sector->BPB_SecPerClus & (boot_sector->BPB_SecPerClus - 1)) == 0
				&& boot_sector->BPB_RootEntCnt == 0
				&& boot_sector->BPB_TotSec16 == 0
				&& boot_sector->BPB_FATSz16 == 0
				&& boot_sector->BPB_FSVer == 0
				&& boot_sector->BS_Reserved == 0
				&& boot_sector->BS_BootSig == 0x29
				&& memeq("FAT32   ", boot_sector->BS_FilSysType, sizeof(boot_sector->BS_FilSysType))
				&& boot_sector->BS_BootSign == 0xAA55
			)
		)
		{
			error(); // Not the boot sector we're expecting...
		}

		for (u8 i = 0; i < 8; i += 1) // Get exponent of sectors per cluster.
		{
			if (boot_sector->BPB_SecPerClus & (1 << i))
			{
				sectors_per_cluster_exp = i;
				break;
			}
		}

		u32 root_cluster        = boot_sector->BPB_RootClus;
		u32 fat_address         = boot_sector_address + boot_sector->BPB_RsvdSecCnt;
		u32 data_region_address = fat_address + boot_sector->BPB_NumFATs * boot_sector->BPB_FATSz32;

		//
		// Search through root directory for the word stream binary file.
		//

		char word_stream_file_name[FAT32_SHORT_NAME_LENGTH + FAT32_SHORT_EXTENSION_LENGTH] = {0};
		memset(word_stream_file_name, ' ', sizeof(word_stream_file_name));

		static_assert(sizeof(WORD_STREAM_NAME) - 1 <= FAT32_SHORT_NAME_LENGTH);
		for (u8 i = 0; i < sizeof(WORD_STREAM_NAME) - 1; i += 1)
		{
			word_stream_file_name[i] = to_upper(pgm_char(PSTR(WORD_STREAM_NAME)[i]));
		}
		static_assert(sizeof(WORD_STREAM_EXTENSION) - 1 <= FAT32_SHORT_EXTENSION_LENGTH);
		for (u8 i = 0; i < sizeof(WORD_STREAM_EXTENSION) - 1; i += 1)
		{
			word_stream_file_name[FAT32_SHORT_NAME_LENGTH + i] = to_upper(pgm_char(PSTR(WORD_STREAM_EXTENSION)[i]));
		}

		for
		(
			u32 cluster = root_cluster;
			(cluster & 0x0FFF'FFFF) != 0x0FFF'FFFF;
		)
		{
			for (u8 sector_index = 0; sector_index < (1 << sectors_per_cluster_exp); sector_index += 1)
			{
				sd_read(data_region_address + ((cluster - 2) << sectors_per_cluster_exp) + sector_index);

				for (u8 entry_index = 0; entry_index < sizeof(sd_sector) / sizeof(union FAT32DirEntry); entry_index += 1)
				{
					// Short entries are only considered; long entries can be done, but that's a lot of complexity for not much.
					struct FAT32DirEntryShort* entry = &((union FAT32DirEntry*) &sd_sector)[entry_index].short_entry;
					static_assert(countof(word_stream_file_name) == countof(entry->DIR_Name));

					if (!entry->DIR_Name[0]) // Current and future entries are unallocated.
					{
						goto STOP_SEARCHING_ROOT_DIRECTORY;
					}
					else if
					(
						entry->DIR_Name[0] != 0xE5                                                                 // Entry is allocated.
						&& !(entry->DIR_Attr & ~(FAT32DirEntryAttrFlag_archive | FAT32DirEntryAttrFlag_read_only)) // Attributes indicate short entry of a file.
						&& memeq(word_stream_file_name, entry->DIR_Name, countof(entry->DIR_Name))                 // File name matches?
					)
					{
						u8                                     last_language_max_word_length = pgm_u8(WORDS_TABLE_OF_CONTENTS[Language_COUNT - 1].max_word_length);
						u8                                     last_language_alphabet_length = pgm_u8(LANGUAGE_INFO[Language_COUNT - 1].alphabet_length);
						const struct WordsTableOfContentEntry* last_entry                    =
							pgm_ptr(struct WordsTableOfContentEntry, WORDS_TABLE_OF_CONTENTS[Language_COUNT - 1].entries)
								+ (last_language_max_word_length - MIN_WORD_LENGTH + 1) * last_language_alphabet_length
								- 1;
						if
						(
							entry->DIR_FileSize !=
								((u32) pgm_u16(last_entry->sector_index)) * FAT32_SECTOR_SIZE
									+ pgm_u16(last_entry->count) * PACKED_WORD_SIZE(MIN_WORD_LENGTH)
						)
						{
							error(); // Binary file mismatching with our table of contents on flash...
						}

						// We'll later convert it to sector address.
						word_stream_cluster_addresses[0] = (((u32) entry->DIR_FstClusHI) << 16) | entry->DIR_FstClusLO;

						goto STOP_SEARCHING_ROOT_DIRECTORY;
					}
				}
			}

			sd_read(fat_address + cluster / FAT32_CLUSTER_ENTRIES_PER_TABLE_SECTOR);
			cluster = ((u32*) sd_sector)[cluster % FAT32_CLUSTER_ENTRIES_PER_TABLE_SECTOR];
		}
		STOP_SEARCHING_ROOT_DIRECTORY:;

		if (!word_stream_cluster_addresses[0])
		{
			error(); // We couldn't find the word stream binary file!
		}

		//
		// Get all clusters' sector addresses of the file.
		//

		for
		(
			u8 cluster_index = 0;
			;
			cluster_index += 1
		)
		{
			// Get sector of FAT corresponding to the current cluster to get the following cluster.
			sd_read(fat_address + word_stream_cluster_addresses[cluster_index] / FAT32_CLUSTER_ENTRIES_PER_TABLE_SECTOR);
			u32 next_cluster = ((u32*) sd_sector)[word_stream_cluster_addresses[cluster_index] % FAT32_CLUSTER_ENTRIES_PER_TABLE_SECTOR];

			// Convert cluster into sector address.
			word_stream_cluster_addresses[cluster_index] = data_region_address + ((word_stream_cluster_addresses[cluster_index] - 2) << sectors_per_cluster_exp);

			if (next_cluster == 0x0FFF'FFFF)
			{
				break; // End of cluster chain.
			}
			else if (cluster_index + 1 < (u8) countof(word_stream_cluster_addresses))
			{
				word_stream_cluster_addresses[cluster_index + 1] = next_cluster; // Will be converted to sector address on next iteration.
			}
			else
			{
				error(); // Too many clusters and not enough space!
			}
		}
	}

	//
	// Wait for packet from Diplomat.
	//

	#if DEBUG
		matrix_set
		(
			(((u64) 0b00000000) << (8 * 0)) |
			(((u64) 0b00111110) << (8 * 1)) |
			(((u64) 0b01000000) << (8 * 2)) |
			(((u64) 0b01111100) << (8 * 3)) |
			(((u64) 0b01000000) << (8 * 4)) |
			(((u64) 0b00111110) << (8 * 5)) |
			(((u64) 0b00000000) << (8 * 6)) |
			(((u64) 0b00000000) << (8 * 7))
		);
	#endif

	struct DiplomatPacket diplomat_packet = {0};
	for (u8 i = 0; i < sizeof(diplomat_packet); i += 1)
	{
		((u8*) &diplomat_packet)[i] = usart_rx();
	}

	//
	// Parse packet.
	//

	enum Language language                           = pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].language);
	u8            max_word_length                    = pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].max_word_length);
	b8            alphabet_used[MAX_ALPHABET_LENGTH] = {0};
	u8            board_alphabet_indices[WORDGAME_MAX_DIM_SLOTS_Y][WORDGAME_MAX_DIM_SLOTS_X] = {0};

	for (u8 y = 0; y < pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.y); y += 1)
	{
		for (u8 x = 0; x < pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.x); x += 1)
		{
			if (diplomat_packet.board[y][x])
			{
				b8 found = false;
				for (u8 alphabet_index = 0; alphabet_index < pgm_u8(LANGUAGE_INFO[language].alphabet_length); alphabet_index += 1)
				{
					if (pgm_u8(LANGUAGE_INFO[language].alphabet[alphabet_index]) == diplomat_packet.board[y][x])
					{
						found                         = true;
						alphabet_used[alphabet_index] = true;
						board_alphabet_indices[y][x]  = alphabet_index;
						break;
					}
				}
				if (!found)
				{
					error(); // OCR identified a letter that's not in the language's alphabet...
				}
			}
			else
			{
				board_alphabet_indices[y][x] = ALPHABET_INDEX_TAKEN; // Make slot unusable.
			}
		}
	}

	//
	// Find WordBites pieces if needed.
	//

	struct WordBitesPieces wordbites_pieces = {0};

	if (diplomat_packet.wordgame == WordGame_wordbites)
	{
		u8   unos_count         = 0;
		u8_2 wordgame_dim_slots =
			{
				pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.x),
				pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.y),
			};

		for (u8 y = 0; y < wordgame_dim_slots.y; y += 1)
		{
			for (u8 x = 0; x < wordgame_dim_slots.x; x += 1)
			{
				if
				(
					board_alphabet_indices[y][x] != ALPHABET_INDEX_TAKEN && // There's a letter on this slot!
					implies(x                           , board_alphabet_indices[y    ][x - 1] == ALPHABET_INDEX_TAKEN) && // Beginning of horizontal duo piece.
					implies(y + 1 < wordgame_dim_slots.y, board_alphabet_indices[y + 1][x    ] == ALPHABET_INDEX_TAKEN)    // Beginning of vertical duo piece.
				)
				{
					if (x + 1 < wordgame_dim_slots.x && board_alphabet_indices[y][x + 1] != ALPHABET_INDEX_TAKEN) // Horizontal duo piece!
					{
						if (wordbites_pieces.horts_count + wordbites_pieces.verts_count == WORDBITES_MAX_DUOS)
						{
							error(); // More pieces than expected...
						}

						wordbites_pieces.horts[wordbites_pieces.horts_count] =
							(struct WordBitesPiece)
							{
								.position.x          = x,
								.position.y          = y,
								.alphabet_indices[0] = board_alphabet_indices[y][x    ],
								.alphabet_indices[1] = board_alphabet_indices[y][x + 1],
							};
						wordbites_pieces.horts_count += 1;
					}
					else if (y && board_alphabet_indices[y - 1][x] != ALPHABET_INDEX_TAKEN) // Vertical piece!
					{
						if (wordbites_pieces.horts_count + wordbites_pieces.verts_count == WORDBITES_MAX_DUOS)
						{
							error(); // More pieces than expected...
						}

						wordbites_pieces.verts[wordbites_pieces.verts_count] =
							(struct WordBitesPiece)
							{
								.position.x          = x,
								.position.y          = y,
								.alphabet_indices[0] = board_alphabet_indices[y    ][x],
								.alphabet_indices[1] = board_alphabet_indices[y - 1][x],
							};
						wordbites_pieces.verts_count += 1;
					}
					else // Uno piece!
					{
						wordbites_pieces.unos[unos_count] =
							(struct WordBitesPiece)
							{
								.position.x          = x,
								.position.y          = y,
								.alphabet_indices[0] = board_alphabet_indices[y][x],
								.alphabet_indices[1] = ALPHABET_INDEX_TAKEN,
							};
						unos_count += 1;
					}
				}
			}
		}

		if (unos_count != countof(wordbites_pieces.unos) || wordbites_pieces.verts_count + wordbites_pieces.horts_count != WORDBITES_MAX_DUOS)
		{
			error(); // Less pieces than expected...
		}

		debug_wordbites_board(&board_alphabet_indices, wordbites_pieces);
	}

	//
	// Process for words.
	//

	#if DEBUG
		matrix_set
		(
			(((u64) 0b00000000) << (8 * 0)) |
			(((u64) 0b00000000) << (8 * 1)) |
			(((u64) 0b01111110) << (8 * 2)) |
			(((u64) 0b00010010) << (8 * 3)) |
			(((u64) 0b00010010) << (8 * 4)) |
			(((u64) 0b00001100) << (8 * 5)) |
			(((u64) 0b00000000) << (8 * 6)) |
			(((u64) 0b00000000) << (8 * 7))
		);
	#endif

	const struct WordsTableOfContentEntry* curr_table_entry = pgm_read_ptr(&WORDS_TABLE_OF_CONTENTS[language].entries);
	for
	(
		u8 entry_word_length = pgm_u8(WORDS_TABLE_OF_CONTENTS[language].max_word_length);
		entry_word_length >= MIN_WORD_LENGTH;
		entry_word_length -= 1
	)
	{
		for
		(
			u8 initial_alphabet_index = 0;
			initial_alphabet_index < pgm_u8(LANGUAGE_INFO[language].alphabet_length);
			initial_alphabet_index += 1,
			curr_table_entry += 1
		)
		{
			if (alphabet_used[initial_alphabet_index])
			{
				u16 curr_cluster_index        = pgm_u16(curr_table_entry->sector_index) >> sectors_per_cluster_exp;
				u8  curr_cluster_sector_index = pgm_u16(curr_table_entry->sector_index) & ((1 << sectors_per_cluster_exp) - 1);
				u16 entries_remaining         = pgm_u16(curr_table_entry->count);
				while (entries_remaining)
				{
					sd_read(word_stream_cluster_addresses[curr_cluster_index] + curr_cluster_sector_index);

					for
					(
						u16 sector_byte_index = 0;
						sector_byte_index + PACKED_WORD_SIZE(entry_word_length) <= FAT32_SECTOR_SIZE && entries_remaining;
					)
					{
						//
						// Get subword bitfield.
						//

						u16 subword_bitfield = pgm_u16(pgm_u16_ptr(WORDS_TABLE_OF_CONTENTS[language].subfields)[sd_sector[sector_byte_index]]);
						sector_byte_index += 1;

						//
						// Get alphabet indices.
						//

						// +2 since the unpacking process writes up to three elements at a time.
						u8 word_alphabet_indices[ABSOLUTE_MAX_WORD_LENGTH + 2] = { initial_alphabet_index };

						static_assert(BITS_PER_ALPHABET_INDEX == 5);
						for
						(
							u8 word_alphabet_indices_index = 1;
							word_alphabet_indices_index < entry_word_length;
							word_alphabet_indices_index += 3
						)
						{
							// This is technically invoking UB if we happen to be trying to read a u16 at the last byte of the sector,
							// but this shaves off some instructions, so I think it's worth it. AVR-GCC seems to be kinda dumb, so it's a safe bet for me!
							u16 packed = *(u16*) &sd_sector[sector_byte_index];

							word_alphabet_indices[word_alphabet_indices_index] = packed & 0b0001'1111;

							if (word_alphabet_indices_index + 1 < entry_word_length)
							{
								word_alphabet_indices[word_alphabet_indices_index + 1] = (packed >>  5) & 0b0001'1111;
								word_alphabet_indices[word_alphabet_indices_index + 2] = (packed >> 10) & 0b0001'1111;
								sector_byte_index += 2;
							}
							else
							{
								sector_byte_index += 1;
							}
						}

						//
						// Truncate the word to only contain the provided letters of the bank.
						//

						u8 subword_length = 1;
						while (subword_length < entry_word_length && subword_length < max_word_length && alphabet_used[word_alphabet_indices[subword_length]])
						{
							subword_length += 1;
						}

						//
						// Test for reproducibility of subwords.
						//

						u16 subword_bits = subword_bitfield << (15 - (subword_length - MIN_WORD_LENGTH));
						for
						(
							;
							subword_length >= MIN_WORD_LENGTH && subword_bits;
							subword_length -= 1
						)
						{
							if (subword_bits & (1 << 15))
							{
								switch (diplomat_packet.wordgame)
								{
									// An optimization can be made where if a subword can be reproduced,
									// then all of the subwords shorter than that can also be reproduced.
									// This optimization can be done, but there's already a huge bottleneck on the Diplomat's end
									// of executing these commands already, so it's not going to buy us much anyways.
									case WordGame_anagrams_english_6:
									case WordGame_anagrams_english_7:
									case WordGame_anagrams_russian:
									case WordGame_anagrams_french:
									case WordGame_anagrams_german:
									case WordGame_anagrams_spanish:
									case WordGame_anagrams_italian:
									{
										b8 reproducible                       = true;
										u8 commands[ABSOLUTE_MAX_WORD_LENGTH] = {0};

										//
										// Find matching slots.
										//

										for (u8 subword_letter_index = 0; subword_letter_index < subword_length; subword_letter_index += 1)
										{
											commands[subword_letter_index] = -1; // Assume no matching slot.

											for (u8 slot_index = 0; slot_index < pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.x); slot_index += 1)
											{
												if (board_alphabet_indices[0][slot_index] == word_alphabet_indices[subword_letter_index]) // Matching slot?
												{
													board_alphabet_indices[0][slot_index] |= ALPHABET_INDEX_TAKEN;
													commands[subword_letter_index]         = slot_index << 4;
													break;
												}
											}

											if (commands[subword_letter_index] == (u8) -1) // Didn't find a matching slot?
											{
												reproducible = false;
												break;
											}
										}

										//
										// Send commands for the slots.
										//

										if (reproducible)
										{
											commands[subword_length - 1] |= NERD_COMMAND_SUBMIT_BIT;
											for (u8 i = 0; i < subword_length; i += 1)
											{
												push_command(commands[i]);
											}
										}

										//
										// Clear the taken bits of the board.
										//

										for (u8 slot_index = 0; slot_index < pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.x); slot_index += 1)
										{
											board_alphabet_indices[0][slot_index] &= ~ALPHABET_INDEX_TAKEN;
										}
									} break;

									// Like with Anagrams, an optimization can be done where if subword is reproducible,
									// so are of the subwords shorter than it. This again can be done, but we're already bottlenecked by
									// the Diplomat taking a while to execute the commands already.
									case WordGame_wordhunt_4x4:
									case WordGame_wordhunt_o:
									case WordGame_wordhunt_x:
									case WordGame_wordhunt_5x5:
									{
										u8 commands[ABSOLUTE_MAX_WORD_LENGTH] = {0};

										//
										// Find starting point for the subword.
										//

										for (u8 start_y = 0; start_y < pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.y); start_y += 1)
										{
											for (u8 start_x = 0; start_x < pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.x); start_x += 1)
											{
												if (board_alphabet_indices[start_y][start_x] == word_alphabet_indices[0])
												{
													board_alphabet_indices[start_y][start_x] |= ALPHABET_INDEX_TAKEN;
													commands[0]                               = (start_x << 4) | start_y;

													//
													// Find the steps to go in.
													//

													u8 reproduced_subword_length = 1;
													i8 delta_x                   = -2;
													i8 delta_y                   = -1;
													while (true)
													{
														//
														// Get next step.
														//

														b8 found_next = false;
														u8 next_x     = 0;
														u8 next_y     = 0;
														while (true)
														{
															delta_x += 1;
															if (delta_x == 2)
															{
																delta_x = -1;
																delta_y += 1;
															}

															if (delta_y == 2)
															{
																break; // Exhausted all directions to move in.
															}
															else
															{
																next_x = NERD_COMMAND_X(commands[reproduced_subword_length - 1]) + delta_x;
																next_y = NERD_COMMAND_Y(commands[reproduced_subword_length - 1]) + delta_y;

																if // Within bounds and matches?
																(
																	next_x < pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.x) &&
																	next_y < pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.y) &&
																	board_alphabet_indices[next_y][next_x] == word_alphabet_indices[reproduced_subword_length]
																)
																{
																	found_next = true;
																	break;
																}
															}
														}

														//
														// Make move or backtrack.
														//

														if (found_next)
														{
															board_alphabet_indices[next_y][next_x] |= ALPHABET_INDEX_TAKEN;
															commands[reproduced_subword_length]     = (next_x << 4) | next_y;
															reproduced_subword_length              += 1;
															delta_x                                 = -2;
															delta_y                                 = -1;

															if (reproduced_subword_length == subword_length) // Reproduced entire subword.
															{
																break;
															}
														}
														else if (reproduced_subword_length == 1)
														{
															break; // Can't backtrack any further.
														}
														else // Backtrack.
														{
															board_alphabet_indices
																[NERD_COMMAND_Y(commands[reproduced_subword_length - 1])]
																[NERD_COMMAND_X(commands[reproduced_subword_length - 1])] &= ~ALPHABET_INDEX_TAKEN;
															delta_x = NERD_COMMAND_X(commands[reproduced_subword_length - 1]) - NERD_COMMAND_X(commands[reproduced_subword_length - 2]);
															delta_y = NERD_COMMAND_Y(commands[reproduced_subword_length - 1]) - NERD_COMMAND_Y(commands[reproduced_subword_length - 2]);
															reproduced_subword_length -= 1;
														}
													}

													//
													// Submit commands and clear taken bits.
													//

													if (reproduced_subword_length == subword_length)
													{
														commands[subword_length - 1] |= NERD_COMMAND_SUBMIT_BIT;
														for (u8 i = 0; i < subword_length; i += 1)
														{
															push_command(commands[i]);
															board_alphabet_indices[NERD_COMMAND_Y(commands[i])][NERD_COMMAND_X(commands[i])] &= ~ALPHABET_INDEX_TAKEN;
														}

														goto WORDHUNT_REPRODUCIBLE;
													}
													else
													{
														board_alphabet_indices[start_y][start_x] &= ~ALPHABET_INDEX_TAKEN;
													}
												}
											}
										}
										WORDHUNT_REPRODUCIBLE:;
									} break;

									case WordGame_wordbites:
									{
										struct WordBitesPiece* parallels       = wordbites_pieces.horts;
										u8                     parallels_count = wordbites_pieces.horts_count;
										struct WordBitesPiece* perps           = wordbites_pieces.verts;
										u8                     perps_count     = wordbites_pieces.verts_count;

										while (true) // Attempt to reproduce the word horizontally and vertically.
										{
											struct UnresolvedPerp
											{
												u8 goal_alphabet_index;
												u8 playing_piece_index;
												u8 resolving_perp_index;
											};

											b8                     reproducible                             = true;
											struct WordBitesPiece* playing_pieces[ABSOLUTE_MAX_WORD_LENGTH] = {0};
											u8                     playing_pieces_count                     = 0;
											struct UnresolvedPerp  unresolved_perps[WORDBITES_MAX_DUOS]     = {0};
											u8                     unresolved_perps_count                   = 0;

											if // Word can fit on board in this orientation?
											(
												parallels == wordbites_pieces.horts
													? subword_length <= pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.x)
													: subword_length <= pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.y)
											)
											{
												//
												// Find a piece for each letter of subword.
												//

												for
												(
													u8 subword_letter_index = 0;
													subword_letter_index < subword_length;
												)
												{
													//
													// See if there's a parallel duo piece that can match two letters at the same for the subword.
													//

													if (subword_letter_index + 1 < subword_length) // Can't do it the end when there's only one letter left!
													{
														for (u8 parallel_index = 0; parallel_index < parallels_count; parallel_index += 1)
														{
															if
															(
																parallels[parallel_index].alphabet_indices[0] == word_alphabet_indices[subword_letter_index    ] &&
																parallels[parallel_index].alphabet_indices[1] == word_alphabet_indices[subword_letter_index + 1]
															)
															{
																parallels[parallel_index].alphabet_indices[0] |= ALPHABET_INDEX_TAKEN; // Mark the piece as used!
																playing_pieces[playing_pieces_count]           = &parallels[parallel_index];
																break;
															}
														}
													}

													if (playing_pieces[playing_pieces_count]) // Found the parallel duo piece!
													{
														playing_pieces_count += 1;
														subword_letter_index += 2;
													}
													else
													{
														//
														// See if there's an uno piece that matches the current letter of the subword.
														//

														for (u8 uno_index = 0; uno_index < countof(wordbites_pieces.unos); uno_index += 1)
														{
															if (wordbites_pieces.unos[uno_index].alphabet_indices[0] == word_alphabet_indices[subword_letter_index])
															{
																wordbites_pieces.unos[uno_index].alphabet_indices[0] |= ALPHABET_INDEX_TAKEN; // Mark the piece as used!
																playing_pieces[playing_pieces_count]                  = &wordbites_pieces.unos[uno_index];
																break;
															}
														}

														if (playing_pieces[playing_pieces_count]) // Uno piece found!
														{
															playing_pieces_count += 1;
															subword_letter_index += 1;
														}
														else if (unresolved_perps_count < perps_count) // We mark the piece here to be resolved by a perpendicular piece later.
														{
															unresolved_perps[unresolved_perps_count] =
																(struct UnresolvedPerp)
																{
																	.goal_alphabet_index = word_alphabet_indices[subword_letter_index],
																	.playing_piece_index = playing_pieces_count,
																};
															unresolved_perps_count += 1;
															playing_pieces_count   += 1;
															subword_letter_index   += 1;
														}
														else // Too many unresolved pieces!
														{
															reproducible = false;
															break;
														}
													}
												}

												//
												// Match the unresolved perpendicular duo pieces.
												//

												if (reproducible)
												{
													u8 resolved_count = 0;
													u8 perp_index     = 0;
													while (true)
													{
														if (resolved_count == unresolved_perps_count) // We fixed every unresolved piece!
														{
															break;
														}
														else if (perp_index == perps_count)
														{
															if (resolved_count) // Backtrack.
															{
																resolved_count                                                                            -= 1;
																playing_pieces[unresolved_perps[resolved_count].playing_piece_index]->alphabet_indices[0] &= ~ALPHABET_INDEX_TAKEN;
																playing_pieces[unresolved_perps[resolved_count].playing_piece_index]                       = 0;
																perp_index                                                                                 = unresolved_perps[resolved_count].resolving_perp_index + 1;
															}
															else // Exhausted all combinations.
															{
																reproducible = false;
																break;
															}
														}
														else if // Matching perpendicular duo piece?
														(
															!(perps[perp_index].alphabet_indices[0] & ALPHABET_INDEX_TAKEN) &&
															(
																perps[perp_index].alphabet_indices[0] == unresolved_perps[resolved_count].goal_alphabet_index ||
																perps[perp_index].alphabet_indices[1] == unresolved_perps[resolved_count].goal_alphabet_index
															)
														)
														{
															perps[perp_index].alphabet_indices[0]                                |= ALPHABET_INDEX_TAKEN; // Mark piece as used!
															playing_pieces[unresolved_perps[resolved_count].playing_piece_index]  = &perps[perp_index];
															unresolved_perps[resolved_count].resolving_perp_index                 = perp_index;
															resolved_count                                                       += 1;
															perp_index                                                            = 0;
														}
														else // Try matching next one.
														{
															perp_index += 1;
														}
													}
												}

												//
												// Clear taken bits.
												//

												for (u8 playing_piece_index = 0; playing_piece_index < playing_pieces_count; playing_piece_index += 1)
												{
													if (playing_pieces[playing_piece_index])
													{
														playing_pieces[playing_piece_index]->alphabet_indices[0] &= ~ALPHABET_INDEX_TAKEN;
													}
												}
											}
											else // Word too long to fit on the board in this orientation.
											{
												reproducible = false;
											}

											//
											// Generate moves.
											//

											if (reproducible)
											{
												if (parallels == wordbites_pieces.horts)
												{
													debug_cstr("h ");
												}
												else
												{
													debug_cstr("v ");
												}
												for (u8 i = 0; i < subword_length; i += 1)
												{
													debug_char('A' + word_alphabet_indices[i]);
												}

												for (u8 playing_piece_index = 0; playing_piece_index < playing_pieces_count; playing_piece_index += 1)
												{
													debug_char(' ');
													if (playing_pieces[playing_piece_index])
													{
														struct WordBitesPiece piece = *playing_pieces[playing_piece_index];
														piece.alphabet_indices[0] &= ~ALPHABET_INDEX_TAKEN;
														debug_piece(piece);
													}
													else
													{
														debug_cstr("(?)");
													}
												}
												debug_char('\n');

												break;
											}
											else if (parallels == wordbites_pieces.horts) // Attempt again but make the word vertically.
											{
												parallels       = wordbites_pieces.verts;
												parallels_count = wordbites_pieces.verts_count;
												perps           = wordbites_pieces.horts;
												perps_count     = wordbites_pieces.horts_count;
											}
											else // We've already attempted making the word both vertically and horizontally.
											{
												break;
											}
										}
									} break;

									case WordGame_COUNT:
									{
										error(); // Impossible.
									} break;
								}
							}

							subword_bits <<= 1;
						}

						if (usart_rx_available() && command_reader != command_writer)
						{
							usart_rx();
							usart_tx(command_buffer[command_reader]);
							command_reader += 1;
						}

						entries_remaining -= 1;
					}

					curr_cluster_sector_index += 1;
					if (curr_cluster_sector_index == (1 << sectors_per_cluster_exp))
					{
						curr_cluster_index        += 1;
						curr_cluster_sector_index  = 0;
					}
				}
			}
		}
	}

	#if DEBUG
		matrix_set
		(
			(((u64) 0b00000000) << (8 * 0)) |
			(((u64) 0b00000000) << (8 * 1)) |
			(((u64) 0b01111110) << (8 * 2)) |
			(((u64) 0b01000010) << (8 * 3)) |
			(((u64) 0b01000010) << (8 * 4)) |
			(((u64) 0b00111100) << (8 * 5)) |
			(((u64) 0b00000000) << (8 * 6)) |
			(((u64) 0b00000000) << (8 * 7))
		);
	#endif

	push_command(NERD_COMMAND_COMPLETE);

	//
	// Send out remaining commands.
	//

	while (command_reader != command_writer)
	{
		usart_rx();
		usart_tx(command_buffer[command_reader]);
		command_reader += 1;
	}

	#if DEBUG
		matrix_set
		(
			(((u64) 0b00001000) << (8 * 0)) |
			(((u64) 0b00000100) << (8 * 1)) |
			(((u64) 0b00101000) << (8 * 2)) |
			(((u64) 0b01000000) << (8 * 3)) |
			(((u64) 0b01000000) << (8 * 4)) |
			(((u64) 0b00101000) << (8 * 5)) |
			(((u64) 0b00000100) << (8 * 6)) |
			(((u64) 0b00001000) << (8 * 7))
		);
	#endif

	for(;;);
}
