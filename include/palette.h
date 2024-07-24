#pragma once

#include "prelude.h"

#define PALETTE_SOUNDS 36

typedef struct PaletteSound {
  const Char* path;     // relative path
  Index frames;         // total number of audio frames
  F32* interleaved;     // stereo sample data
} PaletteSound;

typedef struct Palette {
  PaletteSound sounds[PALETTE_SOUNDS];
} Palette;
