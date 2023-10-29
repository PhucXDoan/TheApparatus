#define false            0
#define true             1
#define stringify_(X)    #X
#define stringify(X)     stringify_(X)
#define concat_(X, Y)    X##Y
#define concat(X, Y)     concat_(X, Y)
#define countof(...)     (sizeof(__VA_ARGS__) / sizeof((__VA_ARGS__)[0]))
#define bitsof(...)      (sizeof(__VA_ARGS__) * 8)
#define implies(P, Q)    (!(P) || (Q))
#define static_assert(X) _Static_assert((X), #X)

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef int8_t   b8;
typedef int16_t  b16;
typedef int32_t  b32;
typedef int64_t  b64;

#define u8(...)  ((u8)  (__VA_ARGS__))
#define u16(...) ((u16) (__VA_ARGS__))
#define u32(...) ((u32) (__VA_ARGS__))
#define u64(...) ((u64) (__VA_ARGS__))
#define i8(...)  ((i8)  (__VA_ARGS__))
#define i16(...) ((i16) (__VA_ARGS__))
#define i32(...) ((i32) (__VA_ARGS__))
#define i64(...) ((i64) (__VA_ARGS__))
#define b8(...)  ((b8)  (__VA_ARGS__))
#define b16(...) ((b16) (__VA_ARGS__))
#define b32(...) ((b32) (__VA_ARGS__))
#define b64(...) ((b64) (__VA_ARGS__))

#if PROGRAM_MICROSERVIENT
	typedef float  f32;
	typedef double f64;
	#define f32(...) ((f32) (__VA_ARGS__))
	#define f64(...) ((f64) (__VA_ARGS__))

	typedef struct { char* data; i64 length; } str;
	#define str(STRLIT) (str) { (STRLIT), sizeof(STRLIT) - 1 }
	#define STR(STRLIT)       { (STRLIT), sizeof(STRLIT) - 1 }
#endif

static_assert(LITTLE_ENDIAN);

//
// "dary.c"
//

struct Dary_void
{
	void* data;
	i64   length;
	i64   size;
};

#define Dary_define(NAME, TYPE) \
	struct Dary_##NAME \
	{ \
		union \
		{ \
			struct \
			{ \
				TYPE* data; \
				i64   length; \
				i64   size; \
			}; \
			struct Dary_void dary_void; \
		}; \
	}
#define Dary_def(TYPE) Dary_define(TYPE, TYPE)

Dary_define(BMPMonochrome, struct BMPMonochrome);

//
// "strbuf.c"
//

#if PROGRAM_MICROSERVIENT
	#define StrBuf(SIZE) ((struct StrBuf) { .data = (char[SIZE]) {0}, .size = (SIZE) })
	struct StrBuf
	{
		union
		{
			struct
			{
				char* data;
				i64   length;
				i64   size;
			};
			str str;
		};
	};
#endif

//
// "bmp.c"
//

struct BMPPixel
{
	u8 r;
	u8 g;
	u8 b;
	u8 a;
};

struct BMP // Left-right, bottom-up.
{
	struct BMPPixel* data;
	i32              dim_x;
	i32              dim_y;
};

struct BMPMonochrome // Left-right, bottom-up, LSb first pixel. No padding except at the end of pixel data.
{
	u8* data;
	i32 dim_x; // Still in terms of pixels.
	i32 dim_y; // Still in terms of pixels.
};

struct BMPCIEXYZTRIPLE // "CIEXYZTRIPLE" in Windows's "wingdi.h".
{
	struct
	{
		u32 ciexyzX;
		u32 ciexyzY;
		u32 ciexyzZ;
	} ciexyzRed, ciexyzGreen, ciexyzBlue;
};

enum BMPCompression // See: Source(23) @ Section(2.1.1.7) @ Page(119).
{
	BMPCompression_BI_RGB       = 0x0,
	BMPCompression_BI_RLE8      = 0x1,
	BMPCompression_BI_RLE4      = 0x2,
	BMPCompression_BI_BITFIELDS = 0x3,
	BMPCompression_BI_JPEG      = 0x4,
	BMPCompression_BI_PNG       = 0x5,
	BMPCompression_BI_CMYK      = 0xB,
	BMPCompression_BI_CMYKRLE8  = 0xC,
	BMPCompression_BI_CMYKRLE4  = 0xD,
};

enum BMPColorSpace
{
	// See: "LogicalColorSpace" @ Source(23) @ Section(2.1.1.14) @ Page(127).
	BMPColorSpace_LCS_CALIBRATED_RGB      = 0x00000000,
	BMPColorSpace_LCS_sRGB                = 0x73524742,
	BMPColorSpace_LCS_WINDOWS_COLOR_SPACE = 0x57696E20,

	// See: "LogicalColorSpaceV5" @ Source(23) @ Section(2.1.1.15) @ Page(128).
	BMPColorSpace_LCS_PROFILE_LINKED   = 0x4C494E4B,
	BMPColorSpace_LCS_PROFILE_EMBEDDED = 0x4D424544,
};

#pragma pack(push, 1)
struct BMPFileHeader // See: "BITMAPFILEHEADER" @ Source(22) @ Page(281).
{
	u16 bfType;      // Must be 'B' followed by 'M', i.e. 0x4D42.
	u32 bfSize;      // Size of the entire BMP file in bytes.
	u16 bfReserved1;
	u16 bfReserved2;
	u32 bfOffBits;   // Byte offset into the BMP file where the image's pixel data is stored.
};
#pragma pack(pop)

struct BMPRGBQuad // See: "RGBQUAD" @ Source(22) @ Page(1344).
{
	u8 rgbBlue;
	u8 rgbGreen;
	u8 rgbRed;
	u8 rgbReserved;
};

#pragma pack(push, 1)
union BMPDIBHeader
{
	u32 size; // Helps determine which of the following headers below are used.

	struct BMPDIBHeaderCore // See: "BITMAPCOREHEADER" @ Source(22) @ Page(277).
	{
		u32 bcSize;    // Must be sizeof(struct BMPDIBHeaderCore).
		u16 bcWidth;
		u16 bcHeight;
		u16 bcPlanes;
		u16 bcBitCount;
	} core;

	struct BMPDIBHeaderInfo // See: "BITMAPINFOHEADER" @ Source(22) @ Page(287).
	{
		u32 biSize;          // Must be sizeof(struct BMPDIBHeaderInfo).
		i32 biWidth;         // Width of image after decompression.
		i32 biHeight;        // Generally, height of image where positive is bottom-up and negative is top-down, but check documentation.
		u16 biPlanes;        // Must be 1.
		u16 biBitCount;      // Bits per pixel.
		u32 biCompression;   // Either BMPCompression_BI_RGB or BMPCompression_BI_BITFIELDS.
		u32 biSizeImage;     // Byte count of the image; can be zero for uncompressed RGB images.
		i32 biXPelsPerMeter; // Irrelevant.
		i32 biYPelsPerMeter; // Irrelevant.
		u32 biClrUsed;       // Colors used in color table; zero means the colors used is determined with 2^biBitCount.
		u32 biClrImportant;  // Pretty much irrelevant.
	} info;

	struct BMPDIBHeaderV4 // See: "BITMAPV4HEADER" @ Source(22) @ Page(293).
	{
		u32                    bV4Size;          // Must be sizeof(struct BMPDIBHeaderV4).
		i32                    bV4Width;         // Width of image after decompression.
		i32                    bV4Height;        // Height of image where positive is bottom-up and negative is top-down. Must conform to bV5Compression's restrictions.
		u16                    bV4Planes;        // Must be 1.
		u16                    bV4BitCount;      // Bits per pixel.
		u32                    bV4Compression;   // Aliasing enum BMPCompression.
		u32                    bV4SizeImage;     // Meaning varies depending on the compression method used.
		i32                    bV4XPelsPerMeter; // Irrelevant.
		i32                    bV4YPelsPerMeter; // Irrelevant.
		u32                    bV4ClrUsed;       // Colors used in color table.
		u32                    bV4ClrImportant;  // Pretty much irrelevant.
		u32                    bV4RedMask;       // When bV5Compression is CompressionMethod_BI_BITFIELDS, bit mask for the red channel.
		u32                    bV4GreenMask;     // When bV5Compression is CompressionMethod_BI_BITFIELDS, bit mask for the green channel.
		u32                    bV4BlueMask;      // When bV5Compression is CompressionMethod_BI_BITFIELDS, bit mask for the blue channel.
		u32                    bV4AlphaMask;     // Bit mask for the alpha channel; doesn't say anything about bV5Compression though...
		u32                    bV4CSType;        // Minor; if we want to be extremely color-correct, we should probably consider this, but nah...
		struct BMPCIEXYZTRIPLE bV4Endpoints;     // Minor; stuff with color spaces.
		u32                    bV4GammaRed;      // Minor.
		u32                    bV4GammaGreen;    // Minor.
		u32                    bV4GammaBlue;     // Minor.
	} v4;

	struct BMPDIBHeaderV5 // See: "BITMAPV5HEADER" @ Source(22) @ Page(300).
	{
		u32                    bV5Size;          // Must be sizeof(struct BMPDIBHeaderV5).
		i32                    bV5Width;         // Width of image after decompression.
		i32                    bV5Height;        // Height of image where positive is bottom-up and negative is top-down. Must conform to bV5Compression's restrictions.
		u16                    bV5Planes;        // Must be 1.
		u16                    bV5BitCount;      // Bits per pixel.
		u32                    bV5Compression;   // Aliasing enum BMPCompression.
		u32                    bV5SizeImage;     // Meaning varies depending on the compression method used.
		i32                    bV5XPelsPerMeter; // Irrelevant.
		i32                    bV5YPelsPerMeter; // Irrelevant.
		u32                    bV5ClrUsed;       // Colors used in color table.
		u32                    bV5ClrImportant;  // Pretty much irrelevant.
		u32                    bV5RedMask;       // When bV5Compression is CompressionMethod_BI_BITFIELDS, bit mask for the red channel.
		u32                    bV5GreenMask;     // When bV5Compression is CompressionMethod_BI_BITFIELDS, bit mask for the green channel.
		u32                    bV5BlueMask;      // When bV5Compression is CompressionMethod_BI_BITFIELDS, bit mask for the blue channel.
		u32                    bV5AlphaMask;     // Bit mask for the alpha channel; doesn't say anything about bV5Compression though...
		u32                    bV5CSType;        // Minor; if we want to be extremely color-correct, we should probably consider this, but nah...
		struct BMPCIEXYZTRIPLE bV5Endpoints;     // Minor; stuff with color spaces.
		u32                    bV5GammaRed;      // Minor.
		u32                    bV5GammaGreen;    // Minor.
		u32                    bV5GammaBlue;     // Minor.
		u32                    bV5Intent;        // Minor.
		u32                    bV5ProfileData;   // Minor.
		u32                    bV5ProfileSize;   // Minor.
		u32                    bV5Reserved;
	} v5;
};
#pragma pack(pop)

//
// "pin.c"
//

enum HaltSource
{
	HaltSource_diplomat     = 0,
	HaltSource_diplomat_usb = 1,
	HaltSource_nerd         = 2,
	HaltSource_sd           = 3,
};

