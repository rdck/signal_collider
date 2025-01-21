/*******************************************************************************
 * view.h - model of user interface
 ******************************************************************************/

#pragma once

#include "prelude.h"
#include "linear_algebra.h"

typedef struct View {
  V2S cursor;
  V2F camera;
} View;
