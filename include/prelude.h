/*******************************************************************************
 * prelude.h - core types and macros
 ******************************************************************************/

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

/*******************************************************************************
 * TYPE DEFINITIONS
 ******************************************************************************/

typedef uint8_t         U8;
typedef uint16_t        U16;
typedef uint32_t        U32;
typedef uint64_t        U64;
typedef int8_t          S8;
typedef int16_t         S16;
typedef int32_t         S32;
typedef int64_t         S64;
typedef float           F32;
typedef double          F64;

typedef unsigned char   Byte;
typedef char            Char;
typedef bool            Bool;
typedef void            Void;
typedef size_t          Size;

typedef Void* VoidPointer;
typedef ptrdiff_t Index;

/*******************************************************************************
 * COMMON MACROS
 ******************************************************************************/

#define KIBI 0x400
#define MEBI 0x100000
#define GIBI 0x40000000

#define KILO 1000
#define MEGA 1000000
#define GIGA 1000000000

#define INDEX_NONE (-1)

#define CAT_INTERNAL(a, ...) a ## __VA_ARGS__
#define CAT(a, ...) CAT_INTERNAL(a, __VA_ARGS__)

#define ASSERT assert
#define UNUSED_PARAMETER(name) ((Void) (name))

#define CONTAINER_OF_UNCHECKED(ptr, type, member) \
  ((type *) ((char *)(ptr) - offsetof(type, member)))

#ifdef NDEBUG
#define CONTAINER_OF CONTAINER_OF_UNCHECKED
#else
#define CONTAINER_OF(ptr, type, member) \
  (ptr ? CONTAINER_OF_UNCHECKED(ptr, type, member) : NULL)
#endif

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define CLAMP(min, max, x) (MAX((min), MIN((max), (x))))
#define ABS(a) ((a) < 0 ? -(a) : (a))

// division rounding towards -inf when d < 0
#define KNUTH_DIV(n, d) ((n) >= 0 ? (n) / (d) : ((n) - (d) + 1) / (d))
