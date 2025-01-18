/*******************************************************************************
 * view.h - model of user interface
 ******************************************************************************/

#pragma once

#include "prelude.h"
#include "linear_algebra.h"

typedef struct View {

  V2S cursor;

  // measured in tiles
  V2F camera;

} View;
