/*******************************************************************************
 * display.h - abstraction of platform rendering interface
 ******************************************************************************/

#pragma once
#include "linear_algebra.h"

#define COLOR_WHITE     0xFFFFFFFF
#define COLOR_BLACK     0xFF000000
#define COLOR_RED       0xFF0000FF
#define COLOR_GREEN     0xFF00FF00
#define COLOR_BLUE      0xFFFF0000

typedef U32 TextureID;

typedef struct {
  V2S size;
  Byte* atlas;
} Font;

typedef struct {
  TextureID texture;
  V2F ta;
  V2F tb;
  U32 color;
  F32 depth;
  V2F root;
  V2F size;
} Sprite;

typedef struct Color32 {
  U8 a;
  U8 g;
  U8 b;
  U8 r;
} Color32;

Void display_init(V2S window, V2S render);
Void display_begin_frame();
Void display_end_frame();
TextureID display_load_image(const Byte* image, V2S dimensions);
Void display_sprite(Sprite s);
U32 display_color_lerp(U32 a, U32 b, F32 t);
