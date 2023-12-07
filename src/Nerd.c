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
#include <stdint.h>
#include <string.h>
#include "defs.h"
#include "W:/data/words_table_of_contents.h" // Generated by Microservices.c.
#include "pin.c"
#include "misc.c"
#include "usart.c"
#include "spi.c"
#include "sd.c"
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

int
main(void)
{
	#if DEBUG // Configure USART0 to have 250Kbps. See: Source(27) @ Table(22-12) @ Page(226).
		UCSR0A = 1 << U2X0;
		UBRR0  = 7;
		UCSR0B = 1 << TXEN0;
		debug_cstr("\n\n\n\n"); // If terminal is opened early, lots of junk could appear due to noise; this helps separate.
	#endif

	usart_init();
	spi_init();

	#if DEBUG // 8x8 LED dot matrix to have visual debug info. Must be initialized first before SD or else it messes with SPI communications...
		matrix_init();
	#endif

	sd_init();

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

		u8  sectors_per_cluster = boot_sector->BPB_SecPerClus;
		u32 root_cluster        = boot_sector->BPB_RootClus;
		u32 fat_address         = boot_sector_address + boot_sector->BPB_RsvdSecCnt;
		u32 data_region_address = fat_address + boot_sector->BPB_NumFATs * boot_sector->BPB_FATSz32;

		//
		// FAT32 short file name of word stream binary file.
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

		//
		// Search through root directory for the word stream binary files.
		//

		u32 word_stream_clusters[32] = {0}; // Zero is never a valid cluster.

		for
		(
			u32 cluster = root_cluster;
			(cluster & 0x0FFF'FFFF) != 0x0FFF'FFFF;
		)
		{
			for (u8 sector_index = 0; sector_index < sectors_per_cluster; sector_index += 1)
			{
				sd_read(data_region_address + (cluster - 2) * sectors_per_cluster + sector_index);

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
						word_stream_clusters[0] = (((u32) entry->DIR_FstClusHI) << 16) | entry->DIR_FstClusLO;

						debug_char ('"');
						debug_chars(word_stream_file_name, countof(word_stream_file_name));
						debug_cstr ("\" : ");
						debug_u64  (word_stream_clusters[0]);
						debug_char ('\n');

						goto STOP_SEARCHING_ROOT_DIRECTORY;
					}
				}
			}

			sd_read(fat_address + cluster * sizeof(u32) / FAT32_SECTOR_SIZE);
			cluster = ((u32*) sd_sector)[cluster % (FAT32_SECTOR_SIZE / sizeof(u32))];
		}
		STOP_SEARCHING_ROOT_DIRECTORY:;

		if (!word_stream_clusters[0])
		{
			error(); // We didn't find the word stream binary file!
		}
	}

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

//	//
//	// Wait for packet from Diplomat.
//	//
//
//	matrix_set
//	(
//		(((u64) 0b00000000) << (8 * 0)) |
//		(((u64) 0b00111110) << (8 * 1)) |
//		(((u64) 0b01000000) << (8 * 2)) |
//		(((u64) 0b01111100) << (8 * 3)) |
//		(((u64) 0b01000000) << (8 * 4)) |
//		(((u64) 0b00111110) << (8 * 5)) |
//		(((u64) 0b00000000) << (8 * 6)) |
//		(((u64) 0b00000000) << (8 * 7))
//	);
//
//	struct DiplomatPacket diplomat_packet = {0};
//	for (u8 i = 0; i < sizeof(diplomat_packet); i += 1)
//	{
//		((u8*) &diplomat_packet)[i] = usart_rx();
//	}
//
//	//
//	//
//	//
//
//	matrix_set
//	(
//		(((u64) 0b00000000) << (8 * 0)) |
//		(((u64) 0b00000000) << (8 * 1)) |
//		(((u64) 0b01111110) << (8 * 2)) |
//		(((u64) 0b00010010) << (8 * 3)) |
//		(((u64) 0b00010010) << (8 * 4)) |
//		(((u64) 0b00001100) << (8 * 5)) |
//		(((u64) 0b00000000) << (8 * 6)) |
//		(((u64) 0b00000000) << (8 * 7))
//	);
//
//	debug_cstr("WordGame: ");
//	debug_u64(diplomat_packet.wordgame);
//	debug_cstr("\n");
//
//	for
//	(
//		i8 y = WORDGAME_MAX_DIM_SLOTS_Y - 1;
//		y >= 0;
//		y -= 1
//	)
//	{
//		for (u8 x = 0; x < WORDGAME_MAX_DIM_SLOTS_X; x += 1)
//		{
//			debug_u64 (diplomat_packet.board[y][x]);
//			debug_char(' ');
//		}
//		debug_char('\n');
//	}
//
//	u8 counter = 0;
//	while (true)
//	{
//		if (usart_rx_available()) // Diplomat signaled that it's ready for next command?
//		{
//			counter += 1;
//
//			usart_rx(); // Eat dummy signal byte.
//			usart_tx(counter);
//			matrix_set
//			(
//				(
//					(((u64) 0b00000000) << (8 * 0)) |
//					(((u64) 0b00000000) << (8 * 1)) |
//					(((u64) 0b00100100) << (8 * 2)) |
//					(((u64) 0b01001010) << (8 * 3)) |
//					(((u64) 0b01010010) << (8 * 4)) |
//					(((u64) 0b00100100) << (8 * 5)) |
//					(((u64) 0b00000000) << (8 * 6)) |
//					(((u64) 0b00000000) << (8 * 7))
//				) + counter
//			);
//		}
//	}

	for(;;);
}