#if __AVR_ATmega32U4__
	#if BOARD_LEONARDO
		#define PIN_XMDT(X) /* See: Source(3). */ \
			X(0 , D, 2) X(1 , D, 3) X(2 , D, 1) X(3 , D, 0) X(4 , D, 4) X(5 , C, 6) X(6 , D, 7) \
			X(7 , E, 6) X(8 , B, 4) X(9 , B, 5) X(10, B, 6) X(11, B, 7) X(12, D, 6) X(13, C, 7) \
			X(A0, F, 7) X(A1, F, 6) X(A2, F, 5) X(A3, F, 4) X(A4, F, 1) X(A5, F, 0) \
			X(SPI_SS, B, 0) X(SPI_CLK, B, 1) X(SPI_MOSI, B, 2) X(SPI_MISO, B, 3) \
			X(HALT, D, 5)
	#endif

	#if BOARD_PRO_MICRO
		#define PIN_XMDT(X) /* Pin assignments are pretty similar to the Arduino Leonardo. */ \
			X(2     , D, 1) X(3      , D, 0) X(4       , D, 4) X(5       , C, 6) \
			X(6     , D, 7) X(7      , E, 6) X(8       , B, 4) X(9       , B, 5) \
			X(10    , B, 6) X(14     , B, 3) X(15      , B, 1) X(16      , B, 2) \
			X(A0    , F, 7) X(A1     , F, 6) X(A2      , F, 5) X(A3      , F, 4) \
			X(SPI_SS, B, 0) X(SPI_CLK, B, 1) X(SPI_MOSI, B, 2) X(SPI_MISO, B, 3) \
			X(HALT  , D, 5)
	#endif
#endif

#if __AVR_ATmega2560__
	#if BOARD_MEGA2560
		#define PIN_XMDT(X) /* See: Source(18). */ \
			X(0 , E, 0) X(1 , E, 1) X(2 , E, 4) X(3 , E, 5) X( 4, G, 5) X(5 , E, 3) \
			X(6 , H, 3) X(7 , H, 4) X(8 , H, 5) X(9 , H, 6) X(10, B, 4) X(11, B, 5) \
			X(12, B, 6) X(13, B, 7) X(14, J, 1) X(15, J, 0) X(16, H, 1) X(17, H, 0) \
			X(18, D, 3) X(19, D, 2) X(20, D, 1) X(21, D, 0) X(22, A, 0) X(23, A, 1) \
			X(24, A, 2) X(25, A, 3) X(26, A, 4) X(27, A, 5) X(28, A, 6) X(29, A, 7) \
			X(30, C, 7) X(31, C, 6) X(32, C, 5) X(33, C, 4) X(34, C, 3) X(35, C, 2) \
			X(36, C, 1) X(37, C, 0) X(38, D, 7) X(39, G, 2) X(40, G, 1) X(41, G, 0) \
			X(42, L, 7) X(43, L, 6) X(44, L, 5) X(45, L, 4) X(46, L, 3) X(47, L, 2) \
			X(48, L, 1) X(49, L, 0) X(50, B, 3) X(51, B, 2) X(52, B, 1) X(53, B, 0) \
			X(SPI_SS, B, 0) X(SPI_CLK, B, 1) X(SPI_MOSI, B, 2) X(SPI_MISO, B, 3) \
			X(HALT, B, 7)
	#endif
#endif

//
// "timer.c"
//

enum TimerPrescaler // Prescalers for Timer0's TCCR0B register. See: Source(1) @ Table(13-8) @ Page(108).
{
	//                          "CS02".
	//                          | "CS01".
	//                          | | "CS00".
	//                          | | |
	//                          v v v
	TimerPrescaler_no_clk   = 0b0'0'0,
	TimerPrescaler_1        = 0b0'0'1,
	TimerPrescaler_8        = 0b0'1'0,
	TimerPrescaler_64       = 0b0'1'1,
	TimerPrescaler_256      = 0b1'0'0,
	TimerPrescaler_1024     = 0b1'0'1,
	TimerPrescaler_ext_fall = 0b1'1'0,
	TimerPrescaler_ext_rise = 0b1'1'1,
};

#define TIMER_INITIAL_COUNTER 6 // See: [Overview] @ "timer.c".

#if 0
	static volatile u32 _timer_ms = 0;
#endif

//
// "spi.c"
//

enum SPIPrescaler // See: Source(1) @ Table(17-5) @ Page(186).
{
	//                   "SPI2X" bit in "SPSR".
	//                   | "SPR1" bit in "SPCR".
	//                   | | "SPR0" bit in "SPCR".
	//                   | | |
	//                   v v v
	SPIPrescaler_4   = 0b0'0'0,
	SPIPrescaler_16  = 0b0'0'1,
	SPIPrescaler_64  = 0b0'1'0,
	SPIPrescaler_128 = 0b0'1'1,
	SPIPrescaler_2   = 0b1'0'0,
	SPIPrescaler_8   = 0b1'0'1,
	SPIPrescaler_32  = 0b1'1'0,
	SPIPrescaler_64_ = 0b1'1'1, // Equivalent to SPIPrescaler_64 (0b0'1'0).
};

#define SPI_PRESCALER SPIPrescaler_2

//
// FAT32.
//

// Arbitrary values really; mostly taken from how Windows formats the boot sector.
// With only one FAT, the reported drive capacity should be: (FAT32_TOTAL_SECTOR_COUNT - FAT32_RESERVED_SECTOR_COUNT - FAT32_TABLE_SECTOR_COUNT) * FAT32_SECTOR_SIZE.
#define FAT32_TOTAL_SECTOR_COUNT    16777216
#define FAT32_RESERVED_SECTOR_COUNT 6144
#define FAT32_TABLE_SECTOR_COUNT    1024
#define FAT32_SECTOR_SIZE           512
#define FAT32_SECTORS_PER_CLUSTER   128

#define FAT32_MEDIA_TYPE                        0xF8
#define FAT32_BACKUP_BOOT_SECTOR_OFFSET         6
#define FAT32_FILE_STRUCTURE_INFO_SECTOR_OFFSET 1
#define FAT32_ROOT_CLUSTER                      2

// Includes the reserved region, the FAT itself, and the root cluster.
#define FAT32_WIPE_SECTOR_COUNT FAT32_RESERVED_SECTOR_COUNT + FAT32_TABLE_SECTOR_COUNT + FAT32_SECTORS_PER_CLUSTER

