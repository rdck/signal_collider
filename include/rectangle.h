#pragma once

#include "linear_algebra.h"

typedef struct R2S {
  V2S origin;
  V2S size;
} R2S;

typedef struct R2F {
  V2F origin;
  V2F size;
} R2F;

static inline Bool v2f_in_r2f(V2F v, R2F r)
{
  const Bool x = v.x >= r.origin.x && v.x < r.origin.x + r.size.x;
  const Bool y = v.y >= r.origin.y && v.y < r.origin.y + r.size.y;
  return x && y;
}
