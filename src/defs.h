// TODO Make mouse commands not buffered.
// TODO Nerd busy LED.
// TODO Mass storage reset.
// TODO Remove magic numbers of mouse coordinates.
// TODO German words might be duplicated?
// TODO Time's up signal from Nerd.
// TODO Flip board upside down.
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
#define memeq(...)       (!memcmp(__VA_ARGS__))

typedef uint8_t                                u8;
typedef uint16_t                               u16;
typedef uint32_t                               u32;
typedef uint64_t                               u64;
typedef int8_t                                 i8;
typedef int16_t                                i16;
typedef int32_t                                i32;
typedef int64_t                                i64;
typedef int8_t                                 b8;
typedef int16_t                                b16;
typedef int32_t                                b32;
typedef int64_t                                b64;
typedef struct { u8  x; u8  y;               } u8_2;
typedef struct { u16 x; u16 y;               } u16_2;
typedef struct { u32 x; u32 y;               } u32_2;
typedef struct { u64 x; u64 y;               } u64_2;
typedef struct { i8  x; i8  y;               } i8_2;
typedef struct { i16 x; i16 y;               } i16_2;
typedef struct { i32 x; i32 y;               } i32_2;
typedef struct { i64 x; i64 y;               } i64_2;
typedef struct { b8  x; b8  y;               } b8_2;
typedef struct { b16 x; b16 y;               } b16_2;
typedef struct { b32 x; b32 y;               } b32_2;
typedef struct { b64 x; b64 y;               } b64_2;
typedef struct { u8  x; u8  y; u8  z;        } u8_3;
typedef struct { u16 x; u16 y; u16 z;        } u16_3;
typedef struct { u32 x; u32 y; u32 z;        } u32_3;
typedef struct { u64 x; u64 y; u64 z;        } u64_3;
typedef struct { i8  x; i8  y; i8  z;        } i8_3;
typedef struct { i16 x; i16 y; i16 z;        } i16_3;
typedef struct { i32 x; i32 y; i32 z;        } i32_3;
typedef struct { i64 x; i64 y; i64 z;        } i64_3;
typedef struct { b8  x; b8  y; b8  z;        } b8_3;
typedef struct { b16 x; b16 y; b16 z;        } b16_3;
typedef struct { b32 x; b32 y; b32 z;        } b32_3;
typedef struct { b64 x; b64 y; b64 z;        } b64_3;
typedef struct { u8  x; u8  y; u8  z; u8  w; } u8_4;
typedef struct { u16 x; u16 y; u16 z; u16 w; } u16_4;
typedef struct { u32 x; u32 y; u32 z; u32 w; } u32_4;
typedef struct { u64 x; u64 y; u64 z; u64 w; } u64_4;
typedef struct { i8  x; i8  y; i8  z; i8  w; } i8_4;
typedef struct { i16 x; i16 y; i16 z; i16 w; } i16_4;
typedef struct { i32 x; i32 y; i32 z; i32 w; } i32_4;
typedef struct { i64 x; i64 y; i64 z; i64 w; } i64_4;
typedef struct { b8  x; b8  y; b8  z; b8  w; } b8_4;
typedef struct { b16 x; b16 y; b16 z; b16 w; } b16_4;
typedef struct { b32 x; b32 y; b32 z; b32 w; } b32_4;
typedef struct { b64 x; b64 y; b64 z; b64 w; } b64_4;

#if PROGRAM_MICROSERVICES
	typedef float                                  f32;
	typedef double                                 f64;
	typedef struct { f32 x; f32 y;               } f32_2;
	typedef struct { f64 x; f64 y;               } f64_2;
	typedef struct { f32 x; f32 y; f32 z;        } f32_3;
	typedef struct { f64 x; f64 y; f64 z;        } f64_3;
	typedef struct { f32 x; f32 y; f32 z; f32 w; } f32_4;
	typedef struct { f64 x; f64 y; f64 z; f64 w; } f64_4;

	typedef struct { char* data; i64 length; } str;
	#define str(STRLIT) (str) { (STRLIT), sizeof(STRLIT) - 1 }
	#define STR(STRLIT)       { (STRLIT), sizeof(STRLIT) - 1 }

	#define strbuf(SIZE) ((strbuf) { .data = (char[SIZE]) {0}, .size = (SIZE) })
	typedef struct
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
	} strbuf;
#endif

static_assert(LITTLE_ENDIAN); // Lots of structures assume little-endian.

//
// "lcd.c"
//

#ifdef PROGRAM_DIPLOMAT
	#define LCD_DIM_X 20
	#define LCD_DIM_Y 4

	static char _lcd_cache [LCD_DIM_Y][LCD_DIM_X] = {0};
	static char lcd_display[LCD_DIM_Y][LCD_DIM_X] = {0};
	static u8_2 lcd_cursor_pos                    = {0};

	// Character code assumes the LCD display has ROM code of 0xA00. See: Source(25) @ Table(4) @ Page(16).
	#define LCD_RIGHT_ARROW 0b0111'1110
	#define LCD_LEFT_ARROW  0b0111'1111

	static const u8 LCD_CUSTOM_CHAR_PATTERNS[][8] PROGMEM = // Restricted to 8 rows and 5 columns. See: Source(25) @ Table(5) @ Page(19).
		{
			[0b000] = // Empty space.
				{
					0b00000,
					0b00000,
					0b00000,
					0b00000,
					0b00000,
					0b00000,
					0b00000,
					0b00000,
				},
			[0b001] = // Letter_boris.
				{
					0b11111,
					0b10001,
					0b10000,
					0b11110,
					0b10001,
					0b10001,
					0b11110,
					0b00000,
				},
			[0b010] = // Letter_chelovek.
				{
					0b10001,
					0b10001,
					0b10001,
					0b01111,
					0b00001,
					0b00001,
					0b00001,
					0b00000,
				},
			[0b011] = // Letter_ivan.
				{
					0b10001,
					0b10001,
					0b10011,
					0b10101,
					0b11001,
					0b10001,
					0b10001,
					0b00000,
				},
			[0b100] = // Letter_ivan_kratkiy.
				{
					0b01010,
					0b00100,
					0b10001,
					0b10011,
					0b10101,
					0b11001,
					0b10001,
					0b00000,
				},
			[0b101] = // Letter_shura.
				{
					0b00000,
					0b10101,
					0b10101,
					0b10101,
					0b10101,
					0b10101,
					0b11111,
					0b00000,
				},
			[0b110] = // Letter_yery.
				{
					0b10001,
					0b10001,
					0b10001,
					0b11001,
					0b10101,
					0b10101,
					0b11001,
					0b00000,
				},
			[0b111] = // Letter_zhenya.
				{
					0b10101,
					0b10101,
					0b10101,
					0b01110,
					0b10101,
					0b10101,
					0b10101,
					0b00000,
				},
		};
	static_assert(countof(LCD_CUSTOM_CHAR_PATTERNS) <= 8); // Maximum of eight custom characters are supported by the LCD.
#endif

//
// Miscellaneous.
//

#define ASSISTIVE_TOUCH_X             9
#define ASSISTIVE_TOUCH_Y             9
#define MASK_ACTIVATION_THRESHOLD     8  // Applied to red channel.
#define MASK_DIM                      64
#define MIN_WORD_LENGTH               3
#define BITS_PER_ALPHABET_INDEX       5
#define PACKED_WORD_SIZE(WORD_LENGTH) (1 + ((((WORD_LENGTH) - 1) * BITS_PER_ALPHABET_INDEX) + ((WORD_LENGTH) - 1) / 3 + 7) / 8)

#define ANAGRAMS_6_INIT_X             7
#define ANAGRAMS_6_INIT_Y             241
#define ANAGRAMS_6_DELTA              23
#define ANAGRAMS_6_SUBMIT_X           64
#define ANAGRAMS_6_SUBMIT_Y           171
static_assert(ANAGRAMS_6_INIT_X + ANAGRAMS_6_DELTA * 5 < 128);

#define ANAGRAMS_7_INIT_X             4
#define ANAGRAMS_7_INIT_Y             240
#define ANAGRAMS_7_DELTA              20
#define ANAGRAMS_7_SUBMIT_X           64
#define ANAGRAMS_7_SUBMIT_Y           171
static_assert(ANAGRAMS_7_INIT_X + ANAGRAMS_7_DELTA * 6 < 128);

#define WORDHUNT_4x4_INIT_X             27
#define WORDHUNT_4x4_INIT_Y             215
#define WORDHUNT_4x4_DELTA              25
static_assert(WORDHUNT_4x4_INIT_X + WORDHUNT_4x4_DELTA * 3 <  128);
static_assert(WORDHUNT_4x4_INIT_Y - WORDHUNT_4x4_DELTA * 3 >= 0  );

