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

static inline V2F v2f_lerp(V2F l, V2F r, F32 t)
{
  return HMM_LerpV2(l, t, r);
}

static inline V3F v3f_lerp(V3F l, V3F r, F32 t)
{
  return HMM_LerpV3(l, t, r);
}

static inline V4F v4f_lerp(V4F l, V4F r, F32 t)
{
  return HMM_LerpV4(l, t, r);
}

static inline V2F v2f_smoothstep(V2F l, V2F r, F32 t)
{
  V2F out;
  out.x = f32_smoothstep(l.x, r.x, t);
  out.y = f32_smoothstep(l.y, r.y, t);
  return out;
}

static inline V3F v3f_smoothstep(V3F l, V3F r, F32 t)
{
  V3F out;
  out.x = f32_smoothstep(l.x, r.x, t);
  out.y = f32_smoothstep(l.y, r.y, t);
  out.z = f32_smoothstep(l.z, r.z, t);
  return out;
}

static inline V4F v4f_smoothstep(V4F l, V4F r, F32 t)
{
  V4F out;
  out.x = f32_smoothstep(l.x, r.x, t);
  out.y = f32_smoothstep(l.y, r.y, t);
  out.z = f32_smoothstep(l.z, r.z, t);
  out.w = f32_smoothstep(l.w, r.w, t);
  return out;
}
