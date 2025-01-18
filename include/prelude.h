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

// unsigned integer types
typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;

// signed integer types
typedef int8_t S8;
typedef int16_t S16;
typedef int32_t S32;
typedef int64_t S64;

// floating point types
typedef float F32;
typedef double F64;

// single byte types
typedef unsigned char Byte;
typedef char Char;

// booleans
typedef bool Bool;

// void
typedef void Void;

// size types
typedef size_t Size;
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

#define STEREO 2
#define OCTAVE 12

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

// division rounding towards negative infinity when d < 0
#define KNUTH_DIV(n, d) ((n) >= 0 ? (n) / (d) : ((n) - (d) + 1) / (d))

/*******************************************************************************
 * INTEGER VECTORS
 ******************************************************************************/

typedef union V2B {

  struct { Bool x, y; };
  struct { Bool l, r; };

  Bool elements[2];

} V2B;

typedef union V2S {

  struct { S32 x, y; };
  struct { S32 w, h; };
  struct { S32 u, v; };
  struct { S32 l, r; };

  S32 elements[2];

} V2S;

typedef union V3S {

  struct { S32 x, y, z; };
  struct { S32 u, v, w; };
  struct { S32 r, g, b; };

  struct { V2S xy; S32 _ignored_xy; };
  struct { V2S yz; S32 _ignored_yz; };
  struct { V2S uv; S32 _ignored_uv; };
  struct { V2S vw; S32 _ignored_vw; };

  S32 elements[3];

} V3S;

typedef union V4S {

  struct { S32 x, y, z, w; };
  struct { S32 r, g, b, a; };

  struct { V3S xyz; S32 _ignored_xyz; };
  struct { S32 _ignored_yzw; V3S yzw; };

  struct { V2S xy; S32 _ignored_xy_z; S32 _ignored_xy_w; };
  struct { S32 _ignored_yz_x; V2S yz; S32 _ignored_yz_w; };
  struct { S32 _ignored_zw_x; S32 _ignored_zw_y; V2S zw; };

  S32 elements[4];

} V4S;

static inline V2S v2s(S32 x, S32 y)
{
  V2S out;
  out.x = x;
  out.y = y;
  return out;
}

static inline V3S v3s(S32 x, S32 y, S32 z)
{
  V3S out;
  out.x = x;
  out.y = y;
  out.z = z;
  return out;
}

static inline V4S v4s(S32 x, S32 y, S32 z, S32 w)
{
  V4S out;
  out.x = x;
  out.y = y;
  out.z = z;
  out.w = w;
  return out;
}

static inline V2S v2s_add(V2S left, V2S right)
{
  V2S out;
  out.x = left.x + right.x;
  out.y = left.y + right.y;
  return out;
}

static inline V3S v3s_add(V3S left, V3S right)
{
  V3S out;
  out.x = left.x + right.x;
  out.y = left.y + right.y;
  out.z = left.z + right.z;
  return out;
}

static inline V4S v4s_add(V4S left, V4S right)
{
  V4S out;
  out.x = left.x + right.x;
  out.y = left.y + right.y;
  out.z = left.z + right.z;
  out.w = left.w + right.w;
  return out;
}

static inline V2S v2s_sub(V2S left, V2S right)
{
  V2S out;
  out.x = left.x - right.x;
  out.y = left.y - right.y;
  return out;
}

static inline V3S v3s_sub(V3S left, V3S right)
{
  V3S out;
  out.x = left.x - right.x;
  out.y = left.y - right.y;
  out.z = left.z - right.z;
  return out;
}

static inline V4S v4s_sub(V4S left, V4S right)
{
  V4S out;
  out.x = left.x - right.x;
  out.y = left.y - right.y;
  out.z = left.z - right.z;
  out.w = left.w - right.w;
  return out;
}

static inline S32 v2s_manhattan(V2S left, V2S right)
{
  const S32 x = ABS(left.x - right.x);
  const S32 y = ABS(left.y - right.y);
  return x + y;
}

static inline S32 v3s_manhattan(V3S left, V3S right)
{
  const S32 x = ABS(left.x - right.x);
  const S32 y = ABS(left.y - right.y);
  const S32 z = ABS(left.z - right.z);
  return x + y + z;
}

static inline S32 v4s_manhattan(V4S left, V4S right)
{
  const S32 x = ABS(left.x - right.x);
  const S32 y = ABS(left.y - right.y);
  const S32 z = ABS(left.z - right.z);
  const S32 w = ABS(left.w - right.w);
  return x + y + z + w;
}

static inline V2S v2s_mul(V2S left, V2S right)
{
  V2S out;
  out.x = left.x * right.x;
  out.y = left.y * right.y;
  return out;
}

static inline V3S v3s_mul(V3S left, V3S right)
{
  V3S out;
  out.x = left.x * right.x;
  out.y = left.y * right.y;
  out.z = left.z * right.z;
  return out;
}

static inline V4S v4s_mul(V4S left, V4S right)
{
  V4S out;
  out.x = left.x * right.x;
  out.y = left.y * right.y;
  out.z = left.z * right.z;
  out.w = left.w * right.w;
  return out;
}