#define WORDHUNT_5x5_INIT_X             20
#define WORDHUNT_5x5_INIT_Y             220
#define WORDHUNT_5x5_DELTA              22
static_assert(WORDHUNT_5x5_INIT_X + WORDHUNT_5x5_DELTA * 4 <  128);
static_assert(WORDHUNT_5x5_INIT_Y - WORDHUNT_5x5_DELTA * 4 >= 0  );

#define WORDBITES_INIT_X             5
#define WORDBITES_INIT_Y             236
#define WORDBITES_DELTA              17
static_assert(WORDBITES_INIT_X + WORDBITES_DELTA * 7 <  128);
static_assert(WORDBITES_INIT_Y - WORDBITES_DELTA * 8 >= 0  );

static_assert(BITS_PER_ALPHABET_INDEX == 5); // PACKED_WORD_SIZE calculation assumes 5 bits per alphabet index.

#define ANAGRAMS_GENERIC_6_PRINT_NAME "Anagrams (6)"
#define ANAGRAMS_PLAY_BUTTON_Y        190
#define WORDHUNT_PLAY_BUTTON_Y        202
#define WORDBITES_PLAY_BUTTON_Y       202
#define WORDGAME_XMDT(X, Y) /* There's no good way to determine the language of Anagrams perfectly, so we will always assume it's English and have the test region all be zero. */ \
	/*    Names                                     | Language | Max Word Length | Sentinel Letter | Board Position | Board Dim (slots) | Slot Dim | Uncompressed Slot Stride | Compressed Slot Stride | Test Region Position | Test Region Dimensions | Test Region RGB       | Excluded Slot Coordinates */ \
		X(anagrams_english_6, "Anagrams (EN, 6)"    , english  , 6               , Letter_z + 1    ,  35, 391       , 6, 1              , 124      , 195                      , 101                    ,   32,  700           , 256,  16               , 0.2645, 0.2409, 0.3358                                         ) \
		X(anagrams_english_7, "Anagrams (EN, 7)"    , english  , 7               , Letter_z + 1    ,  29, 413       , 7, 1              , 106      , 167.75                   , 101                    ,   32,  656           , 256,  16               , 0.2936, 0.2699, 0.3658                                         ) \
		X(anagrams_russian  , "Anagrams (RU)"       , russian  , 6               , Letter_COUNT    ,  35, 391       , 6, 1              , 124      , 195                      , 101                    ,    0,    0           ,   0,   0               ,      0,      0,      0                                         ) \
		X(anagrams_french   , "Anagrams (FR)"       , french   , 6               , Letter_z + 1    ,  35, 391       , 6, 1              , 124      , 195                      , 101                    ,    0,    0           ,   0,   0               ,      0,      0,      0                                         ) \
		X(anagrams_german   , "Anagrams (DE)"       , german   , 6               , Letter_COUNT    ,  35, 391       , 6, 1              , 124      , 195                      , 101                    ,    0,    0           ,   0,   0               ,      0,      0,      0                                         ) \
		X(anagrams_spanish  , "Anagrams (ES)"       , spanish  , 6               , Letter_ene + 1  ,  35, 391       , 6, 1              , 124      , 195                      , 101                    ,    0,    0           ,   0,   0               ,      0,      0,      0                                         ) \
		X(anagrams_italian  , "Anagrams (IT)"       , italian  , 6               , Letter_z + 1    ,  35, 391       , 6, 1              , 124      , 195                      , 101                    ,    0,    0           ,   0,   0               ,      0,      0,      0                                         ) \
		X(wordhunt_4x4      , "WordHunt (4x4)"      , english  , 15              , Letter_z + 1    , 196, 608       , 4, 4              , 142      , 211.875                  ,  96                    ,  128,  770           , 900,  16               , 0.2204, 0.2775, 0.2089                                         ) \
		X(wordhunt_o        , "WordHunt (O)"        , english  , 15              , Letter_z + 1    , 140, 550       , 5, 5              , 125      , 191.25                   ,  97                    ,  512,  926           , 128, 128               , 0.4752, 0.6331, 0.4367, Y(0, 0) Y(0, 4) Y(4, 0) Y(4, 4) Y(2, 2)) \
		X(wordhunt_x        , "WordHunt (X)"        , english  , 15              , Letter_z + 1    , 140, 550       , 5, 5              , 125      , 191.25                   ,  97                    ,  500,  554           , 128, 128               , 0.4356, 0.5768, 0.4018, Y(0, 2) Y(2, 0) Y(4, 2) Y(2, 4)        ) \
		X(wordhunt_5x5      , "WordHunt (5x5)"      , english  , 15              , Letter_z + 1    , 140, 550       , 5, 5              , 125      , 191.25                   ,  97                    , 1050,  468           ,  64,  64               , 0.4641, 0.6239, 0.4251                                         ) \
		X(wordbites         , "WordBites"           , english  , 9               , Letter_z + 1    ,  42, 460       , 8, 9              , 104      , 140.375                  ,  87                    ,  400,  218           ,  64,  64               , 0.2675, 0.3956, 0.5153                                         )

