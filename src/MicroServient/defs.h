#define false            0
#define true             1
#define stringify_(X)    #X
#define stringify(X)     stringify_(X)
#define concat_(X, Y)    X##Y
#define concat(X, Y)     concat_(X, Y)
#define countof(...)     (sizeof(__VA_ARGS__)/sizeof((__VA_ARGS__)[0]))
#define bitsof(...)      (sizeof(__VA_ARGS__) * 8)
#define static_assert(X) _Static_assert((X), #X);

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

typedef struct { char* data; i64 length; } str;
#define str(STRLIT) (str) { (STRLIT), sizeof(STRLIT) - 1 }
#define STR(STRLIT)       { (STRLIT), sizeof(STRLIT) - 1 }

static_assert(LITTLE_ENDIAN);

//
// "bmp.c"
//

struct BMPPixel
{
	u8 b;
	u8 g;
	u8 r;
	u8 a;
};

struct BMP
{
	struct BMPPixel* data;
	i32              dim_x;
	i32              dim_y;
};

struct BMPCIEXYZTRIPLE // Defined by Windows as "CIEXYZTRIPLE".
{
	struct
	{
		u32 ciexyzX;
		u32 ciexyzY;
		u32 ciexyzZ;
	} ciexyzRed, ciexyzGreen, ciexyzBlue;
};

#pragma pack(push, 1)
struct BMPFileHeader // See: "BITMAPFILEHEADER" @ Source(1).
{
	u16 bfType;      // Must be 'B' followed by 'M', i.e. 0x4D42.
	u32 bfSize;      // Size of the entire BMP file in bytes.
	u16 bfReserved1;
	u16 bfReserved2;
	u32 bfOffBits;   // Byte offset into the BMP file where the image's pixel data is stored.
};
#pragma pack(pop)

#pragma pack(push, 1)
union BMPDIBHeader // See: "DIB Header" @ Source(1).
{
	u32 size; // Helps determine which of the following headers below are used.

	struct BMPDIBHeaderCore // See: "BITMAPCOREHEADER" @ Source(1).
	{
		u32 bcSize;    // Must be sizeof(struct BMPDIBHeaderCore).
		u16 bcWidth;
		u16 bcHeight;
		u16 bcPlanes;
		u16 bcBitCount;
	} core;

	struct BMPDIBHeaderInfo // See: "BITMAPINFOHEADER" @ Source(1).
	{
		u32 biSize;
		i32 biWidth;
		i32 biHeight;
		u16 biPlanes;
		u16 biBitCount;
		u32 biCompression;
		u32 biSizeImage;
		i32 biXPelsPerMeter;
		i32 biYPelsPerMeter;
		u32 biClrUsed;
		u32 biClrImportant;
	} info;

	struct BMPDIBHeaderV4 // See: "BITMAPV4HEADER" @ Source(1).
	{
		u32                    bV4Size;
		i32                    bV4Width;
		i32                    bV4Height;
		u16                    bV4Planes;
		u16                    bV4BitCount;
		u32                    bV4V4Compression;
		u32                    bV4SizeImage;
		i32                    bV4XPelsPerMeter;
		i32                    bV4YPelsPerMeter;
		u32                    bV4ClrUsed;
		u32                    bV4ClrImportant;
		u32                    bV4RedMask;
		u32                    bV4GreenMask;
		u32                    bV4BlueMask;
		u32                    bV4AlphaMask;
		u32                    bV4CSType;
		struct BMPCIEXYZTRIPLE bV4Endpoints;
		u32                    bV4GammaRed;
		u32                    bV4GammaGreen;
		u32                    bV4GammaBlue;
	} v4;

	struct BMPDIBHeaderV5 // See: "BITMAPV5HEADER" @ Source(1).
	{
		u32                    bV5Size;          // Must be sizeof(struct BMPDIBHeaderV5).
		i32                    bV5Width;         // Width of image after decompression.
		i32                    bV5Height;
		u16                    bV5Planes;
		u16                    bV5BitCount;
		u32                    bV5Compression;
		u32                    bV5SizeImage;
		i32                    bV5XPelsPerMeter;
		i32                    bV5YPelsPerMeter;
		u32                    bV5ClrUsed;
		u32                    bV5ClrImportant;
		u32                    bV5RedMask;
		u32                    bV5GreenMask;
		u32                    bV5BlueMask;
		u32                    bV5AlphaMask;
		u32                    bV5CSType;
		struct BMPCIEXYZTRIPLE bV5Endpoints;
		u32                    bV5GammaRed;
		u32                    bV5GammaGreen;
		u32                    bV5GammaBlue;
		u32                    bV5Intent;
		u32                    bV5ProfileData;
		u32                    bV5ProfileSize;
		u32                    bV5Reserved;
	} v5;
};
#pragma pack(pop)

//
// Documentation.
//

/* [Overview].
	Source(1) := Library of Congress on BMPs ("Bitmap Image File (BMP), Version 5") (loc.gov/preservation/digital/formats/fdd/fdd000189.shtml/) (Dated: 2022-04-27).
*/
