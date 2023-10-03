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

//
// Documentation.
//

/* [Overview].
*/