#define LETTER_XMDT(X) \
	/* Name       | LCD Character Code | Unicode Codepoints */ \
	X(null        , 0b1111'1111        , '\0'                                                                                                          ) \
	X(a           , 'A'                , 'A'   , 'a'   , 0x0410, 0x0430, 0x00C0, 0x00E0, 0x00C1, 0x00E1, 0x00C2, 0x00E2, 0x00C3, 0x00E3, 0x00C5, 0x00E5) \
	X(b           , 'B'                , 'B'   , 'b'   , 0x0412, 0x0432                                                                                ) \
	X(c           , 'C'                , 'C'   , 'c'   , 0x0421, 0x0441, 0x00C7, 0x00E7, 0x010C, 0x010D                                                ) \
	X(d           , 'D'                , 'D'   , 'd'                                                                                                   ) \
	X(e           , 'E'                , 'E'   , 'e'   , 0x0415, 0x0435, 0x00C8, 0x00E8, 0x00C9, 0x00E9, 0x00CA, 0x00EA, 0x0112, 0x0113                ) \
	X(f           , 'F'                , 'F'   , 'f'                                                                                                   ) \
	X(g           , 'G'                , 'G'   , 'g'                                                                                                   ) \
	X(h           , 'H'                , 'H'   , 'h'   , 0x041D, 0x043D                                                                                ) \
	X(i           , 'I'                , 'I'   , 'i'   , 0x0406, 0x0456, 0x00CC, 0x00EC, 0x00CD, 0x00ED, 0x00CE, 0x00EE, 0x012A, 0x012B                ) \
	X(j           , 'J'                , 'J'   , 'j'   , 0x0408, 0x0458                                                                                ) \
	X(k           , 'K'                , 'K'   , 'k'   , 0x041A, 0x043A                                                                                ) \
	X(l           , 'L'                , 'L'   , 'l'                   , 0x0141, 0x0142                                                                ) \
	X(m           , 'M'                , 'M'   , 'm'   , 0x041C, 0x043C                                                                                ) \
	X(n           , 'N'                , 'N'   , 'n'                                                                                                   ) \
	X(o           , 'O'                , 'O'   , 'o'   , 0x041E, 0x043E, 0x00D5, 0x00F5, 0x00D2, 0x00F2, 0x00D3, 0x00F3, 0x014C, 0x014D                ) \
	X(p           , 'P'                , 'P'   , 'p'   , 0x0420, 0x0440                                                                                ) \
	X(q           , 'Q'                , 'Q'   , 'q'                                                                                                   ) \
	X(r           , 'R'                , 'R'   , 'r'                                                                                                   ) \
	X(s           , 'S'                , 'S'   , 's'   , 0x0405, 0x0455, 0x0160, 0x0161                                                                ) \
	X(t           , 'T'                , 'T'   , 't'   , 0x0422, 0x0442                                                                                ) \
	X(u           , 'U'                , 'U'   , 'u'                   , 0x00D9, 0x00F9, 0x00DA, 0x00FA, 0x00DB, 0x00FB, 0x016A, 0x016B                ) \
	X(v           , 'V'                , 'V'   , 'v'                                                                                                   ) \
	X(w           , 'W'                , 'W'   , 'w'                                                                                                   ) \
	X(x           , 'X'                , 'X'   , 'x'   , 0x0425, 0x0445                                                                                ) \
	X(y           , 'Y'                , 'Y'   , 'y'                                                                                                   ) \
	X(z           , 'Z'                , 'Z'   , 'z'                                                                                                   ) /* English letters must be contiguous up to prune out foreign letters that don't need to be processed. */ \
	X(ene         , 0b1110'1110        , 0x00D1, 0x00F1                                                                                                ) /* Same reason as above; Spanish alphabet essentially consists the basic 26 Latin letters plus N with tilde. */ \
	X(boris       , 0b001              , 0x0411, 0x0431                                                                                                ) \
	X(chelovek    , 0b010              , 0x0427, 0x0447                                                                                                ) \
	X(dmitri      , 0b1101'1011        , 0x0414, 0x0434                                                                                                ) \
	X(fyodor      , '0'                , 0x0424, 0x0444                                                                                                ) \
	X(gregory     , 'r'                , 0x0413, 0x0433                                                                                                ) \
	X(ivan        , 0b011              , 0x0418, 0x0438                                                                                                ) \
	X(ivan_kratkiy, 0b100              , 0x0419, 0x0439                                                                                                ) \
	X(leonid      , 0b1011'0110        , 0x041B, 0x043B                                                                                                ) \
	X(myagkiy_znak, 'b'                , 0x042C, 0x044C                                                                                                ) \
	X(pavel       , 0b1111'0111        , 0x041F, 0x043F                                                                                                ) \
	X(shura       , 0b101              , 0x0428, 0x0448                                                                                                ) \
	X(ulyana      , 'y'                , 0x0423, 0x0443                                                                                                ) \
	X(yery        , 0b110              , 0x042B, 0x044B                                                                                                ) \
	X(zhenya      , 0b111              , 0x0416, 0x0436                                                                                                ) \
	X(zinaida     , '3'                , 0x0417, 0x0437                                                                                                ) \
	X(a_umlaut    , 0b1110'0001        , 0x04D2, 0x00C4, 0x00C4, 0x00E4                                                                                ) \
	X(o_umlaut    , 0b1110'1111        , 0x04E6, 0x00F6, 0x00D6, 0x00F6                                                                                )

#define LANGUAGE_XMDT(X) \
	X(english, Letter_a, Letter_b       , Letter_c, Letter_d      , Letter_e     , Letter_f, Letter_g     , Letter_h      , Letter_i   , Letter_j           , Letter_k, Letter_l     , Letter_m, Letter_n, Letter_o, Letter_p    , Letter_q       , Letter_r, Letter_s, Letter_t     , Letter_u     , Letter_v, Letter_w       , Letter_x    , Letter_y   , Letter_z                      ) \
	X(russian, Letter_a, Letter_boris   , Letter_b, Letter_gregory, Letter_dmitri, Letter_e, Letter_zhenya, Letter_zinaida, Letter_ivan, Letter_ivan_kratkiy, Letter_k, Letter_leonid, Letter_m, Letter_h, Letter_o, Letter_pavel, Letter_p       , Letter_c, Letter_t, Letter_ulyana, Letter_fyodor, Letter_x, Letter_chelovek, Letter_shura, Letter_yery, Letter_myagkiy_znak           ) /* No records of e_umlaut, tsaplya, shchuka, tvyordiy_znak, echo, yuri, or yakov. */ \
	X(french , Letter_a, Letter_b       , Letter_c, Letter_d      , Letter_e     , Letter_f, Letter_g     , Letter_h      , Letter_i   , Letter_j           , Letter_k, Letter_l     , Letter_m, Letter_n, Letter_o, Letter_p    , Letter_q       , Letter_r, Letter_s, Letter_t     , Letter_u     , Letter_v, Letter_w       , Letter_x    , Letter_y   , Letter_z                      ) \
	X(german , Letter_a, Letter_a_umlaut, Letter_b, Letter_c      , Letter_d     , Letter_e, Letter_f     , Letter_g      , Letter_h   , Letter_i           , Letter_j, Letter_k     , Letter_l, Letter_m, Letter_n, Letter_o    , Letter_o_umlaut, Letter_p, Letter_q, Letter_r     , Letter_s     , Letter_t, Letter_u       , Letter_v    , Letter_w   , Letter_x, Letter_y  , Letter_z) /* No records of eszett or u_umlaut. */ \
	X(spanish, Letter_a, Letter_b       , Letter_c, Letter_d      , Letter_e     , Letter_f, Letter_g     , Letter_h      , Letter_i   , Letter_j           , Letter_k, Letter_l     , Letter_m, Letter_n, Letter_o, Letter_p    , Letter_q       , Letter_r, Letter_s, Letter_t     , Letter_u     , Letter_v, Letter_w       , Letter_x    , Letter_y   , Letter_z, Letter_ene          ) \
	X(italian, Letter_a, Letter_b       , Letter_c, Letter_d      , Letter_e     , Letter_f, Letter_g     , Letter_h      , Letter_i   , Letter_l           , Letter_m, Letter_n     , Letter_o, Letter_p, Letter_q, Letter_r    , Letter_s       , Letter_t, Letter_u, Letter_v     , Letter_z                                                                                           )

#define WORD_STREAM_NAME      "words"
#define WORD_STREAM_EXTENSION "bin"

#if PROGRAM_MICROSERVICES
	static const u32 BLACKLISTED_CODEPOINTS[] =
		{
			0x0426, 0x0446,                 // tsaplya.
			0x0429, 0x0449,                 // shchuka.
			0x042D, 0x044D,                 // echo.
			0x042E, 0x044E,                 // yuri.
			0x042F, 0x044F,                 // yakov.
			0x042A, 0x044A,                 // tvyordiy_znak.
			0x0401, 0x0451, 0x00CB, 0x00EB, // e_umlaut.
			0x00DC, 0x00FC,                 // u_umlaut.
			0x0053, 0x00DF,                 // eszett.
			0x0152, 0x0153,                 // oe.
		};
#endif

#pragma pack(push, 1)
struct WordsTableOfContentEntry
{
	u16 sector_index;
	u16 count;
};
#pragma pack(pop)

#define WORDGAME_MAX_PRINT_NAME_SIZE_(IDENTIFIER_NAME, PRINT_NAME, ...) u8 IDENTIFIER_NAME[sizeof(PRINT_NAME)];
#define WORDGAME_MAX_DIM_SLOTS_X_(IDENTIFIER_NAME, PRINT_NAME, LANGUAGE, MAX_WORD_LENGTH, SENTINEL_LETTER, POS_X, POS_Y, DIM_SLOTS_X, DIM_SLOTS_Y, ...) u8 IDENTIFIER_NAME[DIM_SLOTS_X];
#define WORDGAME_MAX_DIM_SLOTS_Y_(IDENTIFIER_NAME, PRINT_NAME, LANGUAGE, MAX_WORD_LENGTH, SENTINEL_LETTER, POS_X, POS_Y, DIM_SLOTS_X, DIM_SLOTS_Y, ...) u8 IDENTIFIER_NAME[DIM_SLOTS_Y];
#define WORDGAME_MAX_PRINT_NAME_LENGTH_(IDENTIFIER_NAME, PRINT_NAME, ...) u8 IDENTIFIER_NAME[sizeof(PRINT_NAME)];
#define LETTER_MAX_NAME_LENGTH_(NAME, ...) u8 NAME[sizeof(#NAME) - 1];
#define LETTER_MAX_CODEPOINTS_(NAME, LCD_CHARACTER_CODE, ...) u8 NAME[countof((u16[]) { __VA_ARGS__ })];
#define MAX_ALPHABET_LENGTH_(NAME, ...) u8 NAME[countof((enum Letter[]) { __VA_ARGS__ })];
#define ABSOLUTE_MAX_WORD_LENGTH_(IDENTIFIER_NAME, PRINT_NAME, LANGUAGE, MAX_WORD_LENGTH, ...) u8 IDENTIFIER_NAME[MAX_WORD_LENGTH];

#define WORDGAME_MAX_PRINT_NAME_SIZE    sizeof(union { WORDGAME_XMDT(WORDGAME_MAX_PRINT_NAME_SIZE_,) })
#define WORDGAME_MAX_DIM_SLOTS_X        sizeof(union { WORDGAME_XMDT(WORDGAME_MAX_DIM_SLOTS_X_,) })
#define WORDGAME_MAX_DIM_SLOTS_Y        sizeof(union { WORDGAME_XMDT(WORDGAME_MAX_DIM_SLOTS_Y_,) })
#define WORDGAME_MAX_DIM_SLOTS          (WORDGAME_MAX_DIM_SLOTS_X > WORDGAME_MAX_DIM_SLOTS_Y ? WORDGAME_MAX_DIM_SLOTS_X : WORDGAME_MAX_DIM_SLOTS_Y)
#define WORDGAME_MAX_PRINT_NAME_LENGTH (sizeof(union { WORDGAME_XMDT(WORDGAME_MAX_PRINT_NAME_LENGTH_,) }) - 1)
#define LETTER_MAX_NAME_LENGTH          sizeof(union { LETTER_XMDT(LETTER_MAX_NAME_LENGTH_) })
#define LETTER_MAX_CODEPOINTS           sizeof(union { LETTER_XMDT(LETTER_MAX_CODEPOINTS_) })
#define MAX_ALPHABET_LENGTH             sizeof(union { LANGUAGE_XMDT(MAX_ALPHABET_LENGTH_) })
#define ABSOLUTE_MAX_WORD_LENGTH        sizeof(union { WORDGAME_XMDT(ABSOLUTE_MAX_WORD_LENGTH_,) })

enum Letter
{
	#define MAKE(NAME, ...) Letter_##NAME,
	LETTER_XMDT(MAKE)
	#undef MAKE
	Letter_COUNT,
};

struct LetterInfo
{
	u8 lcd_character_code;

	#if PROGRAM_MICROSERVICES
		str name;
		u32 codepoints[LETTER_MAX_CODEPOINTS];
	#endif
};

struct LanguageInfo
{
	enum Letter alphabet[MAX_ALPHABET_LENGTH];
	u8          alphabet_length;

	#if PROGRAM_MICROSERVICES
		str name;
	#endif
};

enum Language
{
	#define MAKE(NAME, ...) Language_##NAME,
	LANGUAGE_XMDT(MAKE)
	#undef MAKE
	Language_COUNT,
};

enum WordGame
{
	#define MAKE(IDENTIFIER_NAME, ...) WordGame_##IDENTIFIER_NAME,
	WORDGAME_XMDT(MAKE,)
	#undef MAKE
	WordGame_COUNT,
};

struct WordGameInfo
{
	u8_2 dim_slots;

	#if PROGRAM_DIPLOMAT
		enum Letter sentinel_letter;
	#endif

	#if PROGRAM_DIPLOMAT || PROGRAM_MICROSERVICES
		char print_name_cstr[WORDGAME_MAX_PRINT_NAME_SIZE];
		u8   compressed_slot_stride;
	#endif

	#if PROGRAM_NERD || PROGRAM_MICROSERVICES
		enum Language language;
		u8            max_word_length;
	#endif

	#if PROGRAM_MICROSERVICES
		str   print_name;
		i32_2 pos;
		i32   slot_dim;
		f64   uncompressed_slot_stride;
		i32_2 test_region_pos;
		i32_2 test_region_dim;
		f64_3 test_region_rgb;
	#endif
};

static_assert(MAX_ALPHABET_LENGTH <= (1 << BITS_PER_ALPHABET_INDEX)); // Alphabet too long to be able to index.

#if PROGRAM_DIPLOMAT
	#define count_cleared_bits(...) pgm_u8(COUNT_CLEARED_BITS_DT_[(u8) { __VA_ARGS__ }])
	static const u8 COUNT_CLEARED_BITS_DT_[] PROGMEM =
		{
			[0b00000000] = 8, [0b00000001] = 7, [0b00000010] = 7, [0b00000011] = 6, [0b00000100] = 7, [0b00000101] = 6, [0b00000110] = 6, [0b00000111] = 5,
			[0b00001000] = 7, [0b00001001] = 6, [0b00001010] = 6, [0b00001011] = 5, [0b00001100] = 6, [0b00001101] = 5, [0b00001110] = 5, [0b00001111] = 4,
			[0b00010000] = 7, [0b00010001] = 6, [0b00010010] = 6, [0b00010011] = 5, [0b00010100] = 6, [0b00010101] = 5, [0b00010110] = 5, [0b00010111] = 4,
			[0b00011000] = 6, [0b00011001] = 5, [0b00011010] = 5, [0b00011011] = 4, [0b00011100] = 5, [0b00011101] = 4, [0b00011110] = 4, [0b00011111] = 3,
			[0b00100000] = 7, [0b00100001] = 6, [0b00100010] = 6, [0b00100011] = 5, [0b00100100] = 6, [0b00100101] = 5, [0b00100110] = 5, [0b00100111] = 4,
			[0b00101000] = 6, [0b00101001] = 5, [0b00101010] = 5, [0b00101011] = 4, [0b00101100] = 5, [0b00101101] = 4, [0b00101110] = 4, [0b00101111] = 3,
			[0b00110000] = 6, [0b00110001] = 5, [0b00110010] = 5, [0b00110011] = 4, [0b00110100] = 5, [0b00110101] = 4, [0b00110110] = 4, [0b00110111] = 3,
			[0b00111000] = 5, [0b00111001] = 4, [0b00111010] = 4, [0b00111011] = 3, [0b00111100] = 4, [0b00111101] = 3, [0b00111110] = 3, [0b00111111] = 2,
			[0b01000000] = 7, [0b01000001] = 6, [0b01000010] = 6, [0b01000011] = 5, [0b01000100] = 6, [0b01000101] = 5, [0b01000110] = 5, [0b01000111] = 4,
			[0b01001000] = 6, [0b01001001] = 5, [0b01001010] = 5, [0b01001011] = 4, [0b01001100] = 5, [0b01001101] = 4, [0b01001110] = 4, [0b01001111] = 3,
			[0b01010000] = 6, [0b01010001] = 5, [0b01010010] = 5, [0b01010011] = 4, [0b01010100] = 5, [0b01010101] = 4, [0b01010110] = 4, [0b01010111] = 3,
			[0b01011000] = 5, [0b01011001] = 4, [0b01011010] = 4, [0b01011011] = 3, [0b01011100] = 4, [0b01011101] = 3, [0b01011110] = 3, [0b01011111] = 2,
			[0b01100000] = 6, [0b01100001] = 5, [0b01100010] = 5, [0b01100011] = 4, [0b01100100] = 5, [0b01100101] = 4, [0b01100110] = 4, [0b01100111] = 3,
			[0b01101000] = 5, [0b01101001] = 4, [0b01101010] = 4, [0b01101011] = 3, [0b01101100] = 4, [0b01101101] = 3, [0b01101110] = 3, [0b01101111] = 2,
			[0b01110000] = 5, [0b01110001] = 4, [0b01110010] = 4, [0b01110011] = 3, [0b01110100] = 4, [0b01110101] = 3, [0b01110110] = 3, [0b01110111] = 2,
			[0b01111000] = 4, [0b01111001] = 3, [0b01111010] = 3, [0b01111011] = 2, [0b01111100] = 3, [0b01111101] = 2, [0b01111110] = 2, [0b01111111] = 1,
			[0b10000000] = 7, [0b10000001] = 6, [0b10000010] = 6, [0b10000011] = 5, [0b10000100] = 6, [0b10000101] = 5, [0b10000110] = 5, [0b10000111] = 4,
			[0b10001000] = 6, [0b10001001] = 5, [0b10001010] = 5, [0b10001011] = 4, [0b10001100] = 5, [0b10001101] = 4, [0b10001110] = 4, [0b10001111] = 3,
			[0b10010000] = 6, [0b10010001] = 5, [0b10010010] = 5, [0b10010011] = 4, [0b10010100] = 5, [0b10010101] = 4, [0b10010110] = 4, [0b10010111] = 3,
			[0b10011000] = 5, [0b10011001] = 4, [0b10011010] = 4, [0b10011011] = 3, [0b10011100] = 4, [0b10011101] = 3, [0b10011110] = 3, [0b10011111] = 2,
			[0b10100000] = 6, [0b10100001] = 5, [0b10100010] = 5, [0b10100011] = 4, [0b10100100] = 5, [0b10100101] = 4, [0b10100110] = 4, [0b10100111] = 3,
			[0b10101000] = 5, [0b10101001] = 4, [0b10101010] = 4, [0b10101011] = 3, [0b10101100] = 4, [0b10101101] = 3, [0b10101110] = 3, [0b10101111] = 2,
			[0b10110000] = 5, [0b10110001] = 4, [0b10110010] = 4, [0b10110011] = 3, [0b10110100] = 4, [0b10110101] = 3, [0b10110110] = 3, [0b10110111] = 2,
			[0b10111000] = 4, [0b10111001] = 3, [0b10111010] = 3, [0b10111011] = 2, [0b10111100] = 3, [0b10111101] = 2, [0b10111110] = 2, [0b10111111] = 1,
			[0b11000000] = 6, [0b11000001] = 5, [0b11000010] = 5, [0b11000011] = 4, [0b11000100] = 5, [0b11000101] = 4, [0b11000110] = 4, [0b11000111] = 3,
			[0b11001000] = 5, [0b11001001] = 4, [0b11001010] = 4, [0b11001011] = 3, [0b11001100] = 4, [0b11001101] = 3, [0b11001110] = 3, [0b11001111] = 2,
			[0b11010000] = 5, [0b11010001] = 4, [0b11010010] = 4, [0b11010011] = 3, [0b11010100] = 4, [0b11010101] = 3, [0b11010110] = 3, [0b11010111] = 2,
			[0b11011000] = 4, [0b11011001] = 3, [0b11011010] = 3, [0b11011011] = 2, [0b11011100] = 3, [0b11011101] = 2, [0b11011110] = 2, [0b11011111] = 1,
			[0b11100000] = 5, [0b11100001] = 4, [0b11100010] = 4, [0b11100011] = 3, [0b11100100] = 4, [0b11100101] = 3, [0b11100110] = 3, [0b11100111] = 2,
			[0b11101000] = 4, [0b11101001] = 3, [0b11101010] = 3, [0b11101011] = 2, [0b11101100] = 3, [0b11101101] = 2, [0b11101110] = 2, [0b11101111] = 1,
			[0b11110000] = 4, [0b11110001] = 3, [0b11110010] = 3, [0b11110011] = 2, [0b11110100] = 3, [0b11110101] = 2, [0b11110110] = 2, [0b11110111] = 1,
			[0b11111000] = 3, [0b11111001] = 2, [0b11111010] = 2, [0b11111011] = 1, [0b11111100] = 2, [0b11111101] = 1, [0b11111110] = 1, [0b11111111] = 0,
		};

	static const struct WordGameInfo WORDGAME_INFO[] PROGMEM =
		{
			#define MAKE( \
				IDENTIFIER_NAME, \
				PRINT_NAME, \
				LANGUAGE, \
				MAX_WORD_LENGTH, \
				SENTINEL_LETTER, \
				POS_X, POS_Y, \
				DIM_SLOTS_X, DIM_SLOTS_Y, \
				SLOT_DIM, \
				UNCOMPRESSED_SLOT_STRIDE, \
				COMPRESSED_SLOT_STRIDE, \
				TEST_REGION_POS_X, TEST_REGION_POS_Y, \
				TEST_REGION_DIM_X, TEST_REGION_DIM_Y, \
				TEST_REGION_R, TEST_REGION_G, TEST_REGION_B, \
				... \
			) \
				{ \
					.print_name_cstr        = PRINT_NAME, \
					.sentinel_letter        = SENTINEL_LETTER, \
					.dim_slots.x            = DIM_SLOTS_X, \
					.dim_slots.y            = DIM_SLOTS_Y, \
					.compressed_slot_stride = COMPRESSED_SLOT_STRIDE, \
				},
			WORDGAME_XMDT(MAKE,)
			#undef MAKE
		};

	static const struct LetterInfo LETTER_INFO[] PROGMEM =
		{
			#define MAKE(NAME, LCD_CHARACTER_CODE, ...) \
				{ \
					.lcd_character_code = LCD_CHARACTER_CODE, \
				},
			LETTER_XMDT(MAKE)
			#undef MAKE
		};
