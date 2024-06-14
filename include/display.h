/*******************************************************************************
 * DISPLAY INTERFACE
 ******************************************************************************/

#pragma once
#include "linear_algebra.h"

#define Display_COLOR_WHITE     0xFFFFFFFF
#define Display_COLOR_BLACK     0xFF000000
#define Display_COLOR_RED       0xFF0000FF
#define Display_COLOR_GREEN     0xFF00FF00
#define Display_COLOR_BLUE      0xFFFF0000

typedef U32 Display_TextureID;

typedef struct {
  V2S size;
  Byte* atlas;
} Display_Font;

typedef struct {
  Display_TextureID texture;
  V2F ta;
  V2F tb;
  U32 color;
  F32 depth;
  V2F root;
  V2F size;
} Display_Sprite;

typedef struct Display_Color {
  U8 a;
  U8 g;
  U8 b;
  U8 r;
} Display_Color;

Void Display_init(V2S window, V2S render);
Void Display_begin_frame();
Void Display_end_frame();
Display_TextureID Display_load_image(const Byte* image, V2S dimensions);
Void Display_sprite(Display_Sprite s);
U32 Display_color_lerp(U32 a, U32 b, F32 t);
