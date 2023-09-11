#define false                   0
#define true                    1
#define stringify_(X)           #X
#define concat_(X, Y)           X##Y
#define countof(...)            (sizeof(__VA_ARGS__)/sizeof((__VA_ARGS__)[0]))
#define countof_m(TYPE, MEMBER) (sizeof(((TYPE*) 0)->MEMBER)/sizeof(((TYPE*) 0)->MEMBER[0]))
#define stringify(X)            stringify_(X)
#define concat(X, Y)            concat_(X, Y)
#define static_assert(X)        _Static_assert((X), #X);

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  b8;
typedef uint16_t b16;
typedef uint32_t b32;
typedef uint64_t b64;