#endif

#if PROGRAM_NERD
	static const struct WordGameInfo WORDGAME_INFO[] PROGMEM =
		{
			#define MAKE( \
				IDENTIFIER_NAME, \
				PRINT_NAME, \
				LANGUAGE, \
				MAX_WORD_LENGTH, \
				SENTINEL_LETTER, \
				POS_X, POS_Y, \
				DIM_SLOTS_X, DIM_SLOTS_Y, \
				SLOT_DIM, \
				UNCOMPRESSED_SLOT_STRIDE, \
				COMPRESSED_SLOT_STRIDE, \
				TEST_REGION_POS_X, TEST_REGION_POS_Y, \
				TEST_REGION_DIM_X, TEST_REGION_DIM_Y, \
				TEST_REGION_R, TEST_REGION_G, TEST_REGION_B, \
				... \
			) \
				{ \
					.dim_slots       = { DIM_SLOTS_X, DIM_SLOTS_Y }, \
					.language        = Language_##LANGUAGE, \
					.max_word_length = MAX_WORD_LENGTH, \
				},
			WORDGAME_XMDT(MAKE,)
			#undef MAKE
		};

	static const struct LanguageInfo LANGUAGE_INFO[] PROGMEM =
		{
			#define MAKE(NAME, ...) \
				{ \
					.alphabet        = { __VA_ARGS__ }, \
					.alphabet_length = countof((enum Letter[]) { __VA_ARGS__ }), \
				},
			LANGUAGE_XMDT(MAKE)
			#undef MAKE
		};