static inline V2S v2s_div(V2S left, V2S right)
{
  V2S out;
  out.x = left.x / right.x;
  out.y = left.y / right.y;
  return out;
}

static inline V3S v3s_div(V3S left, V3S right)
{
  V3S out;
  out.x = left.x / right.x;
  out.y = left.y / right.y;
  out.z = left.z / right.z;
  return out;
}

static inline V4S v4s_div(V4S left, V4S right)
{
  V4S out;
  out.x = left.x / right.x;
  out.y = left.y / right.y;
  out.z = left.z / right.z;
  out.w = left.w / right.w;
  return out;
}

static inline V2S v2s_knuth_div(V2S left, V2S right)
{
  V2S out;
  out.x = KNUTH_DIV(left.x, right.x);
  out.y = KNUTH_DIV(left.y, right.y);
  return out;
}

static inline V3S v3s_knuth_div(V3S left, V3S right)
{
  V3S out;
  out.x = KNUTH_DIV(left.x, right.x);
  out.y = KNUTH_DIV(left.y, right.y);
  out.z = KNUTH_DIV(left.z, right.z);
  return out;
}

static inline V4S v4s_knuth_div(V4S left, V4S right)
{
  V4S out;
  out.x = KNUTH_DIV(left.x, right.x);
  out.y = KNUTH_DIV(left.y, right.y);
  out.z = KNUTH_DIV(left.z, right.z);
  out.w = KNUTH_DIV(left.w, right.w);
  return out;
}

static inline V2S v2s_mod(V2S left, V2S right)
{
  V2S out;
  out.x = left.x % right.x;
  out.y = left.y % right.y;
  return out;
}

static inline V3S v3s_mod(V3S left, V3S right)
{
  V3S out;
  out.x = left.x % right.x;
  out.y = left.y % right.y;
  out.z = left.z % right.z;
  return out;
}

static inline V4S v4s_mod(V4S left, V4S right)
{
  V4S out;
  out.x = left.x % right.x;
  out.y = left.y % right.y;
  out.z = left.z % right.z;
  out.w = left.w % right.w;
  return out;
}

static inline V2S v2s_scale(V2S v, S32 scalar)
{
  V2S out;
  out.x = v.x * scalar;
  out.y = v.y * scalar;
  return out;
}

static inline V3S v3s_scale(V3S v, S32 scalar)
{
  V3S out;
  out.x = v.x * scalar;
  out.y = v.y * scalar;
  out.z = v.z * scalar;
  return out;
}

static inline V4S v4s_scale(V4S v, S32 scalar)
{
  V4S out;
  out.x = v.x * scalar;
  out.y = v.y * scalar;
  out.z = v.z * scalar;
  out.w = v.w * scalar;
  return out;
}

static inline V2S v2s_inv_scale(V2S v, S32 scalar)
{
  V2S out;
  out.x = v.x / scalar;
  out.y = v.y / scalar;
  return out;
}

static inline V3S v3s_inv_scale(V3S v, S32 scalar)
{
  V3S out;
  out.x = v.x / scalar;
  out.y = v.y / scalar;
  out.z = v.z / scalar;
  return out;
}

static inline V4S v4s_inv_scale(V4S v, S32 scalar)
{
  V4S out;
  out.x = v.x / scalar;
  out.y = v.y / scalar;
  out.z = v.z / scalar;
  out.w = v.w / scalar;
  return out;
}

static inline Bool v4s_equal(V4S left, V4S right)
{
  return
    (left.x == right.x) &&
    (left.y == right.y) &&
    (left.z == right.z) &&
    (left.w == right.w) ;
}

static inline Bool v3s_equal(V3S left, V3S right)
{
  return
    (left.x == right.x) &&
    (left.y == right.y) &&
    (left.z == right.z) ;
}

static inline Bool v2s_equal(V2S left, V2S right)
{
  return
    (left.x == right.x) &&
    (left.y == right.y) ;
}

static inline F32 f32_lerp(F32 a, F32 b, F32 t)
{
  return (1.f - t) * a + t * b;
}

static inline F32 f32_smoothstep(F32 a, F32 b, F32 t)
{
  const F32 time = (3 * t * t) - (2 * t * t * t);
  return f32_lerp(a, b, time);
}

static inline S32 s32_lerp(S32 a, S32 b, F32 t)
{
  const F32 value = (1.f - t) * a + t * b;
  return (S32) value;
}

static inline S32 s32_smoothstep(S32 a, S32 b, F32 t)
{
  const F32 time = (3 * t * t) - (2 * t * t * t);
  return s32_lerp(a, b, time);
}

static inline U8 u8_lerp(U8 a, U8 b, F32 t)
{
  const F32 value = (1.f - t) * a + t * b;
  return (U8) value;
}

static inline U8 u8_smoothstep(U8 a, U8 b, F32 t)
{
  const F32 time = (3 * t * t) - (2 * t * t * t);
  return u8_lerp(a, b, time);
}