static_assert(FAT32_SECTORS_PER_CLUSTER && !(FAT32_SECTORS_PER_CLUSTER & (FAT32_SECTORS_PER_CLUSTER - 1)) && FAT32_SECTORS_PER_CLUSTER <= 128); // See: "BPB_SecPerClus" @ Source(15) @ Section(3.1) @ Page(8).
static_assert((FAT32_TOTAL_SECTOR_COUNT - FAT32_RESERVED_SECTOR_COUNT - FAT32_TABLE_SECTOR_COUNT) / FAT32_SECTORS_PER_CLUSTER >= 65'525);       // See: Source(15) @ Section(3.5) @ Page(15).

struct FAT32BootSector // See: Source(15) @ Section(3.1) @ Page(7-9) & Source(15) @ Section(3.3) @ Page(11-12).
{
	u8   BS_jmpBoot[3];    // Best as { 0xEB, 0x00, 0x90 }.
	char BS_OEMName[8];    // Best as "MSWIN4.1". See: "Boot Sector and BPB" @ Source(17).
	u16  BPB_BytsPerSec;   // Must be 512, 1024, 2048, or 4096 bytes per sector.
	u8   BPB_SecPerClus;   // Must be power of two no greater than 128.
	u16  BPB_RsvdSecCnt;   // Must be non-zero.
	u8   BPB_NumFATs;      // Fine as 1.
	u16  BPB_RootEntCnt;   // Must be zero.
	u16  BPB_TotSec16;     // Must be zero.
	u8   BPB_Media;        // Best as 0xF8 for "fixed (non-removable) media"; must be also in lower 8-bits of the first FAT entry.
	u16  BPB_FATSz16;      // Must be zero.
	u16  BPB_SecPerTrk;    // Irrelevant.
	u16  BPB_NumHeads;     // Irrelevant.
	u32  BPB_HiddSec;      // Seems irrelevant; best as 0.
	u32  BPB_TotSec32;     // Must be non-zero.
	u32  BPB_FATSz32;      // Sectors per FAT.
	u16  BPB_ExtFlags;     // Seems irrelevant; best as 0.
	u16  BPB_FSVer;        // Must be zero.
	u32  BPB_RootClus;     // Cluster number of the first cluster of the root directory; best as 2.
	u16  BPB_FSInfo;       // Sector of the "FSINFO"; usually 1.
	u16  BPB_BkBootSec;    // Sector of the duplicated boot record; best as 6.
	u8   BPB_Reserved[12]; // Must be zero.
	u8   BS_DrvNum;        // Seems irrelevant; best as 0x80.
	u8   BS_Reserved;      // Must be zero.
	u8   BS_BootSig;       // Must be 0x29.
	u32  BS_VolID;         // Irrelevant.
	char BS_VolLab[11];    // Essentially a name.
	char BS_FilSysType[8]; // Must be "FAT32   ".
	u8   BS_BootCode[420]; // Irrelevant.
	u16  BS_BootSign;      // Must be 0xAA55.
};

struct FAT32FileStructureInfo // See: Source(15) @ Page(21-22).
{
	u32 FSI_LeadSig;        // Must be 0x41615252.
	u8  FSI_Reserved1[480];
	u32 FSI_StrucSig;       // Must be 0x61417272.
	u32 FSI_Free_Count;     // Last known free cluster; best as 0xFFFFFFFF to signify unknown.
	u32 FSI_Nxt_Free;       // Hints the FAT driver of where to look for the next free cluster; best as 0xFFFFFFFF to signify no hint.
	u8  FSI_Reserved2[12];
	u32 FSI_TrailSig;       // Must be 0xAA550000.
};

#if PROGRAM_DIPLOMAT
	static const struct FAT32BootSector FAT32_BOOT_SECTOR PROGMEM =
		{
			.BS_jmpBoot     = { 0xEB, 0x00, 0x90 },
			.BS_OEMName     = "MSWIN4.1",
			.BPB_BytsPerSec = FAT32_SECTOR_SIZE,
			.BPB_SecPerClus = FAT32_SECTORS_PER_CLUSTER,
			.BPB_RsvdSecCnt = FAT32_RESERVED_SECTOR_COUNT,
			.BPB_NumFATs    = 1,
			.BPB_Media      = FAT32_MEDIA_TYPE,
			.BPB_TotSec32   = 0x01000000,
			.BPB_FATSz32    = FAT32_TABLE_SECTOR_COUNT,
			.BPB_RootClus   = FAT32_ROOT_CLUSTER,
			.BPB_FSInfo     = FAT32_FILE_STRUCTURE_INFO_SECTOR_OFFSET,
			.BPB_BkBootSec  = FAT32_BACKUP_BOOT_SECTOR_OFFSET,
			.BS_DrvNum      = 0x80,
			.BS_BootSig     = 0x29,
			.BS_VolID       = (u32('B') << 0) | (u32('o') << 8) | (u32('o') << 16) | (u32('b') << 24),
			.BS_VolLab      = "Le Diplomat",
			.BS_FilSysType  = "FAT32   ",
			.BS_BootSign    = 0xAA55,
		};

	static const struct FAT32FileStructureInfo FAT32_FILE_STRUCTURE_INFO PROGMEM =
		{
			.FSI_LeadSig    = 0x41615252,
			.FSI_StrucSig   = 0x61417272,
			.FSI_Free_Count = 0xFFFFFFFF,
			.FSI_Nxt_Free   = 0xFFFFFFFF,
			.FSI_TrailSig   = 0xAA550000,
		};

	static const u32 FAT32_TABLE[128] PROGMEM = // Most significant nibbles are reserved. See: Source(15) @ Section(4) @ Page(16).
		{
			[0]                  = 0x0'FFFFF'00 | FAT32_MEDIA_TYPE, // See: Source(15) @ Section(4.2) @ Page(19).
			[1]                  = 0xF'FFFFFFF,                     // For format utilities. Seems to be commonly always all set. See: Source(15) @ Section(4.2) @ Page(19).
			[FAT32_ROOT_CLUSTER] = 0x0'FFFFFFF,
		};

	#define FAT32_SECTOR_XMDT(X) \
		X(FAT32_BOOT_SECTOR        , 0) \
		X(FAT32_FILE_STRUCTURE_INFO, 1) \
		X(FAT32_BOOT_SECTOR        , 6) \
		X(FAT32_FILE_STRUCTURE_INFO, 7) \
		X(FAT32_TABLE              , FAT32_RESERVED_SECTOR_COUNT)

	#define MAKE(SECTOR_DATA, SECTOR_ADDRESS) static_assert(sizeof(SECTOR_DATA) == 512);
	FAT32_SECTOR_XMDT(MAKE)
	#undef MAKE
#endif

//
// "sd.c"
//

enum SDCommand // Non-exhaustive. See: Source(19) @ Section(7.3.1.3) @ AbsPage(113-117).
{
	SDCommand_GO_IDLE_STATE     = 0,
	SDCommand_SEND_IF_COND      = 8,
	SDCommand_SEND_CSD          = 9,
	SDCommand_READ_SINGLE_BLOCK = 17,
	SDCommand_WRITE_BLOCK       = 24,
	SDCommand_APP_CMD           = 55,
	SDCommand_SD_SEND_OP_COND   = 41, // Application-specific.
};

enum SDR1ResponseFlag // See: Source(19) @ Figure(7-9) @ AbsPage(120).
{
	SDR1ResponseFlag_in_idle_state        = 1 << 0,
	SDR1ResponseFlag_erase_reset          = 1 << 1,
	SDR1ResponseFlag_illegal_command      = 1 << 2,
	SDR1ResponseFlag_com_crc_error        = 1 << 3,
	SDR1ResponseFlag_erase_sequence_error = 1 << 4,
	SDR1ResponseFlag_address_error        = 1 << 5,
	SDR1ResponseFlag_parameter_error      = 1 << 6,
};

#if PROGRAM_DIPLOMAT
	static u8 sd_sector[FAT32_SECTOR_SIZE] = {0};
#endif

//
// "Diplomat_usb.c"
//

#if DEBUG // Used to disable some USB functionalities for development purposes, but does not necessairly remove all data and control flow.
	#define USB_CDC_ENABLE true
	#define USB_HID_ENABLE true
	#define USB_MS_ENABLE  true
#else
	#define USB_CDC_ENABLE false
	#define USB_HID_ENABLE true
	#define USB_MS_ENABLE  true
#endif

enum USBEndpointSizeCode // See: Source(1) @ Section(22.18.2) @ Page(287).
{
	USBEndpointSizeCode_8   = 0b000,
	USBEndpointSizeCode_16  = 0b001,
	USBEndpointSizeCode_32  = 0b010,
	USBEndpointSizeCode_64  = 0b011,
	USBEndpointSizeCode_128 = 0b100, // See: [Endpoint Sizes] @ "Diplomat_usb.c".
	USBEndpointSizeCode_256 = 0b101, // See: [Endpoint Sizes] @ "Diplomat_usb.c".

	// See: [Endpoint Sizes] @ "Diplomat_usb.c".
	// USBEndpointSizeCode_512 = 0b110,
};

// See: ATmega32U4's Bitcodes @ Source(1) @ Section(22.18.2) @ Page(286).
// See: USB 2.0's Bitcodes @ Source(2) @ Table(9-13) @ Page(270).
// See: Differences Between Endpoint Transfer Types @ Source(2) @ Section(4.7) @ Page(20-21).
enum USBEndpointTransferType
{
	USBEndpointTransferType_control     = 0b00, // Guaranteed data delivery. Used by the USB standard to configure devices, but is open to other implementors.
	USBEndpointTransferType_isochronous = 0b01, // Bounded latency, one-way, guaranteed data bandwidth (but no guarantee of successful delivery). Ex: audio/video.
	USBEndpointTransferType_bulk        = 0b10, // No latency guarantees, one-way, large amount of data. Ex: file upload.
	USBEndpointTransferType_interrupt   = 0b11, // Low-latency, one-way, small amount of data. Ex: keyboard.
};

enum USBFeatureSelector // See: "Standard Feature Selectors" @ Source(2) @ Table(9-6) @ Page(252).
{
	USBFeatureSelector_endpoint_halt        = 0,
	USBFeatureSelector_device_remote_wakeup = 1,
	USBFeatureSelector_device_test_mode     = 2,
};

enum USBClass // Non-exhaustive. See: Source(5).
{
	USBClass_null     = 0x00, // See: [About: USBClass_null].
	USBClass_cdc      = 0x02, // See: "Communication Class Device" @ Source(6) @ Section(1) @ AbsPage(11).
	USBClass_hid      = 0x03, // See: "Human-Interface Device" @ Source(7) @ Section(3) @ AbsPage(19).
	USBClass_ms       = 0x08, // See: "USB Mass Storage Device" @ Source(11) @ Section(1.1) @ AbsPage(1).
	USBClass_cdc_data = 0x0A, // See: "Data Interface Class" @ Source(6) @ Section(1) @ AbsPage(11).
	USBClass_misc     = 0XEF, // For IADs. See: Source(9) @ Table(1-1) @ AbsPage(5).
};

enum USBSetupRequestKind // "bmRequestType" in LSB and "bRequest" in MSB. See: Source(2) @ Table(9-2) @ Page(248).
{
	#define MAKE(BM_REQUEST_TYPE, B_REQUEST) ((u16(B_REQUEST) << 8) | (BM_REQUEST_TYPE))

	// Non-exhaustive.
	// See: "Standard Device Requests" @ Source(2) @ Table(9-3) @ Page(250)
	// See: "Standard Request Codes" @ Source(2) @ Table(9-4) @ Page(251).
	USBSetupRequestKind_endpoint_clear_feature = MAKE(0b0'00'00010, 1),
	USBSetupRequestKind_set_address            = MAKE(0b0'00'00000, 5),
	USBSetupRequestKind_get_desc               = MAKE(0b1'00'00000, 6),
	USBSetupRequestKind_set_config             = MAKE(0b0'00'00000, 9),
	USBSetupRequestKind_interface_get_status   = MAKE(0b1'00'00010, 0),

	// Non-exhaustive.
	// See: CDC-Specific Requests @ Source(6) @ Table(44) @ AbsPage(62-63).
	// See: CDC-Specific Request Codes @ Source(6) @ Table(46) @ AbsPage(64-65).
	USBSetupRequestKind_cdc_set_line_coding        = MAKE(0b0'01'00001, 0x20),
	USBSetupRequestKind_cdc_get_line_coding        = MAKE(0b1'01'00001, 0x21),
	USBSetupRequestKind_cdc_set_control_line_state = MAKE(0b0'01'00001, 0x22),

	// Non-exhaustive. See: HID-Specific Requests @ Source(7) @ Section(7.1) @ AbsPage(58-63).
	USBSetupRequestKind_hid_get_desc = MAKE(0b1'00'00001, 6   ), // Request code is from the standard device codes.
	USBSetupRequestKind_hid_set_idle = MAKE(0b0'01'00001, 0x0A),

	// Non-exhuastive. See: MS-Specific Requests @ Source(12) @ Section(3) @ Page(7).
	USBSetupRequestKind_ms_get_max_lun = MAKE(0b1'01'00001, 0b11111110),

	#undef MAKE
};

struct USBSetupRequest // See: Source(2) @ Table(9-2) @ Page(248).
{
	u16 kind; // Aliasing enum USBSetupRequestKind. "bmRequestType" in LSB and "bRequest" in MSB.
	union
	{
		struct // See: Standard "GetDescriptor" @ Source(2) @ Section(9.4.3) @ Page(253).
		{
			u8  desc_index;       // Irrelevant; only for USBDescType_string and USBDescType_config.
			u8  desc_type;        // Aliasing enum USBDescType.
			u16 language_id;      // Irrelevant; only for USBDescType_string. See: [USB Strings].
			u16 requested_amount; // Maximum amount of data the host expects back from the device.
		} get_desc;

		struct // See: Standard "SetAddress" @ Source(2) @ Section(9.4.6) @ Page(256).
		{
			u16 address; // Must be within 7-bits.
		} set_address;

		struct // See: Standard "SetConfiguration" @ Source(2) @ Section(9.4.7) @ Page(257).
		{
			u16 id; // See: "bConfigurationValue" @ Source(2) @ Table(9-10) @ Page(265).
		} set_config;

		struct // See: Standard "Clear Feature" on Endpoints @ Source(2) @ Section(9.4.1) @ Page(252).
		{
			u16 feature_selector; // Must be zero.
			u16 endpoint;         // Endpoint index that is also potentially OR'd with USBEndpointAddressFlag_in.
		} endpoint_clear_feature;

		struct
		{
			u16 _zero;
			u16 designated_interface_index;
			u16 _two;
		} interface_get_status;

		struct // See: CDC-Specific "SetLineCoding" @ Source(6) @ Setion(6.2.12) @ AbsPage(68-69).
		{
			u16 _zero;
			u16 designated_interface_index;           // Index of the interface that the host wants to set the line-coding for. Should be to our only CDC interface (USBConfigInterface_cdc).
			u16 incoming_line_coding_datapacket_size; // Amount of bytes the host will be sending. Should be sizeof(struct USBCDCLineCoding).
		} cdc_set_line_coding;

		struct // See: HID-Specific "GetDescriptor" @ Source(7) @ Section(7.1.1) @ AbsPage(59).
		{
			u8  desc_index;                 // Irrelevant; only for HID-specific physical descriptors.
			u8  desc_type;                  // Aliasing enum USBDescType.
			u16 designated_interface_index; // Index of the interface that the request is for. Should be to our only HID interface (USBConfigInterface_hid).
			u16 requested_amount;           // Maximum amount of data the host expects back from the device.
		} hid_get_desc;
	};
};

enum USBDescType
{
	// Non-exhaustive. See: Standard Descriptor Types @ Source(10) @ Table(9-5) @ Page(315).
	USBDescType_device                = 1,
	USBDescType_config                = 2,
	USBDescType_string                = 3, // See: [USB Strings].
	USBDescType_interface             = 4,
	USBDescType_endpoint              = 5,
	USBDescType_device_qualifier      = 6,
	USBDescType_interface_association = 11,

	// See: CDC-Specific Descriptors @ Source(6) @ Table(24) @ AbsPage(44).
	USBDescType_cdc_interface = 0x24,
	USBDescType_cdc_endpoint  = 0x25,

	// See: HID-Specific Descriptors @ Source(7) @ Section(7.1) @ AbsPage(59).
	USBDescType_hid          = 0x21,
	USBDescType_hid_report   = 0x22,
	USBDescType_hid_physical = 0x23,
};

struct USBDescDevice // See: Source(2) @ Table(9-8) @ Page(262-263).
{
	u8  bLength;            // Must be sizeof(struct USBDescDevice).
	u8  bDescriptorType;    // Must be USBDescType_device.
	u16 bcdUSB;             // USB specification version that's being complied with.
	u8  bDeviceClass;       // Aliasing enum USBClass.
	u8  bDeviceSubClass;    // Optionally used to further subdivide the device class. See: Source(2) @ Section(9.2.3) @ Page(245).
	u8  bDeviceProtocol;    // Optionally used to indicate to the host on how to communicate with the device. See: Source(2) @ Section(9.2.3) @ Page(245).
	u8  bMaxPacketSize0;    // Most amount of bytes endpoint 0 can send, i.e. USB_ENDPOINT_DFLT_SIZE.
	u16 idVendor;           // Irrelevant; This with idProduct helps the host find drivers for the device. See: Source(4) @ Chapter(5).
	u16 idProduct;          // Irrelevant; UID arbitrated by the vendor for a specific device made by them.
	u16 bcdDevice;          // Irrelevant; version number of the device.
	u8  iManufacturer;      // Irrelevant. See: [USB Strings].
	u8  iProduct;           // Irrelevant. See: [USB Strings].
	u8  iSerialNumber;      // Irrelevant. See: [USB Strings].
	u8  bNumConfigurations; // Must be 1; only a single configuration (USB_CONFIG) is defined.
};

enum USBConfigAttrFlag // See: "bmAttributes" @ Source(2) @ Table(9-10) @ Page(266).
{
	USBConfigAttrFlag_remote_wakeup = 1 << 5,
	USBConfigAttrFlag_self_powered  = 1 << 6,
	USBConfigAttrFlag_reserved_one  = 1 << 7, // Must always be set.
};
struct USBDescConfig // See: Source(2) @ Table(9-10) @ Page(265).
{
	u8  bLength;             // Must be sizeof(struct USBDescConfig).
	u8  bDescriptorType;     // Must be USBDescType_config.
	u16 wTotalLength;        // Amount of bytes to describe the entire contiguous configuration hierarchy. See: Configuration Hierarchy Diagram @ Source(4) @ Chapter(5).
	u8  bNumInterfaces;      // Amount of interfaces that this configuration has.
	u8  bConfigurationValue; // Argument passed by SetConfiguration to select this configuration. See: Source(2) @ Table(9-10) @ Page(265).
	u8  iConfiguration;      // Irrelevant. See: [USB Strings].
	u8  bmAttributes;        // Aliasing enum USBConfigAttrFlag.
	u8  bMaxPower;           // Expressed in units of 2mA (e.g. bMaxPower = 50 -> 100mA usage).
};

struct USBDescInterface // See: Source(2) @ Table(9-12) @ Page(268-269).
{
	u8 bLength;            // Must be sizeof(struct USBDescInterface).
	u8 bDescriptorType;    // Must be USBDescType_interface.
	u8 bInterfaceNumber;   // Index of the interface within the configuration.
	u8 bAlternateSetting;  // Irrelevant; this is allow the host have more options to configure the device.
	u8 bNumEndpoints;      // Number of endpoints (not including endpoint 0) used by this interface.
	u8 bInterfaceClass;    // Aliasing enum USBClass.
	u8 bInterfaceSubClass; // Optionally used to further subdivide the interface class.
	u8 bInterfaceProtocol; // Optionally used to indicate to the host on how to communicate with the device.
	u8 iInterface;         // Irrelevant. See: [USB Strings].
};

enum USBEndpointAddressFlag // See: "bEndpointAddress" @ Source(2) @ Table(9-13) @ Page(269).
{
	USBEndpointAddressFlag_in = 1 << 7,
};
struct USBDescEndpoint // See: Source(2) @ Table(9-13) @ Page(269-271).
{
	u8  bLength;          // Must be sizeof(struct USBDescEndpoint).
	u8  bDescriptorType;  // Must be USBDescType_endpoint.
	u8  bEndpointAddress; // The "N" in "Endpoint N". Addresses must be specified in low-nibble. Can be applied with enum USBEndpointAddressFlag.
	u8  bmAttributes;     // Aliasing enum USBEndpointTransferType. Other bits have meanings too, but only for isochronous endpoints, which we don't use.
	u16 wMaxPacketSize;   // Bits 10-0 specifies the maximum data-packet size that can be sent/received to/from this endpoint. Bits 12-11 are reserved for high-speed.
	u8  bInterval;        // Frames (1ms for full-speed USB) between each polling of this endpoint. Valid values differ depending on this endpoint's transfer-type.
};

enum USBDescCDCSubtype // Non-exhaustive. See: Source(6) @ Table(25) @ AbsPage(44-45).
{
	USBDescCDCSubtype_header          = 0x00,
	USBDescCDCSubtype_call_management = 0x01,
	USBDescCDCSubtype_acm_management  = 0x02,
	USBDescCDCSubtype_union           = 0x06,
};

struct USBDescCDCHeader // Source(6) @ Section(5.2.3.1) @ AbsPage(45).
{
	u8  bLength;            // Must be sizeof(struct USBDescCDCHeader).
	u8  bDescriptorType;    // Must be USBDescType_cdc_interface.
	u8  bDescriptorSubtype; // Must be USBDescCDCSubtype_header.
	u16 bcdCDC;             // CDC specification version that's being complied with.
};

struct USBDescCDCCallManagement // See: Source(6) @ Table(27) @ AbsPage(45-46).
{
	u8 bLength;            // Must be sizeof(struct USBDescCDCCallManagement).
	u8 bDescriptorType;    // Must be USBDescType_cdc_interface.
	u8 bDescriptorSubtype; // Must be USBDescCDCSubtype_call_management.
	u8 bmCapabilities;     // Describes what the CDC device is capable of in terms of call-management. Seems irrelevant for functionality.
	u8 bDataInterface;     // Index of CDC-Data interface to receive call-management command. Seems irrelevant for functionality.
};

struct USBDescCDCACMManagement // See: Source(6) @ Table(28) @ AbsPage(46-47).
{
	u8 bLength;            // Must be sizeof(struct USBDescCDCACMManagement).
	u8 bDescriptorType;    // Must be USBDescType_cdc_interface.
	u8 bDescriptorSubtype; // Must be USBDescCDCSubtype_acm.
	u8 bmCapabilities;     // Describes what commands the host can use for the CDC interface. Seems irrelevant for functionality, although we do handle SetControlLineState.
};

struct USBDescCDCUnion // See: Source(6) @ Table(33) @ AbsPage(51).
{
	u8 bLength;            // Must be sizeof(struct USBDescCDCUnion).
	u8 bDescriptorType;    // Must be USBDescType_cdc_interface.
	u8 bDescriptorSubtype; // Must be USBDescCDCSubtype_union.
	u8 bMasterInterface;   // Index to the master-interface.
	u8 bSlaveInterface[1]; // Indices to slave-interfaces that belong to bMasterInterface. CDC allows varying amounts is allowed here, but 1 is all we need.
};

struct USBDescHID // See: Source(7) @ Section(6.2.1) @ AbsPage(32).
{
	u8  bLength;                  // Must be sizeof(struct USBDescHID).
	u8  bDescriptorType;          // Must be USBDescType_hid.
	u16 bcdHID;                   // HID specification version that's being complied with.
	u8  bCountryCode;             // Irrelevant for functionality.
	u8  bNumDescriptors;          // Must be countof(subordinate_descriptors).
	struct                        // Descriptors that'd belong to this HID descriptor. See: Source(7) @ Section(5.1) @ AbsPage(22).
	{
		u8  bDescriptorType;      // Alias: enum USBDescType.
		u16 wDescriptorLength;    // Must be the amount of bytes of corresponding descriptor.
	} subordinate_descriptors[1]; // The amount of descriptor-type and descriptor-length pairs could vary here, but 1 is sufficient for our application.
};

struct USBDescIAD // See: Source(10) @ Table(9-16) @ Page(336).
{
	u8 bLength;           // Must be sizeof(struct USBDescIAD).
	u8 bDescriptorType;   // Must be USBDescType_interface_association.
	u8 bFirstInterface;
	u8 bInterfaceCount;
	u8 bFunctionClass;
	u8 bFunctionSubClass;
	u8 bFunctionProtocol;
	u8 iFunction;         // Irrelevant. See: [USB Strings].
};

struct USBCDCLineCoding // See: Source(6) @ Table(50) @ AbsPage(69).
{
	u32 dwDTERate;   // Baud-rate.
	u8  bCharFormat; // Describes amount of stop-bits.
	u8  bParityType;
	u8  bDataBits;
};

enum USBHIDItem // Short-items with hardcoded amount of data bytes. See: Source(7) @ Section(6.2.2.2) @ AbsPage(36).
{
	// Non-exhaustive. See: "Main Items" @ Source(7) @ Section(6.2.2.4) @ AbsPage(38).
	USBHIDItem_main_input            = 0b1000'00'01,
	USBHIDItem_main_begin_collection = 0b1010'00'01,
	USBHIDItem_main_end_collection   = 0b1100'00'00,

	// Non-exhaustive. See: "Global Items" @ Source(7) @ Section(6.2.2.7) @ AbsPage(45).
	USBHIDItem_global_usage_page   = 0b0000'01'01,
	USBHIDItem_global_logical_min  = 0b0001'01'01,
	USBHIDItem_global_logical_max  = 0b0010'01'01,
	USBHIDItem_global_report_size  = 0b0111'01'01, // In terms of bits.
	USBHIDItem_global_report_count = 0b1001'01'01,

	// Non-exhaustive. See: "Local Items" @ Source(7) @ Section(6.2.2.8) @ AbsPage(50).
	USBHIDItem_local_usage     = 0b0000'10'01,
	USBHIDItem_local_usage_min = 0b0001'10'01,
	USBHIDItem_local_usage_max = 0b0010'10'01,
};

enum USBHIDItemMainInputFlag // Non-exhaustive. See: Source(7) @ Section(6.2.2.4) @ AbsPage(38) & Source(7) @ Section(6.2.2.5) @ AbsPage(40-41).
{
	USBHIDItemMainInputFlag_padding  = u16(1) << 0, // Also called "constant", but it's essentially used for padding.
	USBHIDItemMainInputFlag_variable = u16(1) << 1, // Data is just a simple bitmap/integer; otherwise, the data is a fixed-sized buffer that can be filled up.
	USBHIDItemMainInputFlag_relative = u16(1) << 2, // Data is a delta.
};

enum USBHIDItemMainCollectionType // Non-exhaustive. See: Source(7) @ Section(6.2.2.4) @ AbsPage(38) & Source(7) @ Section(6.2.2.6) @ AbsPage(42-43).
{
	USBHIDItemMainCollectionType_physical    = 0x00, // Groups report data together to represent a measurement at one "geometric point".
	USBHIDItemMainCollectionType_application = 0x01, // Packages up all the report layouts together to represent one single-purpose device, e.g. mouse.
};

enum USBHIDUsagePage // Non-exhaustive. See: Source(8) @ Section(3) @ AbsPage(17).
{
	USBHIDUsagePage_generic_desktop = 0x01, // Category of common computer peripherals such as mice, keyboard, joysticks, etc. See: enum USBHIDUsageGenericDesktop.
	USBHIDUsagePage_button          = 0x09, // Details how buttons are defined. See: Source(8) @ Section(12) @ AbsPage(110).
};

enum USBHIDUsageGenericDesktop // Non-exhaustive. See: Source(8) @ Section(4) @ AbsPage(32).
{
	USBHIDUsageGenericDesktop_pointer = 0x01, // "A collection of axes that generates a value to direct, indicate, or point user intentions to an application."
	USBHIDUsageGenericDesktop_mouse   = 0x02, // "A[n] ... input device that when rolled along a flat surface, directs an indicator to move correspondingly about a computer screen."
	USBHIDUsageGenericDesktop_x       = 0x30, // Linear translation from left-to-right.
	USBHIDUsageGenericDesktop_y       = 0x31, // Linear translation from far-to-near, so in the context of a mouse, top-down.
};

struct USBMSCommandBlockWrapper // See: Source(12) @ Section(5.1) @ Page(13-14).
{
	#define USB_MS_COMMAND_BLOCK_WRAPPER_SIGNATURE 0x43425355
	u32 dCBWSignature;          // Must be USB_MS_COMMAND_BLOCK_WRAPPER_SIGNATURE.
	u32 dCBWTag;                // Value to be copied for the command status wrapper.
	u32 dCBWDataTransferLength; // Amount of bytes to be transferred from host to device or device to host.
	u8  bmCBWFlags;             // Must not be used if dCBWDataTransferLength is zero. If the MSb is set, data transfer is from device to host. Any other bit must be cleared.
	u8  bCBWLUN;                // Irrelevant; "logical unit number" the command is designated for. Must be 0.
	u8  bCBWCBLength;           // Amount of bytes defined in CBWCB; must be within [1, 16] and high 3 bits be cleared.
	u8  CBWCB[16];              // Bytes of command.
};

enum USBMSCommandStatusWrapperStatus // See: Source(12) @ Table(5.3) @ Page(15).
{
	USBMSCommandStatusWrapperStatus_success     = 0x00,
	USBMSCommandStatusWrapperStatus_failed      = 0x01,
	USBMSCommandStatusWrapperStatus_phase_error = 0x02,
};

struct USBMSCommandStatusWrapper // See: Source(12) @ Section(5.2) @ Page(14-15).
{
	#define USB_MS_COMMAND_STATUS_WRAPPER_SIGNATURE 0x53425355
	u32 dCSWSignature;   // Must be USB_MS_COMMAND_STATUS_WRAPPER_SIGNATURE,
	u32 dCSWTag;         // Must be the value the host assigned in "dCBWTag" of the command block wrapper. See: struct USBMSCommandBlockWrapper.
	u32 dCSWDataResidue; // Amount of "left-over" data that wasn't sent to the host or was ignored by the device.
	u8  bCSWStatus;      // Aliasing enum USBMSCommandStatusWrapperStatus.
};

enum USBMSSCSIOpcode
{
	// Non-exhaustive. See: Source(13) @ Table(13) @ Page(41-42).
	USBMSSCSIOpcode_test_unit_ready = 0x00,
	USBMSSCSIOpcode_request_sense   = 0x03,
	USBMSSCSIOpcode_inquiry         = 0x12,

	USBMSSCSIOpcode_mode_sense      = 0x1A,

	// Non-exhaustive. See: Source(14) @ Table(10) @ Page(31-33).
	USBMSSCSIOpcode_read_capacity                = 0x25, // 10-byte version.
	USBMSSCSIOpcode_read                         = 0x28, // 10-byte version.
	USBMSSCSIOpcode_write                        = 0x2A, // 10-byte version.
};

enum USBConfigInterface // These interfaces are defined uniquely for our device application.
{
	#if USB_CDC_ENABLE
		USBConfigInterface_cdc,
		USBConfigInterface_cdc_data,
	#endif
	#if USB_HID_ENABLE
		USBConfigInterface_hid,
	#endif
	#if USB_MS_ENABLE
		USBConfigInterface_ms,
	#endif
	USBConfigInterface_COUNT,
};

struct USBConfig // This layout is defined uniquely for our device application.
{
	struct USBDescConfig desc;

	#if USB_CDC_ENABLE
		struct USBDescIAD iad_cdc; // Must be placed here to group the CDC and CDC-Data interfaces together. See: Source(9) @ Figure(2-1) @ AbsPage(6).

		struct
		{
			struct USBDescInterface         desc;
			struct USBDescCDCHeader         cdc_header;
			struct USBDescCDCCallManagement cdc_call_management;
			struct USBDescCDCACMManagement  cdc_acm_management;
			struct USBDescCDCUnion          cdc_union;
		} cdc;

		struct
		{
			struct USBDescInterface desc;
			struct USBDescEndpoint  endpoints[2];
		} cdc_data;
	#endif

	#if USB_HID_ENABLE
		struct
		{
			struct USBDescInterface desc;
			struct USBDescHID       hid;
			struct USBDescEndpoint  endpoints[1];
		} hid;
	#endif

	#if USB_MS_ENABLE
		struct
		{
			struct USBDescInterface desc;
			struct USBDescEndpoint  endpoints[2];
		} ms;
	#endif
};

// Endpoint buffer sizes must be one of the names of enum USBEndpointSizeCode.
// The maximum capacity between endpoints also differ. See: Source(1) @ Section(22.1) @ Page(270).

#define USB_ENDPOINT_DFLT_INDEX         0                               // "Default Endpoint" is synonymous with endpoint 0.
#define USB_ENDPOINT_DFLT_TRANSFER_TYPE USBEndpointTransferType_control // The default endpoint is always a control-typed endpoint.
#define USB_ENDPOINT_DFLT_TRANSFER_DIR  0                               // See: [ATmega32U4's Configuration of Endpoint 0].
#define USB_ENDPOINT_DFLT_SIZE          8
#define USB_ENDPOINT_DFLT_DOUBLE_BANK   false

#if USB_CDC_ENABLE
	#define USB_ENDPOINT_CDC_IN_INDEX          1
	#define USB_ENDPOINT_CDC_IN_TRANSFER_TYPE  USBEndpointTransferType_bulk
	#define USB_ENDPOINT_CDC_IN_TRANSFER_DIR   USBEndpointAddressFlag_in
	#define USB_ENDPOINT_CDC_IN_SIZE           64
	#define USB_ENDPOINT_CDC_IN_DOUBLE_BANK    true

	#define USB_ENDPOINT_CDC_OUT_INDEX         2
	#define USB_ENDPOINT_CDC_OUT_TRANSFER_TYPE USBEndpointTransferType_bulk
	#define USB_ENDPOINT_CDC_OUT_TRANSFER_DIR  0
	#define USB_ENDPOINT_CDC_OUT_SIZE          64
	#define USB_ENDPOINT_CDC_OUT_DOUBLE_BANK   true
#endif

#if USB_HID_ENABLE
	#define USB_ENDPOINT_HID_INDEX         3
	#define USB_ENDPOINT_HID_TRANSFER_TYPE USBEndpointTransferType_interrupt
	#define USB_ENDPOINT_HID_TRANSFER_DIR  USBEndpointAddressFlag_in
	#define USB_ENDPOINT_HID_SIZE          8
	#define USB_ENDPOINT_HID_DOUBLE_BANK   true
#endif

#if USB_MS_ENABLE
	#define USB_ENDPOINT_MS_IN_INDEX         4
	#define USB_ENDPOINT_MS_IN_TRANSFER_TYPE USBEndpointTransferType_bulk
	#define USB_ENDPOINT_MS_IN_TRANSFER_DIR  USBEndpointAddressFlag_in
	#define USB_ENDPOINT_MS_IN_SIZE          64
	#define USB_ENDPOINT_MS_IN_DOUBLE_BANK   true

	#define USB_ENDPOINT_MS_OUT_INDEX         5
	#define USB_ENDPOINT_MS_OUT_TRANSFER_TYPE USBEndpointTransferType_bulk
	#define USB_ENDPOINT_MS_OUT_TRANSFER_DIR  0
	#define USB_ENDPOINT_MS_OUT_SIZE          64
	#define USB_ENDPOINT_MS_OUT_DOUBLE_BANK   true

	// Maximum packet size is 64. See: "wMaxPacketSize" @ Source(12) @ Table(4.6) @ Page(11).
	static_assert(USB_ENDPOINT_MS_IN_SIZE  <= 64);
	static_assert(USB_ENDPOINT_MS_OUT_SIZE <= 64);
#endif

#if PROGRAM_DIPLOMAT
	static const u8 USB_ENDPOINT_UECFGNX[][2] PROGMEM = // UECFG0X and UECFG1X that an endpoint will be configured with.
		{
			#define MAKE(NAME) \
				[USB_ENDPOINT_##NAME##_INDEX] = \
					{ \
						(USB_ENDPOINT_##NAME##_TRANSFER_TYPE << EPTYPE0) | ((!!USB_ENDPOINT_##NAME##_TRANSFER_DIR) << EPDIR), \
						(concat(USBEndpointSizeCode_, USB_ENDPOINT_##NAME##_SIZE) << EPSIZE0) | (USB_ENDPOINT_##NAME##_DOUBLE_BANK << EPBK0) | (1 << ALLOC), \
					},
			MAKE(DFLT)
			#if USB_CDC_ENABLE
				MAKE(CDC_IN)
				MAKE(CDC_OUT)
			#endif
			#if USB_HID_ENABLE
				MAKE(HID)
			#endif
			#if USB_MS_ENABLE
				MAKE(MS_IN)
				MAKE(MS_OUT)
			#endif
			#undef MAKE
		};

	static const u8 USB_DESC_HID_REPORT[] PROGMEM =
		{
			USBHIDItem_global_usage_page,     USBHIDUsagePage_generic_desktop,
			USBHIDItem_local_usage,           USBHIDUsageGenericDesktop_mouse,
			USBHIDItem_main_begin_collection, USBHIDItemMainCollectionType_application,

				USBHIDItem_local_usage,           USBHIDUsageGenericDesktop_pointer,
				USBHIDItem_main_begin_collection, USBHIDItemMainCollectionType_physical,

					// LSb for primary mouse button being held.
					USBHIDItem_global_usage_page,   USBHIDUsagePage_button,
					USBHIDItem_local_usage_min,     1,
					USBHIDItem_local_usage_max,     1,
					USBHIDItem_global_logical_min,  0,
					USBHIDItem_global_logical_max,  1,
					USBHIDItem_global_report_size,  1,
					USBHIDItem_global_report_count, 1,
					USBHIDItem_main_input,          USBHIDItemMainInputFlag_variable,

					// Padding bits.
					USBHIDItem_global_report_size,  7,
					USBHIDItem_global_report_count, 1,
					USBHIDItem_main_input,          USBHIDItemMainInputFlag_padding,

					// Two signed bytes for delta-x and delta-y respectively.
					USBHIDItem_global_usage_page,   USBHIDUsagePage_generic_desktop,
					USBHIDItem_local_usage,         USBHIDUsageGenericDesktop_x,
					USBHIDItem_local_usage,         USBHIDUsageGenericDesktop_y,
					USBHIDItem_global_logical_min,  -128,
					USBHIDItem_global_logical_max,  127,
					USBHIDItem_global_report_size,  8,
					USBHIDItem_global_report_count, 2,
					USBHIDItem_main_input,          USBHIDItemMainInputFlag_variable | USBHIDItemMainInputFlag_relative,

				USBHIDItem_main_end_collection,

			USBHIDItem_main_end_collection,
		};

	static const u8 USB_MS_SCSI_READ_CAPACITY_DATA[] PROGMEM = // See: Source(14) @ Section(5.10.2) @ Page(54-55).
		{
			// "RETURNED LOGICAL BLOCK ADDRESS" : Big-endian address of the last addressable sector.
				(u32(FAT32_TOTAL_SECTOR_COUNT - 1) >> 24) & 0xFF,
				(u32(FAT32_TOTAL_SECTOR_COUNT - 1) >> 16) & 0xFF,
				(u32(FAT32_TOTAL_SECTOR_COUNT - 1) >>  8) & 0xFF,
				(u32(FAT32_TOTAL_SECTOR_COUNT - 1) >>  0) & 0xFF,

			// "BLOCK LENGTH IN BYTES" : Big-endian size of sectors.
				(u32(FAT32_SECTOR_SIZE) >> 24) & 0xFF,
				(u32(FAT32_SECTOR_SIZE) >> 16) & 0xFF,
				(u32(FAT32_SECTOR_SIZE) >>  8) & 0xFF,
				(u32(FAT32_SECTOR_SIZE) >>  0) & 0xFF,
		};

	static const u8 USB_MS_SCSI_INQUIRY_DATA[] PROGMEM = // See: Source(13) @ Section(7.3.2) @ Page(82-86).
		{
			// "PERIPHERAL QUALIFIER"         : We support the following specified peripheral device type. See: Source(13) @ Table(47) @ Page(83).
			//     | "PERIPHERAL DEVICE TYPE" : We are a "direct access block device", e.g. SD cards, flashdrives, etc. See: Source(13) @ Table(48) @ Page(83).
			//     |    |
			//    vvv vvvvv
				0b000'00000,

			// "RMB" : We are a removable storage device. See: Source(13) @ Section(7.3.2) @ Page(84).
			//    | Reserved.
			//    |    |
			//    v vvvvvvv
				0b1'0000000,

			// "VERSION" : We are using the cited standard "ISO/IEC 14776-312 : 200x / ANSI NCITS.351:200x". See: Source(13) @ Section(7.3.2) @ Page(84).
				0x04,

			// "AERC"                            : We don't have the "asynchronous event reporting capability". See: Source(13) @ Section(7.3.2) @ Page(84).
			//    | Obsolete.
			//    | | "NORMACA"                  : The "ACA" bit in the control byte of commands is not supported. See: Source(13) @ Section(7.3.2) @ Page(85).
			//    | | | "HISUP"                  : We do not have "hierarchical support" for accessing logical units. See: Source(13) @ Section(7.3.2) @ Page(85).
			//    | | | | "RESPONSE DATA FORMAT" : Two is the only value defined for this field. See: Source(13) @ Section(7.3.2) @ Page(85).
			//    | | | | |
			//    v v v v vvvv
				0b0'0'0'0'0010,

			// "ADDITIONAL LENGTH" : Amount of bytes remaining in the inquiry data after this byte. See: Source(13) @ Section(7.3.2) @ Page(85).
				31,

			// "SCCS" : We don't have "embedded storage array controller component support". See: Source(13) @ Section(7.3.2) @ Page(85).
			//    | Reserved.
			//    |   |
			//    v vvvvvvv
				0b0'0000000,

			// "BQUE"                  : With CMDQUE being zero, we do not support "tagged tasks (command queuing)". See: Source(13) @ Section(7.3.2) @ Page(85).
			//    | "ENCSERV"          : We do not have a "embedded enclosure services component". See: Source(13) @ Section(7.3.2) @ Page(85).
			//    | | Vendor-Specific.
			//    | | | "MULTIP"       : We do not implement "multi-port requirements". See: Source(13) @ Section(7.3.2) @ Page(85).
			//    | | | | "MCHNGER"    : We are not "embedded within or attached to a medium transport element". See: Source(13) @ Section(7.3.2) @ Page(85).
			//    | | | | | Obsolete.
			//    | | | | | | "ADDR16" : Irrelevant; only for the SCSI Parallel Interface.
			//    | | | | | |  |
			//    v v v v v vv v
				0b0'0'0'0'0'00'0,

			// "RELADR"                 : We don't support "relative addressing". See: Source(13) @ Section(7.3.2) @ Page(85).
			//    | Obsolete.
			//    | | "WBUS16"          : Irrelevant; only for the SCSI Parallel Interface.
			//    | | | "SYNC"          : Irrelevant; only for the SCSI Parallel Interface.
			//    | | | | "LINKED"      : We don't support "linked commands". See: Source(13) @ Section(7.3.2) @ Page(85).
			//    | | | | | Obsolete.
			//    | | | | | | "CMDQUE"  : With BQUE being zero, we do not support any kind of command queuing. See: Source(13) @ Table(50) @ Page(86).
			//    | | | | | | | Vendor Specific.
			//    | | | | | | | |
			//    v v v v v v v v
				0b0'0'0'0'0'0'0'0,

			// "VENDOR IDENTIFICATION" : Irrelevant; 8 character text representing the vendor name. See: Source(13) @ Section(7.3.2) @ Page(86).
				' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',

			// "PRODUCT IDENTIFICATION" : Irrelevant; 16 character text representing the product name. See: Source(13) @ Section(7.3.2) @ Page(86).
				' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',

			// "PRODUCT REVISION LEVEL" : Irrelevant; 4 character text representing the product version. See: Source(13) @ Section(7.3.2) @ Page(86).
				' ', ' ', ' ', ' ',
		};
	static const u8 USB_MS_SCSI_UNSUPPORTED_COMMAND_SENSE[] PROGMEM = // See: Source(13) @ Section(7.20.2) @ Page(136-138).
		{
			// "VALID"              : Must be 1 for this sense data to comply with the standard. See: Source(13) @ Section(7.20.2) @ Page(136).
			//    | "RESPONSE CODE" : 0x70 says taht the sense data is about the "current errors" that the device just had. See: Source(13) @ Section(7.20.4) @ Page(140).
			//    |    |
			//    v vvvvvvv
				0b1'1110000,

			// Obsolete.
				0,

			// "FILEMARK"             : Irrelevant; only for sequential-access devices such as magnetic tapes. See: Source(13) @ Section(7.20.2) @ Page(136).
			//    | "EOM"             : Irrelevant; only for sequential-access and printer devices. See: Source(13) @ Section(7.20.2) @ Page(136).
			//    | | "ILI"           : Irrelevant; meaning of this bit is broadly whenever the data is "incorrect length". See: Source(13) @ Section(7.20.2) @ Page(137).
			//    | | | Reserved.
			//    | | | | "SENSE KEY" : 0x05 for illegal requests where we don't support the command or we don't understand a parameter. See: Source(13) @ Table(107) @ Page(141-142).
			//    | | | |  |
			//    v v v v vvvv
				0b0'0'0'0'0101,

			// "INFORMATION" : For our direct-access device, this would be the big-endian "unsigned logical block address associated with the sense key"; seems irrelevant. See: Source(13) @ Section(7.20.2) @ Page(137).
				0, 0, 0, 0,

			// "ADDITIONAL SENSE LENGTH" : Amount of remaining data after this byte. See: Source(13) @ Section(7.20.2) @ Page(137).
				0,
		};

	static const struct USBDescDevice USB_DESC_DEVICE PROGMEM =
		{
			.bLength            = sizeof(struct USBDescDevice),
			.bDescriptorType    = USBDescType_device,
			.bcdUSB             = 0x0200,        // We are still pretty much USB 2.0 despite using IADs.
			.bDeviceClass       = USBClass_misc, // See: "bDeviceClass" @ Source(9) @ Table(1-1) @ AbsPage(5).
			.bDeviceSubClass    = 0x02,          // See: "bDeviceSubClass" @ Source(9) @ Table(1-1) @ AbsPage(5).
			.bDeviceProtocol    = 0x01,          // See: "bDeviceProtocol" @ Source(9) @ Table(1-1) @ AbsPage(5).
			.bMaxPacketSize0    = USB_ENDPOINT_DFLT_SIZE,
			.bNumConfigurations = 1
		};

	#define USB_CONFIG_ID 1 // Must be non-zero. See: Soure(2) @ Figure(11-10) @ Page(310).
	static const struct USBConfig USB_CONFIG PROGMEM =
		{
			.desc =
				{
					.bLength             = sizeof(struct USBDescConfig),
					.bDescriptorType     = USBDescType_config,
					.wTotalLength        = sizeof(struct USBConfig),
					.bNumInterfaces      = USBConfigInterface_COUNT,
					.bConfigurationValue = USB_CONFIG_ID,
					.bmAttributes        = USBConfigAttrFlag_reserved_one | USBConfigAttrFlag_self_powered, // TODO We should calculate our power consumption!
					.bMaxPower           = 50,                                                              // TODO We should calculate our power consumption!
				},
		#if USB_CDC_ENABLE
			.iad_cdc =
				{
					.bLength           = sizeof(struct USBDescIAD),
					.bDescriptorType   = USBDescType_interface_association,
					.bFirstInterface   = USBConfigInterface_cdc,
					.bInterfaceCount   = 2,
					.bFunctionClass    = USBClass_cdc,
					.bFunctionSubClass = 0x2, // This field will act like "bDeviceSubClass". See: "Abstract Control Model" @ Source(6) @ Table(16) @ AbsPage(39).
					.bFunctionProtocol = 0,   // This field will act like "bDeviceProtocol". Seems irrelevant for functionality. See: Source(6) @ Table(17) @ AbsPage(40).
				},
			.cdc =
				{
					.desc =
						{
							.bLength            = sizeof(struct USBDescInterface),
							.bDescriptorType    = USBDescType_interface,
							.bInterfaceNumber   = USBConfigInterface_cdc,
							.bNumEndpoints      = 0,
							.bInterfaceClass    = USBClass_cdc, // See: Source(6) @ Section(4.2) @ AbsPage(39).
							.bInterfaceSubClass = 0x2,          // See: "Abstract Control Model" @ Source(6) @ Table(16) @ AbsPage(39).
							.bInterfaceProtocol = 0,            // Seems irrelevant for functionality. See: Source(6) @ Table(17) @ AbsPage(40).
						},
					.cdc_header =
						{
							.bLength            = sizeof(struct USBDescCDCHeader),
							.bDescriptorType    = USBDescType_cdc_interface,
							.bDescriptorSubtype = USBDescCDCSubtype_header,
							.bcdCDC             = 0x0110,
						},
					.cdc_call_management =
						{
							.bLength            = sizeof(struct USBDescCDCCallManagement),
							.bDescriptorType    = USBDescType_cdc_interface,
							.bDescriptorSubtype = USBDescCDCSubtype_call_management,
						},
					.cdc_acm_management =
						{
							.bLength            = sizeof(struct USBDescCDCACMManagement),
							.bDescriptorType    = USBDescType_cdc_interface,
							.bDescriptorSubtype = USBDescCDCSubtype_acm_management,
						},
					.cdc_union =
						{
							.bLength            = sizeof(struct USBDescCDCUnion),
							.bDescriptorType    = USBDescType_cdc_interface,
							.bDescriptorSubtype = USBDescCDCSubtype_union,
							.bMasterInterface   = USBConfigInterface_cdc,
							.bSlaveInterface    = { USBConfigInterface_cdc_data }
						},
				},
			.cdc_data =
				{
					.desc =
						{
							.bLength            = sizeof(struct USBDescInterface),
							.bDescriptorType    = USBDescType_interface,
							.bInterfaceNumber   = USBConfigInterface_cdc_data,
							.bNumEndpoints      = countof(USB_CONFIG.cdc_data.endpoints),
							.bInterfaceClass    = USBClass_cdc_data, // See: Source(6) @ Section(4.5) @ AbsPage(40).
							.bInterfaceSubClass = 0,                 // Should be left alone. See: Source(6) @ Section(4.6) @ AbsPage(40).
							.bInterfaceProtocol = 0,                 // Seems irrelevant for functionality. See: Source(6) @ Table(19) @ AbsPage(40-41).
						},
					.endpoints =
						{
							{
								.bLength          = sizeof(struct USBDescEndpoint),
								.bDescriptorType  = USBDescType_endpoint,
								.bEndpointAddress = USB_ENDPOINT_CDC_IN_INDEX | USB_ENDPOINT_CDC_IN_TRANSFER_DIR,
								.bmAttributes     = USB_ENDPOINT_CDC_IN_TRANSFER_TYPE,
								.wMaxPacketSize   = USB_ENDPOINT_CDC_IN_SIZE,
								.bInterval        = 0,
							},
							{
								.bLength          = sizeof(struct USBDescEndpoint),
								.bDescriptorType  = USBDescType_endpoint,
								.bEndpointAddress = USB_ENDPOINT_CDC_OUT_INDEX | USB_ENDPOINT_CDC_OUT_TRANSFER_DIR,
								.bmAttributes     = USB_ENDPOINT_CDC_OUT_TRANSFER_TYPE,
								.wMaxPacketSize   = USB_ENDPOINT_CDC_OUT_SIZE,
								.bInterval        = 0,
							},
						}
				},
		#endif
		#if USB_HID_ENABLE
			.hid =
				{
					.desc =
						{
							.bLength            = sizeof(struct USBDescInterface),
							.bDescriptorType    = USBDescType_interface,
							.bInterfaceNumber   = USBConfigInterface_hid,
							.bNumEndpoints      = countof(USB_CONFIG.hid.endpoints),
							.bInterfaceClass    = USBClass_hid,
							.bInterfaceSubClass = 0, // Set to 1 if we support a boot interface, which we don't need to. See: Source(7) @ Section(4.2) @ AbsPage(18).
							.bInterfaceProtocol = 0, // Since we don't support a boot interface, this is also 0. See: Source(7) @ Section(4.3) @ AbsPage(19).
						},
					.hid =
						{
							.bLength                 = sizeof(struct USBDescHID),
							.bDescriptorType         = USBDescType_hid,
							.bcdHID                  = 0x0111,
							.bNumDescriptors         = countof(USB_CONFIG.hid.hid.subordinate_descriptors),
							.subordinate_descriptors =
								{
									{
										.bDescriptorType   = USBDescType_hid_report,
										.wDescriptorLength = sizeof(USB_DESC_HID_REPORT),
									},
								}
						},
					.endpoints =
						{
							{
								.bLength          = sizeof(struct USBDescEndpoint),
								.bDescriptorType  = USBDescType_endpoint,
								.bEndpointAddress = USB_ENDPOINT_HID_INDEX | USB_ENDPOINT_HID_TRANSFER_DIR,
								.bmAttributes     = USB_ENDPOINT_HID_TRANSFER_TYPE,
								.wMaxPacketSize   = USB_ENDPOINT_HID_SIZE,
								.bInterval        = 1,
							}
						},
				},
		#endif
		#if USB_MS_ENABLE
			.ms =
				{
					.desc =
						{
							.bLength            = sizeof(struct USBDescInterface),
							.bDescriptorType    = USBDescType_interface,
							.bInterfaceNumber   = USBConfigInterface_ms,
							.bNumEndpoints      = countof(USB_CONFIG.ms.endpoints),
							.bInterfaceClass    = USBClass_ms,
							.bInterfaceSubClass = 0x06, // See: "SCSI Transparent Command Set" @ Source(11) @ AbsPage(3).
							.bInterfaceProtocol = 0x50, // See: "Bulk-Only Transport" @ Source(12) @ Page(11).
						},
					.endpoints =
						{
							{
								.bLength          = sizeof(struct USBDescEndpoint),
								.bDescriptorType  = USBDescType_endpoint,
								.bEndpointAddress = USB_ENDPOINT_MS_IN_INDEX | USB_ENDPOINT_MS_IN_TRANSFER_DIR,
								.bmAttributes     = USB_ENDPOINT_MS_IN_TRANSFER_TYPE,
								.wMaxPacketSize   = USB_ENDPOINT_MS_IN_SIZE,
								.bInterval        = 0,
							},
							{
								.bLength          = sizeof(struct USBDescEndpoint),
								.bDescriptorType  = USBDescType_endpoint,
								.bEndpointAddress = USB_ENDPOINT_MS_OUT_INDEX | USB_ENDPOINT_MS_OUT_TRANSFER_DIR,
								.bmAttributes     = USB_ENDPOINT_MS_OUT_TRANSFER_TYPE,
								.wMaxPacketSize   = USB_ENDPOINT_MS_OUT_SIZE,
								.bInterval        = 0,
							}
						},
				},
		#endif
		};
#endif

#define USB_MOUSE_CALIBRATIONS_REQUIRED 128

#if PROGRAM_DIPLOMAT
	// Only the interrupt can read and write these.
	static u8 _usb_mouse_calibrations = 0;
	static u8 _usb_mouse_curr_x       = 0; // Origin is top-left.
	static u8 _usb_mouse_curr_y       = 0; // Origin is top-left.
	static b8 _usb_mouse_held         = false;

	static volatile u16 _usb_mouse_command_buffer[8] = {0}; // See: [Mouse Commands] @ "Diplomat_usb.c".
	static volatile u8  _usb_mouse_command_writer    = 0;   // Main program writes.
	static volatile u8  _usb_mouse_command_reader    = 0;   // Interrupt reads.

	#define _usb_mouse_command_writer_masked(OFFSET) ((_usb_mouse_command_writer + (OFFSET)) & (countof(_usb_mouse_command_buffer) - 1))
	#define _usb_mouse_command_reader_masked(OFFSET) ((_usb_mouse_command_reader + (OFFSET)) & (countof(_usb_mouse_command_buffer) - 1))

	// A read/write index with a size greater than a byte makes "atomic" read/write operations difficult to guarantee; it can be done, but probably not worthwhile.
	static_assert(sizeof(_usb_mouse_command_writer) == 1 && sizeof(_usb_mouse_command_reader) == 1);

	// The read/write indices must be able to address any element in the corresponding buffer.
	static_assert(countof(_usb_mouse_command_buffer) < (u64(1) << bitsof(_usb_mouse_command_reader)));
	static_assert(countof(_usb_mouse_command_buffer) < (u64(1) << bitsof(_usb_mouse_command_writer)));

	// Buffer sizes must be a power of two for the "_usb_mouse_X_masked" macros.
	static_assert(countof(_usb_mouse_command_buffer) && !(countof(_usb_mouse_command_buffer) & (countof(_usb_mouse_command_buffer) - 1)));

	#if USB_MS_ENABLE
		static struct USBMSCommandStatusWrapper _usb_ms_status      = { .dCSWSignature = USB_MS_COMMAND_STATUS_WRAPPER_SIGNATURE };
		static b8                               _usb_ms_send_status = false;
	#endif

	#if DEBUG
		#if USB_CDC_ENABLE
			static volatile u8 debug_usb_cdc_in_buffer [USB_ENDPOINT_CDC_IN_SIZE ] = {0};
			static volatile u8 debug_usb_cdc_out_buffer[USB_ENDPOINT_CDC_OUT_SIZE] = {0};

			static volatile u8 debug_usb_cdc_in_writer  = 0; // Main program writes.
			static volatile u8 debug_usb_cdc_in_reader  = 0; // Interrupt routine reads.
			static volatile u8 debug_usb_cdc_out_writer = 0; // Interrupt routine writes.
			static volatile u8 debug_usb_cdc_out_reader = 0; // Main program reads.

			#define debug_usb_cdc_in_writer_masked(OFFSET)  ((debug_usb_cdc_in_writer  + (OFFSET)) & (countof(debug_usb_cdc_in_buffer ) - 1))
			#define debug_usb_cdc_in_reader_masked(OFFSET)  ((debug_usb_cdc_in_reader  + (OFFSET)) & (countof(debug_usb_cdc_in_buffer ) - 1))
			#define debug_usb_cdc_out_writer_masked(OFFSET) ((debug_usb_cdc_out_writer + (OFFSET)) & (countof(debug_usb_cdc_out_buffer) - 1))
			#define debug_usb_cdc_out_reader_masked(OFFSET) ((debug_usb_cdc_out_reader + (OFFSET)) & (countof(debug_usb_cdc_out_buffer) - 1))

			// A read/write index with a size greater than a byte makes "atomic" read/write operations difficult to guarantee; it can be done, but probably not worthwhile.
			static_assert(sizeof(debug_usb_cdc_in_writer) == 1 && sizeof(debug_usb_cdc_in_reader) == 1 && sizeof(debug_usb_cdc_out_writer) == 1 && sizeof(debug_usb_cdc_out_reader) == 1);

			// The read/write indices must be able to address any element in the corresponding buffer.
			static_assert(countof(debug_usb_cdc_in_buffer ) <= (u64(1) << bitsof(debug_usb_cdc_in_reader )));
			static_assert(countof(debug_usb_cdc_in_buffer ) <= (u64(1) << bitsof(debug_usb_cdc_in_writer )));
			static_assert(countof(debug_usb_cdc_out_buffer) <= (u64(1) << bitsof(debug_usb_cdc_out_reader)));
			static_assert(countof(debug_usb_cdc_out_buffer) <= (u64(1) << bitsof(debug_usb_cdc_out_writer)));

			// Buffer sizes must be a power of two for the "debug_usb_cdc_X_Y_masked" macros.
			static_assert(countof(debug_usb_cdc_in_buffer ) && !(countof(debug_usb_cdc_in_buffer ) & (countof(debug_usb_cdc_in_buffer ) - 1)));
			static_assert(countof(debug_usb_cdc_out_buffer) && !(countof(debug_usb_cdc_out_buffer) & (countof(debug_usb_cdc_out_buffer) - 1)));

			static volatile b8 debug_usb_diagnostic_signal_received = false;
		#endif
	#endif
#endif

//
// "MicroServient.c".
//

#define CLI_ARG_ADDITIONAL_MARGIN 2
#define CLI_EXE_NAME              str("MicroServient.exe")
#define CLI_EXE_DESC              str("Parse screenshots of Game Pigeon word games.")
#define CLI_XMDT(X) \
	X(screenshot_dir_path, string, "screenshot-dir-path", "Directory path that'll be filtered for screenshots of the games.") \
	X(exam_dir_path      , string, "exam-dir-path"      , "Directory path of compressed monochrome BMPs to compare extracted slots against.") \
	X(output_dir_path    , string, "output-dir-path"    , "Destination directory to store the processing results.") \
	X(clear_output_dir   , b32   , "--clear-output-dir" , "Delete all content within the output directory before processing.")
#define CLI_TYPING_XMDT(X) \
	X(string     , union { struct { char* data; i64 length; }; char* cstr; str str; }) \
	X(dary_string, struct Dary_CLIFieldTyping_string_t) \
	X(b32        , b32)

#if PROGRAM_MICROSERVIENT
	enum CLIFieldTyping
	{
		#define MAKE(NAME, TYPE) CLIFieldTyping_##NAME,
		CLI_TYPING_XMDT(MAKE)
		#undef MAKE
	};

	#define MAKE(NAME, TYPE) typedef TYPE CLIFieldTyping_##NAME##_t;
	CLI_TYPING_XMDT(MAKE)
	#undef MAKE
	Dary_def(CLIFieldTyping_string_t);

	enum CLIField
	{
		#define MAKE(FIELD_NAME, TYPING_NAME, ...) CLIField_##FIELD_NAME,
		CLI_XMDT(MAKE)
		#undef MAKE
		CLIField_COUNT
	};

	struct CLI
	{
		#define MAKE(FIELD_NAME, TYPING_NAME, ...) CLIFieldTyping_##TYPING_NAME##_t FIELD_NAME;
		CLI_XMDT(MAKE)
		#undef MAKE
	};

	struct CLIFieldMetaData
	{
		i64                 offset;
		enum CLIFieldTyping typing;
		str                 pattern;
		str                 desc;
	};

	static const struct CLIFieldMetaData CLI_FIELD_INFO[] =
		{
			#define MAKE(FIELD_NAME, TYPING_NAME, PATTERN, DESC, ...) \
				{ \
					.offset  = offsetof(struct CLI, FIELD_NAME), \
					.typing  = CLIFieldTyping_##TYPING_NAME, \
					.pattern = STR(PATTERN), \
					.desc    = STR(DESC), \
				},
			CLI_XMDT(MAKE)
			#undef MAKE
		};
#endif







#define PHONE_DIM_PX_X 1170
#define PHONE_DIM_PX_Y 2532

#define AVG_RGB_MATCHING_EPSILON 0.01
#define WORDGAME_XMDT(X) \
	X(anagrams , "Anagrams" , 0.49221875000, 0.47258984375, 0.56611328125) \
	X(wordhunt , "WordHunt" , 0.48301171875, 0.53787890625, 0.41358203125) \
	X(wordbites, "WordBites", 0.36726562500, 0.45695312500, 0.53207421875)

enum WordGame
{
	#define MAKE(ENUM_NAME, ...) WordGame_##ENUM_NAME,
	WORDGAME_XMDT(MAKE)
	#undef MAKE
	WordGame_COUNT
};

#if PROGRAM_MICROSERVIENT
	static const struct { str print_name; f64 avg_r; f64 avg_g; f64 avg_b; } WORDGAME_DT[] =
		{
			#define MAKE(ENUM_NAME, PRINT_NAME, AVG_R, AVG_G, AVG_B) \
				{ \
					.print_name = STR(PRINT_NAME), \
					.avg_r = (AVG_R), \
					.avg_g = (AVG_G), \
					.avg_b = (AVG_B) \
				},
			WORDGAME_XMDT(MAKE)
			#undef MAKE
		};
#endif

#define ANAGRAMS_6_SLOT_DIM      195
#define ANAGRAMS_6_BOARD_POS_X   0
#define ANAGRAMS_6_BOARD_POS_Y   311
#define ANAGRAMS_6_BOARD_SLOTS_X 6
#define ANAGRAMS_6_BOARD_SLOTS_Y 1
static_assert(ANAGRAMS_6_BOARD_POS_X + ANAGRAMS_6_BOARD_SLOTS_X * ANAGRAMS_6_SLOT_DIM <= PHONE_DIM_PX_X); // Should not obviously exceed phone screen boundries.
static_assert(ANAGRAMS_6_BOARD_POS_Y + ANAGRAMS_6_BOARD_SLOTS_Y * ANAGRAMS_6_SLOT_DIM <= PHONE_DIM_PX_Y); // Should not obviously exceed phone screen boundries.

#define WORDHUNT_4x4_SLOT_DIM      212
#define WORDHUNT_4x4_BOARD_POS_X   161
#define WORDHUNT_4x4_BOARD_POS_Y   494
#define WORDHUNT_4x4_BOARD_SLOTS_X 4
#define WORDHUNT_4x4_BOARD_SLOTS_Y 4
static_assert(WORDHUNT_4x4_BOARD_POS_X + WORDHUNT_4x4_BOARD_SLOTS_X * WORDHUNT_4x4_SLOT_DIM <= PHONE_DIM_PX_X); // Should not obviously exceed phone screen boundries.
static_assert(WORDHUNT_4x4_BOARD_POS_Y + WORDHUNT_4x4_BOARD_SLOTS_Y * WORDHUNT_4x4_SLOT_DIM <= PHONE_DIM_PX_Y); // Should not obviously exceed phone screen boundries.

#define WORDBITES_SLOT_DIM      140
#define WORDBITES_BOARD_POS_X   25
#define WORDBITES_BOARD_POS_Y   354
#define WORDBITES_BOARD_SLOTS_X 8
#define WORDBITES_BOARD_SLOTS_Y 9
static_assert(WORDBITES_BOARD_POS_X + WORDBITES_BOARD_SLOTS_X * WORDBITES_SLOT_DIM <= PHONE_DIM_PX_X); // Should not obviously exceed phone screen boundries.
static_assert(WORDBITES_BOARD_POS_Y + WORDBITES_BOARD_SLOTS_Y * WORDBITES_SLOT_DIM <= PHONE_DIM_PX_Y); // Should not obviously exceed phone screen boundries.





#define EXAM_DIM              32
#define MONOCHROMIC_THRESHOLD 8

#define LETTER_XMDT(_X) \
	_X(space) \
	_X(A) _X(B) _X(C) _X(D) _X(E) _X(F) _X(G) _X(H) _X(I) _X(J) _X(K) _X(L) _X(M) \
	_X(N) _X(O) _X(P) _X(Q) _X(R) _X(S) _X(T) _X(U) _X(V) _X(W) _X(X) _X(Y) _X(Z) \

enum Letter
{
	#define MAKE(NAME) Letter_##NAME,
	LETTER_XMDT(MAKE)
	#undef MAKE
	Letter_COUNT
};

#if PROGRAM_MICROSERVIENT
	static const str LETTER_NAMES[] =
		{
			#define MAKE(NAME) STR(#NAME),
			LETTER_XMDT(MAKE)
			#undef MAKE
		};
#endif

//
// Documentation.
//

/* [Overview].
	Source(1)  := ATmega32U4 Datasheet ("Atmel-7766J-USB-ATmega16U4/32U4-Datasheet_04/2016").
	Source(2)  := USB 2.0 Specification (Dated: April 27, 2000).
	Source(3)  := Arduino Leonardo Pinout Diagram (STORE.ARDUINO.CC/LEONARDO) (Dated: 17/06/2020).
	Source(4)  := USB in a NutShell by BeyondLogic (Accessed: September 19, 2023).
	Source(5)  := USB-IF Defined Class Codes (usb.org/defined-class-codes) (Accessed: September 19, 2023).
	*Source(6) := USB CDC Specification v1.1 (Dated: January 19, 1999).
	Source(7)  := Device Class Definition for Human Interface Devices (HID) Version 1.11 (Dated: 5/27/01).
	Source(8)  := HID Usage Tables v1.4 (Dated: 1996-2022).
	Source(9)  := USB Interface Association Descriptor Device Class Code and Use Model (Dated: July 23, 2003).
	Source(10) := USB 3.0 Specification (Dated: November 12, 2008).
	Source(11) := Universal Serial Bus Mass Storage Class Specification Overview (Dated: February 19, 2010).
	Source(12) := Universal Serial Bus Mass Storage Class Bulk-Only Transport (Dated: September 31, 1999).
	+Source(13) := dpANS Project T10/1236-D (SCSI Primary Commands - 2) (SPC-2) (Dated: 18 July 2001).
	+Source(14) := Working Draft Project American National T10/1417-D Standard (SCSI Block Commands - 2) (SBC-2) (Dated: 13 November 2004).
	Source(15) := Microsoft FAT Specification (Dated: August 30 2005).
	Source(16) := "Master boot record" on Wikipedia (Accessed: October 7, 2023).
	Source(17) := FAT Filesystem by Elm-Chan (Updated on: October 31, 2020).
	Source(18) := Arduino Mega 2560 Rev3 Pinout Diagram (STORE.ARDUINO.CC/MEGA-2560-REV3) (Updated on: 16/12/2020).
	Source(19) := SD Specifications Part 1 Physical Layer Simplified Specification Version 2.00 (Dated: September 25, 2006).
	Source(20) := "How to Use MMC/SDC" by Elm-Chan (Updated on: December 26, 2019).
	Source(21) := "8-bit Atmel Microcontroller with 64/128Kbytes of ISP Flash and USB Controller" datasheet (Dated: 9/12).
	Source(22) := Microsofts's "Windows GDI" PDF Article (Dated: 01/24/2023).
	Source(23) :=  PDF Article of Microsoft's "Open Specifications" for Protocols (Dated: 08/01/2023).

	We are working within the environment of the ATmega32U4 and ATmega2560 microcontrollers,
	which are 8-bit CPUs. This consequently means that there are no padding bytes to
	worry about within structs, and that the CPU doesn't exactly have the concept of
	"endianess", since all words are single bytes, so it's really up to the compiler
	on how it will layout memory and perform arithmetic calculations on operands greater
	than a byte. That said, the MCUs' 16-bit opcodes and AVR-GCC's ABI are defined to use
	little-endian. In short: no padding bytes exist and we can take advantage of LSB being first
	before the MSB.

	* I seem to not be able to find a comprehensive document of v1.2. USB.org has a zip
	file containing an errata version of v1.2., but even that seem to be missing quite
	a lot of information compared to v1.1. Perhaps we're supposed to use the errata for
	the more up-to-date details, but fallback to v1.1 when needed. Regardless, USB
	should be quite backwards-compatiable, so we should be fine with v1.1.

	+ It was absolutely hell for me to be able to finally find these documents. The
	"SCSI Commands Reference Manual" by SEAGATE is the densest document I had ever
	gone through, and it managed to make my reading experience through USB's specification
	feel a light romance reading in comparison. The lack of clarity just cannot be
	underestimated. There weren't a whole lot of other alternative sources either.
	Especially by T10, one of the working group on the SCSI command set I believe, since
	they thought it would be an amazing idea to lock it behind to only members, even
	documents from decades ago. I guess if you want access to these, you had to pay the
	hefty ol' price of sixty bucks. Absolutely absurd what I had to go through.
*/

/* [USB Strings].
	USB devices can return strings of usually human-readable text back to the host for
	things such as the name of the manufacturer. (1), however, states that devices are
	not required at all to implement this, and thus we will not bother with string
	descriptors.

	(1) "String" @ Source(2) @ Section(9.6.7) @ Page(273).
*/

/* [About: USBClass_null].
	When USBClass_null is used in device descriptors (bDeviceClass), the class is supposedly
	determined by the interfaces of the configurations (bInterfaceClass). (1) says that
	USBClass_null cannot be used in interface descriptors as it is "reserved for future
	use". However, (2) states that it is for the "null class code triple" now,
	whatever that means! Anyways, USBClass_null in the device descriptor level doesn't really
	help with creating a device with multi-function capabilities, which I am supposing is due to
	the fact that USB drivers can't seem to be able to parse the configuration descriptor properly.
	And thus, IAD was introduced and handles it all for us.

	(1) "bInterfaceClass" @ Source(2) @ Table(9-12) @ Page(268).
	(2) "Base Class 00h (Device)" @ Source(5).
*/

/* [ATmega32U4's Configuration of Endpoint 0].
	For UECFG0X, endpoint 0 will always be a control-typed endpoint as enforced
	by the USB specification, so that part of the configuration is straight-forward.
	For the data-direction of endpoint 0, it seems like the ATmega32U4 datasheet wants
	us to define endpoint 0 to have an "OUT" direction (1), despite the fact that
	control-typed endpoints are bidirectional.

	(1) EPDIR @ Source(1) @ Section(22.18.2) @ Page(287).
*/