#endif

#if PROGRAM_MICROSERVICES
	static const struct WordGameInfo WORDGAME_INFO[] =
		{
			#define MAKE( \
				IDENTIFIER_NAME, \
				PRINT_NAME, \
				LANGUAGE, \
				MAX_WORD_LENGTH, \
				SENTINEL_LETTER, \
				POS_X, POS_Y, \
				DIM_SLOTS_X, DIM_SLOTS_Y, \
				SLOT_DIM, \
				UNCOMPRESSED_SLOT_STRIDE, \
				COMPRESSED_SLOT_STRIDE, \
				TEST_REGION_POS_X, TEST_REGION_POS_Y, \
				TEST_REGION_DIM_X, TEST_REGION_DIM_Y, \
				TEST_REGION_R, TEST_REGION_G, TEST_REGION_B, \
				... \
			) \
				{ \
					.print_name_cstr          = PRINT_NAME, \
					.print_name               = STR(PRINT_NAME), \
					.language                 = Language_##LANGUAGE, \
					.max_word_length          = MAX_WORD_LENGTH, \
					.pos.x                    = POS_X, \
					.pos.y                    = POS_Y, \
					.dim_slots.x              = DIM_SLOTS_X, \
					.dim_slots.y              = DIM_SLOTS_Y, \
					.slot_dim                 = SLOT_DIM, \
					.uncompressed_slot_stride = UNCOMPRESSED_SLOT_STRIDE, \
					.compressed_slot_stride   = COMPRESSED_SLOT_STRIDE, \
					.test_region_pos.x        = TEST_REGION_POS_X, \
					.test_region_pos.y        = TEST_REGION_POS_Y, \
					.test_region_dim.x        = TEST_REGION_DIM_X, \
					.test_region_dim.y        = TEST_REGION_DIM_Y, \
					.test_region_rgb.x        = TEST_REGION_R, \
					.test_region_rgb.y        = TEST_REGION_G, \
					.test_region_rgb.z        = TEST_REGION_B, \
				},
			WORDGAME_XMDT(MAKE,)
			#undef MAKE
		};

	static const struct LetterInfo LETTER_INFO[] =
		{
			#define MAKE(NAME, LCD_CHARACTER_CODE, ...) \
				{ \
					.lcd_character_code = LCD_CHARACTER_CODE, \
					.name               = STR(#NAME), \
					.codepoints         = { __VA_ARGS__ }, \
				},
			LETTER_XMDT(MAKE)
			#undef MAKE
		};

	static const struct LanguageInfo LANGUAGE_INFO[] =
		{
			#define MAKE(NAME, ...) \
				{ \
					.alphabet        = { __VA_ARGS__ }, \
					.alphabet_length = countof((enum Letter[]) { __VA_ARGS__ }), \
					.name            = STR(#NAME), \
				},
			LANGUAGE_XMDT(MAKE)
			#undef MAKE
		};
#endif

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

Dary_def(u8   );
Dary_def(u16  );
Dary_def(u32  );
Dary_def(u64  );
Dary_def(i8   );
Dary_def(i16  );
Dary_def(i32  );
Dary_def(i64  );
Dary_def(u8_2 );
Dary_def(u16_2);
Dary_def(u32_2);
Dary_def(u64_2);
Dary_def(i8_2 );
Dary_def(i16_2);
Dary_def(i32_2);
Dary_def(i64_2);
Dary_def(u8_3 );
Dary_def(u16_3);
Dary_def(u32_3);
Dary_def(u64_3);
Dary_def(i8_3 );
Dary_def(i16_3);
Dary_def(i32_3);
Dary_def(i64_3);
Dary_def(u8_4 );
Dary_def(u16_4);
Dary_def(u32_4);
Dary_def(u64_4);
Dary_def(i8_4 );
Dary_def(i16_4);
Dary_def(i32_4);
Dary_def(i64_4);

Dary_define(Letter, enum Letter);

//
// "Microservices_bmp.c"
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
	i32_2            dim;
};

enum BMPCompression // See: Source(23) @ Section(2.1.1.7) @ Page(119).
{
	BMPCompression_RGB       = 0x0,
	BMPCompression_RLE8      = 0x1,
	BMPCompression_RLE4      = 0x2,
	BMPCompression_BITFIELDS = 0x3,
	BMPCompression_JPEG      = 0x4,
	BMPCompression_PNG       = 0x5,
	BMPCompression_CMYK      = 0xB,
	BMPCompression_CMYKRLE8  = 0xC,
	BMPCompression_CMYKRLE4  = 0xD,
};

struct BMPRGBQuad // See: "RGBQUAD" @ Source(22) @ Page(1344).
{
	u8 rgbBlue;
	u8 rgbGreen;
	u8 rgbRed;
	u8 rgbReserved;
};

#pragma pack(push, 1)
struct BMPFileHeader // See: "BITMAPFILEHEADER" @ Source(22) @ Page(281).
{
	#define BMP_FILE_HEADER_SIGNATURE (((u16) 'B') | ((u16) ('M' << 8)))
	u16 bfType;      // Must be BMP_FILE_HEADER_SIGNATURE.
	u32 bfSize;      // Size of the entire BMP file in bytes.
	u16 bfReserved1;
	u16 bfReserved2;
	u32 bfOffBits;   // Byte offset into the BMP file where the image's pixel data is stored.
};
#pragma pack(pop)

#pragma pack(push, 1)
struct BMPDIBHeader // "BITMAPCOREHEADER" not supported.
{
	//
	// Base fields of "BITMAPINFOHEADER". See: Source(22) @ Page(287).
	//

	u32 Size;          // Must be at least 40.
	i32 Width;         // Width of image after decompression.
	i32 Height;        // Generally, height of image where positive is bottom-up and negative is top-down, but check documentation.
	u16 Planes;        // Must be 1.
	u16 BitCount;      // Bits per pixel.
	u32 Compression;
	u32 SizeImage;     // Byte count of the image; can be zero for uncompressed RGB images.
	i32 XPelsPerMeter;
	i32 YPelsPerMeter;
	u32 ClrUsed;       // Colors used in color table; zero means the colors used is determined with 2^biBitCount.
	u32 ClrImportant;

	//
	// Later additions of the DIB header simply append new fields. See: Source(24).
	//

	#define BMPDIBHEADER_MIN_SIZE_V2 52
	struct // See: "BITMAPV2INFOHEADER" @ Source(24).
	{
		u32 RedMask;
		u32 GreenMask;
		u32 BlueMask;
	} v2;

	#define BMPDIBHEADER_MIN_SIZE_V3 56
	struct // See: "BITMAPV3INFOHEADER" @ Source(24).
	{
		u32 AlphaMask;
	} v3;

	#define BMPDIBHEADER_MIN_SIZE_V4 108
	struct // See: "BITMAPV4HEADER" @ Source(22) @ Page(293).
	{
		u32 CSType;
		u32 Endpoints[9];
		u32 GammaRed;
		u32 GammaGreen;
		u32 GammaBlue;
	} v4;

	#define BMPDIBHEADER_MIN_SIZE_V5 124
	struct // See: "BITMAPV5HEADER" @ Source(22) @ Page(300).
	{
		u32 Intent;
		u32 ProfileData;
		u32 ProfileSize;
		u32 Reserved;
	} v5;
};
#pragma pack(pop)

//
// "Microservices.c".
//

#define SCREENSHOT_DIM_X        1170
#define SCREENSHOT_DIM_Y        2532
#define TEST_REGION_RGB_EPSILON 0.015

#define CLI_EXE_NAME "Microservices.exe"
#define CLI_EXE_DESC "Microservices to help you bring change to the world."
#define CLI_PROGRAM_XMDT(X) \
	X(eaglepeek   , "Identify the Game Pigeon word game shown in screenshots.") \
	X(extractorv2 , "Create mask-sized monochrome BMP of each slot in screenshots of Game Pigeon word games.") \
	X(collectune  , "Copy and sort BMPs into the folder with the closest matching mask.") \
	X(maskiversev2, "Format masks into streaming data to be included into C compilation.") \
	X(wordy       , "Compress dictionary text files.")

#define CLI_PROGRAM_eaglepeek_FIELD_XMDT(X, ...) \
	X(input_dir_paths, dary_string, "screenshot-dir-path...", "Directory path of the screenshots to identify.",##__VA_ARGS__) \

#define CLI_PROGRAM_extractorv2_FIELD_XMDT(X, ...) \
	X(output_dir_path , string     , "output-dir-path"   , "Destination directory to store the extracted BMPs.",##__VA_ARGS__) \
	X(input_dir_paths , dary_string, "input-dir-path..." , "Directory paths that'll be filtered for screenshots of the games.",##__VA_ARGS__) \
	X(clear_output_dir, b32        , "--clear-output-dir", "Delete all content within the output directory before processing.",##__VA_ARGS__)

#define CLI_PROGRAM_collectune_FIELD_XMDT(X, ...) \
	X(output_dir_path  , string, "output-dir-path"   , "Destination directory to store the collections.",##__VA_ARGS__) \
	X(mask_dir_path    , string, "mask-dir-path"     , "Directory path of the masks.",##__VA_ARGS__) \
	X(unsorted_dir_path, string, "unsorted-dir-path" , "Directory path of the BMPs to be sorted.",##__VA_ARGS__) \
	X(clear_output_dir , b32   , "--clear-output-dir", "Delete all content within the output directory before processing.",##__VA_ARGS__)

#define CLI_PROGRAM_maskiversev2_FIELD_XMDT(X, ...) \
	X(output_file_path, string, "output-file-path", "File path of the formatted data.",##__VA_ARGS__) \
	X(dir_path        , string, "mask-dir-path"   , "Directory path of the mask BMPs.",##__VA_ARGS__)

#define CLI_PROGRAM_wordy_FIELD_XMDT(X, ...) \
	X(dir_path, string, "dir-path", "Directory path of the dictionary text files.",##__VA_ARGS__) \

#define CLI_TYPING_XMDT(X) \
	X(string     , union { struct { char* data; i64 length; }; char* cstr; str str; }) \
	X(dary_string, struct Dary_CLIFieldTyping_string_t) \
	X(b32        , b32)

#define CLI_PROGRAM_MAX_FIELDS__(PROGRAM_NAME, ...) +1
#define CLI_PROGRAM_MAX_FIELDS_(PROGRAM_NAME, ...) u8 IDENTIFIER_NAME[CLI_PROGRAM_##PROGRAM_NAME##_FIELD_XMDT(CLI_PROGRAM_MAX_FIELDS__)];
#define CLI_PROGRAM_MAX_FIELDS sizeof(union { CLI_PROGRAM_XMDT(LETTER_MAX_NAME_LENGTH_) })

enum CLIFieldTyping
{
	#define MAKE(TYPING_NAME, TYPING_TYPE) CLIFieldTyping_##TYPING_NAME,
	CLI_TYPING_XMDT(MAKE)
	#undef MAKE
};

enum CLIProgram
{
	#define MAKE(PROGRAM_NAME, PROGRAM_DESC) CLIProgram_##PROGRAM_NAME,
	CLI_PROGRAM_XMDT(MAKE)
	#undef MAKE
	CLIProgram_COUNT,
};

#if PROGRAM_MICROSERVICES
	struct CLIFieldInfo
	{
		i64                 offset;
		enum CLIFieldTyping typing;
		str                 pattern;
		str                 desc;
	};

	struct CLIProgramInfo
	{
		str                 name;
		str                 desc;
		struct CLIFieldInfo fields[CLI_PROGRAM_MAX_FIELDS];
		i32                 field_count;
	};

	//
	// Make typedef of each CLI field type.
	//

	#define MAKE(TYPING_NAME, TYPING_TYPE) typedef TYPING_TYPE CLIFieldTyping_##TYPING_NAME##_t;
	CLI_TYPING_XMDT(MAKE)
	#undef MAKE
	Dary_def(CLIFieldTyping_string_t);

	//
	// Make enum of CLI fields for each CLI program.
	//

	#define MAKE_CLI_FIELD(PROGRAM_NAME, PROGRAM_DESC) \
		enum CLIField_##PROGRAM_NAME \
		{ \
			CLI_PROGRAM_##PROGRAM_NAME##_FIELD_XMDT(MAKE_CLI_FIELD_MEMBERS, PROGRAM_NAME) \
			CLIField_##PROGRAM_NAME##_COUNT \
		};
	#define MAKE_CLI_FIELD_MEMBERS(FIELD_NAME, FIELD_TYPING_NAME, FIELD_PATTERN, FIELD_DESC, PROGRAM_NAME) \
		CLIField_##PROGRAM_NAME##_##FIELD_NAME,
	CLI_PROGRAM_XMDT(MAKE_CLI_FIELD)
	#undef MAKE_CLI_FIELD_MEMBERS
	#undef MAKE_CLI_FIELD

	//
	// Make CLI program field members.
	//

	struct CLI
	{
		#define MAKE_CLI_PROGRAM(PROGRAM_NAME, PROGRAM_DESC) \
			struct CLIProgram_##PROGRAM_NAME##_t \
			{ \
				CLI_PROGRAM_##PROGRAM_NAME##_FIELD_XMDT(MAKE_CLI_PROGRAM_MEMBERS) \
			} PROGRAM_NAME;
		#define MAKE_CLI_PROGRAM_MEMBERS(FIELD_NAME, FIELD_TYPING_NAME, FIELD_PATTERN, FIELD_DESC) \
			CLIFieldTyping_##FIELD_TYPING_NAME##_t FIELD_NAME;
		CLI_PROGRAM_XMDT(MAKE_CLI_PROGRAM)
		#undef MAKE_CLI_PROGRAM_MEMBERS
		#undef MAKE_CLI_PROGRAM
	};

	//
	// Make info on each CLI program.
	//

	static const struct CLIProgramInfo CLI_PROGRAM_INFO[] =
		{
			#define MAKE_PROGRAM_INFO(PROGRAM_NAME, PROGRAM_DESC) \
				{ \
					.name   = STR(#PROGRAM_NAME), \
					.desc   = STR(PROGRAM_DESC), \
					.fields = \
						{ \
							CLI_PROGRAM_##PROGRAM_NAME##_FIELD_XMDT(MAKE_FIELD_INFO, PROGRAM_NAME) \
						}, \
					.field_count = CLIField_##PROGRAM_NAME##_COUNT, \
				},
			#define MAKE_FIELD_INFO(FIELD_NAME, FIELD_TYPING_NAME, FIELD_PATTERN, FIELD_DESC, PROGRAM_NAME) \
				{ \
					.offset  = offsetof(struct CLI, PROGRAM_NAME.FIELD_NAME), \
					.typing  = CLIFieldTyping_##FIELD_TYPING_NAME, \
					.pattern = STR(FIELD_PATTERN), \
					.desc    = STR(FIELD_DESC), \
				},
			CLI_PROGRAM_XMDT(MAKE_PROGRAM_INFO)
			#undef MAKE_FIELD_INFO
			#undef MAKE_PROGRAM_INFO
		};
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

#if PROGRAM_NERD
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

//
// FAT32.
//

// Arbitrary values really; mostly taken from how Windows formats the boot sector.
// With only one FAT, the reported drive capacity should be: (FAT32_TOTAL_SECTOR_COUNT - FAT32_RESERVED_SECTOR_COUNT - FAT32_TABLE_SECTOR_COUNT) * FAT32_SECTOR_SIZE.
#define FAT32_TOTAL_SECTOR_COUNT    16777216
#define FAT32_RESERVED_SECTOR_COUNT 6144
#define FAT32_TABLE_SECTOR_COUNT    1024
#define FAT32_SECTOR_SIZE_EXP       9
#define FAT32_SECTORS_PER_CLUSTER   128

#define FAT32_MEDIA_TYPE                        0xF8
#define FAT32_BACKUP_BOOT_SECTOR_OFFSET         6
#define FAT32_FILE_STRUCTURE_INFO_SECTOR_OFFSET 1
#define FAT32_ROOT_CLUSTER                      2

// Includes the reserved region, the FAT itself, and the root cluster.
#define FAT32_WIPE_SECTOR_COUNT FAT32_RESERVED_SECTOR_COUNT + FAT32_TABLE_SECTOR_COUNT + FAT32_SECTORS_PER_CLUSTER

#define FAT32_SECTOR_SIZE                      (1 << FAT32_SECTOR_SIZE_EXP)
#define FAT32_CLUSTER_ENTRIES_PER_TABLE_SECTOR (FAT32_SECTOR_SIZE / sizeof(u32))

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

union FAT32DirEntry
{
	struct FAT32DirEntryShort // See: Source(15) @ Section(6) @ Page(23).
	{
		#define FAT32_SHORT_NAME_LENGTH      8
		#define FAT32_SHORT_EXTENSION_LENGTH 3
		u8   DIR_Name[11];     // 8 characters for the main name followed by 3 for the extension where both are right-padded with spaces if necessary. Must never contain lowercase characters. If first byte is 0xE5, the entry is unallocated; 0x00 indicates the current and future entries are also unallocated.
		u8   DIR_Attr;         // Aliasing enum FAT32DirEntryAttrFlag. Must not be FAT32_DIR_ENTRY_ATTR_FLAGS_LONG.
		u8   DIR_NTRes;        // Must be zero.
		u8   DIR_CrtTimeTenth; // Irrelevant.
		u16  DIR_CrtTime;      // Irrelevant.
		u16  DIR_CrtDate;      // Irrelevant.
		u16  DIR_LstAccDate;   // irrelevant.
		u16  DIR_FstClusHI;    // "High word of first data cluster number for file/directory described by this entry".
		u16  DIR_WrtTime;      // Irrelevant.
		u16  DIR_WrtDate;      // Irrelevant.
		u16  DIR_FstClusLO;    // "Low word of first data cluster number for file/directory described by this entry".
		u32  DIR_FileSize;     // Size of file in bytes; must be zero for directories. See: Source(15) @ Section(6.2) @ Page(26).
	} short_entry;

	// Note that:
	//     - Long-name entries are reversed, that is, the first long-name entry encountered would detail the last few characters of the file name.
	//     - Characters are in UTF-16.
	//     - If there's leftover space, entries are null-terminated and then padded with 0xFFFFs.
	struct FAT32DirEntryLong // See: Source(15) @ Section(7) @ Page(30).
	{
		u8  LDIR_Ord;       // The Nth long-entry. If the entry is the "last" (as in it provides the last part of the long name), then it is additionally OR'd with FAT32_LAST_LONG_DIR_ENTRY.
		u16 LDIR_Name1[5];  // First five UTF-16 characters that this long-entry provides for the long name.
		u8  LDIR_Attr;      // Must be FAT32_DIR_ENTRY_ATTR_FLAGS_LONG.
		u8  LDIR_Type;      // Must be zero.
		u8  LDIR_Chksum;    // "Checksum of name in the associated short name directory entry at the end of the long name directory entry set". TODO Not so simple...
		u16 LDIR_Name2[6];  // The next six UTF-16 characters that this long-entry provides for the long name.
		u16 LDIR_FstClusLO; // Must be zero.
		u16 LDIR_Name3[2];  // The last two UTF-16 characters that this long-entry provides for the long name.
	} long_entry;
};

#define FAT32_DIR_ENTRY_ATTR_FLAGS_LONG (FAT32DirEntryAttrFlag_read_only | FAT32DirEntryAttrFlag_hidden | FAT32DirEntryAttrFlag_system | FAT32DirEntryAttrFlag_volume_id)
#define FAT32_LAST_LONG_DIR_ENTRY       0x40 // See: Source(15) @ Section(7) @ Page(30).
enum FAT32DirEntryAttrFlag // See: Source(15) @ Section(6) @ Page(23).
{
	FAT32DirEntryAttrFlag_read_only = 0x01,
	FAT32DirEntryAttrFlag_hidden    = 0x02,
	FAT32DirEntryAttrFlag_system    = 0x04,
	FAT32DirEntryAttrFlag_volume_id = 0x08,
	FAT32DirEntryAttrFlag_directory = 0x10,
	FAT32DirEntryAttrFlag_archive   = 0x20, // File has been modified/created; used by backup utilities.
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
			.BS_VolID       = (((u32) 'B') << 0) | (((u32) 'o') << 8) | (((u32) 'o') << 16) | (((u32) 'b') << 24),
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

	static const u32 FAT32_TABLE[FAT32_SECTOR_SIZE / sizeof(u32)] PROGMEM = // Most significant nibbles are reserved. See: Source(15) @ Section(4) @ Page(16).
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

#define MASTER_BOOT_RECORD_PARTITION_TYPE_FAT32_LBA 0x0C
struct MasterBootRecordPartition
{
	u8  status;                    // Irrelevant.
	u8  fst_sector_chs_address[3]; // Irrelevant.
	u8  partition_type;
	u8  lst_sector_chs_address[3]; // Irrelevant.
	u32 fst_sector_address;
	u32 sector_count;
};

struct MasterBootRecord // See: Source(16).
{
	u8                               bootstrap_code_area[446];
	struct MasterBootRecordPartition partitions[4];
	u8                               signature[2];  // Must be { 0x55, 0AA }.
};

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

#ifdef PIN_SD_SS
	static u8 sd_sector[FAT32_SECTOR_SIZE] = {0};
#endif

//
// "Diplomat_usb.c"
//

#define USB_MOUSE_CALIBRATIONS_REQUIRED 128

#if DEBUG // Used to disable some USB functionalities for development purposes, but does not necessairly remove all data and control flow.
	#define USB_CDC_ENABLE true
	#define USB_HID_ENABLE true
	#define USB_MS_ENABLE  true
#else
	#define USB_CDC_ENABLE false
	#define USB_HID_ENABLE true
	#define USB_MS_ENABLE  true
#endif

enum USBMSOCRState
{
	USBMSOCRState_ready,      // Main program can set the OCR state.
	USBMSOCRState_set,        // Main program has finished setting the OCR state and OCR now waits for BMP.
	USBMSOCRState_processing, // OCR is currently processing BMP.
};

struct USBMSOCRMaskStreamState
{
	u16 index_u8;
	u16 index_u16;
	u16 runlength_remaining;
};

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
	#define MAKE(BM_REQUEST_TYPE, B_REQUEST) (((u16) (B_REQUEST) << 8) | (BM_REQUEST_TYPE))

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
	USBHIDItemMainInputFlag_padding  = ((u16) 1) << 0, // Also called "constant", but it's essentially used for padding.
	USBHIDItemMainInputFlag_variable = ((u16) 1) << 1, // Data is just a simple bitmap/integer; otherwise, the data is a fixed-sized buffer that can be filled up.
	USBHIDItemMainInputFlag_relative = ((u16) 1) << 2, // Data is a delta.
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
	USBMSSCSIOpcode_read_capacity = 0x25, // 10-byte version.
	USBMSSCSIOpcode_read          = 0x28, // 10-byte version.
	USBMSSCSIOpcode_write         = 0x2A, // 10-byte version.
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
				(((u32) FAT32_TOTAL_SECTOR_COUNT - 1) >> 24) & 0xFF,
				(((u32) FAT32_TOTAL_SECTOR_COUNT - 1) >> 16) & 0xFF,
				(((u32) FAT32_TOTAL_SECTOR_COUNT - 1) >>  8) & 0xFF,
				(((u32) FAT32_TOTAL_SECTOR_COUNT - 1) >>  0) & 0xFF,

			// "BLOCK LENGTH IN BYTES" : Big-endian size of sectors.
				(((u32) FAT32_SECTOR_SIZE) >> 24) & 0xFF,
				(((u32) FAT32_SECTOR_SIZE) >> 16) & 0xFF,
				(((u32) FAT32_SECTOR_SIZE) >>  8) & 0xFF,
				(((u32) FAT32_SECTOR_SIZE) >>  0) & 0xFF,
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
	static_assert(countof(_usb_mouse_command_buffer) < (((u64) 1) << bitsof(_usb_mouse_command_reader)));
	static_assert(countof(_usb_mouse_command_buffer) < (((u64) 1) << bitsof(_usb_mouse_command_writer)));

	// Buffer sizes must be a power of two for the "_usb_mouse_X_masked" macros.
	static_assert(countof(_usb_mouse_command_buffer) && !(countof(_usb_mouse_command_buffer) & (countof(_usb_mouse_command_buffer) - 1)));

	#define MAKE(IDENTIFIER_NAME, PRINT_NAME, LANGUAGE, MAX_WORD_LENGTH, SENTINEL_LETTER, POS_X, POS_Y, DIM_SLOTS_X, DIM_SLOTS_Y, SLOT_DIM, UNCOMPRESSED_SLOT_STRIDE, COMPRESSED_SLOT_STRIDE, ...) \
		static_assert(COMPRESSED_SLOT_STRIDE < 256); // To ensure _usb_ms_ocr_slot_topdown_pixel_coords stays byte-sized.
	WORDGAME_XMDT(MAKE,)
	#undef MAKE

	static volatile enum USBMSOCRState     usb_ms_ocr_state                                                      = {0};
	static u8_2                           _usb_ms_ocr_slot_topdown_board_coords                                  = {0};
	static u8_2                           _usb_ms_ocr_slot_topdown_pixel_coords                                  = {0};
	static u16                            _usb_ms_ocr_accumulated_scores[WORDGAME_MAX_DIM_SLOTS_X][Letter_COUNT] = {0};
	static u8                             _usb_ms_ocr_packed_slot_pixels[MASK_DIM / 8]                           = {0};
	static u8                             _usb_ms_ocr_packed_activated_slots                                     = 0;
	static struct USBMSOCRMaskStreamState _usb_ms_ocr_curr_mask_stream_state                                     = {0};
	static struct USBMSOCRMaskStreamState _usb_ms_ocr_next_mask_stream_state                                     = {0};
	static_assert(WORDGAME_MAX_DIM_SLOTS_X == 8); // For _usb_ms_ocr_packed_activated_slots.
	static_assert(MASK_DIM % 8 == 0);             // For _usb_ms_ocr_packed_slot_pixels.

	#if USB_MS_ENABLE
		static struct USBMSCommandStatusWrapper _usb_ms_status      = { .dCSWSignature = USB_MS_COMMAND_STATUS_WRAPPER_SIGNATURE };
		static b8                               _usb_ms_send_status = false;
	#endif

	#if USB_CDC_ENABLE && DEBUG
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
		static_assert(countof(debug_usb_cdc_in_buffer ) <= (((u64) 1) << bitsof(debug_usb_cdc_in_reader )));
		static_assert(countof(debug_usb_cdc_in_buffer ) <= (((u64) 1) << bitsof(debug_usb_cdc_in_writer )));
		static_assert(countof(debug_usb_cdc_out_buffer) <= (((u64) 1) << bitsof(debug_usb_cdc_out_reader)));
		static_assert(countof(debug_usb_cdc_out_buffer) <= (((u64) 1) << bitsof(debug_usb_cdc_out_writer)));

		// Buffer sizes must be a power of two for the "debug_usb_cdc_X_Y_masked" macros.
		static_assert(countof(debug_usb_cdc_in_buffer ) && !(countof(debug_usb_cdc_in_buffer ) & (countof(debug_usb_cdc_in_buffer ) - 1)));
		static_assert(countof(debug_usb_cdc_out_buffer) && !(countof(debug_usb_cdc_out_buffer) & (countof(debug_usb_cdc_out_buffer) - 1)));

		static b8 debug_usb_is_on_host_machine = false;
	#endif
#endif

//
// Diplomat and Nerd.
//

#define NERD_COMMAND_SUBMIT_BIT 0b1'000'0000
#define NERD_COMMAND_COMPLETE   0b1'111'1111
#define NERD_COMMAND_X(COMMAND) ((COMMAND >> 4) & 0b0000'0111)
#define NERD_COMMAND_Y(COMMAND) ( COMMAND       & 0b0000'1111)

struct DiplomatPacket
{
	enum WordGame wordgame;
	enum Letter   board[WORDGAME_MAX_DIM_SLOTS_Y][WORDGAME_MAX_DIM_SLOTS_X];
};

#if PROGRAM_DIPLOMAT
	static volatile struct DiplomatPacket diplomat_packet = {0};
#endif

//
// "Nerd.c".
//

#define ALPHABET_INDEX_TAKEN (1 << 7)
static_assert(BITS_PER_ALPHABET_INDEX < 8); // For ALPHABET_INDEX_TAKEN.

enum WordBitesPieceOrientation
{
	WordBitesPieceOrientation_none,
	WordBitesPieceOrientation_hort,
	WordBitesPieceOrientation_vert,
};

struct WordBitesPiece
{
	enum WordBitesPieceOrientation orientation;
	u8_2                           position;
	u8                             alphabet_indices[2]; // Singleton pieces will have the same single alphabet index in both entries.
};

#if PROGRAM_NERD
	static u8                    board_alphabet_indices[WORDGAME_MAX_DIM_SLOTS][WORDGAME_MAX_DIM_SLOTS] = {0};
	static u8_2                  board_dim_slots      = {0};
	static struct WordBitesPiece wordbites_pieces[11] = {0};
	static u8                    command_buffer[256]  = {0};
	static u8                    command_reader       = 0;
	static u8                    command_writer       = 0;
	static_assert(sizeof(command_buffer) == 256); // For the byte-sized indices.
	static_assert(sizeof(command_reader) == 1);   // Wrapping behavior needed.
	static_assert(sizeof(command_writer) == 1);   // Wrapping behavior needed.
#endif

//
// "Diplomat.c".
//

enum Menu
{
	Menu_main,
	Menu_chosen_map,
	Menu_displaying,
};

#define MENU_CHOSEN_MAP_OPTION_XMDT(X) \
	X(back     , "Back"       ) \
	X(auto_play, "Auto play"  ) \
	X(on_guard , "Be on guard") \
	X(datamine , "Datamine"   )

#define MENU_CHOSEN_MAP_OPTION_MAX_MESSAGE_SIZE_(NAME, MESSAGE) u8 NAME[sizeof(MESSAGE)];
#define MENU_CHOSEN_MAP_OPTION_MAX_MESSAGE_SIZE sizeof(union { MENU_CHOSEN_MAP_OPTION_XMDT(MENU_CHOSEN_MAP_OPTION_MAX_MESSAGE_SIZE_) })

enum MenuMainOption
{
	// ... enum WordGame ...

	MenuMainOption_reset_filesystem = WordGame_COUNT,
	MenuMainOption_COUNT,
};

enum MenuChosenMapOption
{
	#define MAKE(NAME, ...) MenuChosenMapOption_##NAME,
	MENU_CHOSEN_MAP_OPTION_XMDT(MAKE)
	#undef MAKE
	MenuChosenMapOption_COUNT,
};

#if PROGRAM_DIPLOMAT
	static const char MENU_CHOSEN_MAP_OPTIONS[MenuChosenMapOption_COUNT][MENU_CHOSEN_MAP_OPTION_MAX_MESSAGE_SIZE] PROGMEM =
		{
			#define MAKE(NAME, MESSAGE) MESSAGE,
			MENU_CHOSEN_MAP_OPTION_XMDT(MAKE)
			#undef MAKE
		};
#endif

//
// Documentation.
//

/* [Overview].
	Source(1)  := ATmega16U4/ATmega32U4 Datasheet (Dated: 2016).
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
	Source(23) := PDF Article of Microsoft's "Open Specifications" for Protocols (Dated: 08/01/2023).
	Source(24) := Wikipedia's "BMP file format" Page (Last Edited: 23 October 2022, at 13:30 (UTC)).
	Source(25) := HD44780U (LCD-II) (Dot Matrix Liquid Crystal Display Controller/Driver) by HITACHI (Dated: 1998).
	Source(26) := MAX7219/MAX7221 Serially Interfaced, 8-Digit LED Display Drivers by Maxim Integrated (Dated: 8/21).
	Source(27) := Atmel ATmega640/V-1280/V-1281/V-2560/V-2561/V Datasheet (Dated: 2014).
	Source(28) := "Partition type" on Wikipedia (Accessed: December 6, 2023).

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
