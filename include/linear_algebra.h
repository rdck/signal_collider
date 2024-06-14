/*******************************************************************************
 * linear_algebra.h - adapter for Handmade Math library
 ******************************************************************************/

#pragma once
#include "prelude.h"
#include "HandmadeMath.h"

typedef HMM_Vec2 V2F;
typedef HMM_Vec3 V3F;
typedef HMM_Vec4 V4F;

typedef HMM_Mat2 M2F;
typedef HMM_Mat3 M3F;
typedef HMM_Mat4 M4F;

#define f32_rad_of_deg HMM_ToRad
#define f32_deg_of_rad HMM_ToDeg

#define f32_sin HMM_SinF
#define f32_cos HMM_CosF
#define f32_tan HMM_TanF

#define f32_sqrt HMM_SqrtF
#define f32_inv_sqrt HMM_InvSqrtF

#define v2f HMM_V2
#define v3f HMM_V3
#define v4f HMM_V4

#define v4f_of_v3f_f HMM_V4V

#define v2f_add HMM_AddV2
#define v3f_add HMM_AddV3
#define v4f_add HMM_AddV4

#define v2f_sub HMM_SubV2
#define v3f_sub HMM_SubV3
#define v4f_sub HMM_SubV4

#define v2f_mul HMM_MulV2
#define v3f_mul HMM_MulV3
#define v4f_mul HMM_MulV4

#define v2f_scale HMM_MulV2F
#define v3f_scale HMM_MulV3F
#define v4f_scale HMM_MulV4F

#define v2f_div HMM_DivV2
#define v3f_div HMM_DivV3
#define v4f_div HMM_DivV4

#define v2f_inv_scale HMM_DivV2F
#define v3f_inv_scale HMM_DivV3F
#define v4f_inv_scale HMM_DivV4F

#define v2f_eq HMM_EqV2
#define v3f_eq HMM_EqV3
#define v4f_eq HMM_EqV4

#define v2f_dot HMM_DotV2
#define v3f_dot HMM_DotV3
#define v4f_dot HMM_DotV4

#define v3f_cross HMM_Cross

#define v2f_length_squared HMM_LenSqrV2
#define v3f_length_squared HMM_LenSqrV3
#define v4f_length_squared HMM_LenSqrV4

#define v2f_length HMM_LenV2
#define v3f_length HMM_LenV3
#define v4f_length HMM_LenV4

#define v2f_normalize HMM_NormV2
#define v3f_normalize HMM_NormV3
#define v4f_normalize HMM_NormV4

#define m2f_add HMM_AddM2
#define m3f_add HMM_AddM3
#define m4f_add HMM_AddM4

#define m2f_sub HMM_SubM2
#define m3f_sub HMM_SubM3
#define m4f_sub HMM_SubM4

#define m2f_diagonal HMM_M2D
#define m3f_diagonal HMM_M3D
#define m4f_diagonal HMM_M4D

#define m2f_transpose HMM_TransposeM2
#define m3f_transpose HMM_TransposeM3
#define m4f_transpose HMM_TransposeM4

#define m2f_mul_v2f HMM_MulM2V2
#define m3f_mul_v3f HMM_MulM3V3
#define m4f_mul_v4f HMM_MulM4V4

#define m2f_mul HMM_MulM2
#define m3f_mul HMM_MulM3
#define m4f_mul HMM_MulM4

#define m2f_scale HMM_MulM2F
#define m3f_scale HMM_MulM3F
#define m4f_scale HMM_MulM4F

#define m2f_inv_scale HMM_DivM2F
#define m3f_inv_scale HMM_DivM3F
#define m4f_inv_scale HMM_DivM4F

#define m2f_determinant HMM_DeterminantM2
#define m3f_determinant HMM_DeterminantM3
#define m4f_determinant HMM_DeterminantM4

#define m2f_inverse HMM_InvGeneralM2
#define m3f_inverse HMM_InvGeneralM3
#define m4f_inverse HMM_InvGeneralM4

static inline F32 f32_lerp(F32 a, F32 b, F32 t)
{
  return (1.f - t) * a + t * b;
}

static inline F32 f32_smoothstep(F32 a, F32 b, F32 t)
{
  const F32 v = (3 * t * t) - (2 * t * t * t);
  return f32_lerp(a, b, v);
}

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

static inline V4F v4f_of_v4s(V4S v)
{
  V4F out;
  out.x = (F32) v.x;
  out.y = (F32) v.y;
  out.z = (F32) v.z;
  out.w = (F32) v.w;
  return out;
}

static inline V3F v3f_of_v3s(V3S v)
{
  V3F out;
  out.x = (F32) v.x;
  out.y = (F32) v.y;
  out.z = (F32) v.z;
  return out;
}

static inline V2F v2f_of_v2s(V2S v)
{
  V2F out;
  out.x = (F32) v.x;
  out.y = (F32) v.y;
  return out;
}
