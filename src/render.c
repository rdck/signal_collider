#include "render.h"
#include "display.h"
#include "sim.h"
#include "font.ttf.h"
#include "stb_truetype.h"

#define Render_ASCII_X 16
#define Render_ASCII_Y 8
#define Render_EMPTY_CHARACTER '.'

#define Render_COLOR_LITERAL    0xFFFF8080
#define Render_COLOR_OPERATOR   0xFFFFFFFF
#define Render_COLOR_EMPTY      0x80FFFFFF

static V2S Render_dimensions = {0};

static Display_TextureID Render_white = 0;
static Display_TextureID Render_font_atlas = 0;

static Char Render_char_table[Model_VALUE_CARDINAL] = {
  [ Model_VALUE_LITERAL       ] = 0,
  [ Model_VALUE_BANG          ] = '*',
  [ Model_VALUE_IF            ] = '=',
  [ Model_VALUE_CLOCK         ] = 'C',
  [ Model_VALUE_DELAY         ] = 'D',
  [ Model_VALUE_RANDOM        ] = 'R',
  [ Model_VALUE_ADD           ] = '+',
  [ Model_VALUE_SUB           ] = '-',
  [ Model_VALUE_MUL           ] = '*',
  [ Model_VALUE_GENERATE      ] = '!',
  [ Model_VALUE_SCALE         ] = '#',
  [ Model_VALUE_SYNTH         ] = '~',
};

static V2S Render_tile_size(V2S canvas)
{
  return v2s_div(canvas, v2s(Model_X, Model_Y));
}

static V2S Render_atlas_coordinate(Char c)
{
  V2S out;
  out.x = c % Render_ASCII_X;
  out.y = c / Render_ASCII_X;
  return out;
}

static Void Render_load_font(S32 size)
{

  stbtt_fontinfo font = {0};
  const S32 init_result = stbtt_InitFont(&font, font_Hack_Regular_ttf, 0);
  ASSERT(init_result);

  const F32 scale = stbtt_ScaleForPixelHeight(&font, (F32) size);

  S32 ascent = 0;
  S32 descent = 0;
  S32 line_gap = 0;
  stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);

#if 0
  S32 advance = 0;
  S32 left_side_bearing = 0;
  stbtt_GetCodepointHMetrics(&font, 'M', &advance, &left_side_bearing);
#endif

  const S32 char_area = size * size;
  Byte* const atlas = malloc(Render_ASCII_X * Render_ASCII_Y * char_area);
  memset(atlas, 0, Render_ASCII_X * Render_ASCII_Y * char_area);

  S32 w = 0;
  S32 h = 0;
  S32 xoff = 0;
  S32 yoff = 0;
  for (Char c = '!'; c <= '~'; c++) {

    Byte* const bitmap = stbtt_GetCodepointBitmap(
        &font,
        scale, scale,
        c,
        &w, &h,
        &xoff, &yoff
        );
    ASSERT(w <= size);
    ASSERT(h <= size);

    const V2S p = {
      c % Render_ASCII_X,
      c / Render_ASCII_X,
    };
    const S32 descent_pixels = (S32) (scale * descent);
    for (S32 y = 0; y < h; y++) {
      for (S32 x = 0; x < w; x++) {
        const S32 x0 = p.x * size + xoff + x;
        const S32 y0 = p.y * size + yoff + y + descent_pixels;
        const Bool xr = x0 < Render_ASCII_X * size;
        const Bool yr = y0 < Render_ASCII_Y * size;
        if (xr && yr) {
          // const Index yy = y0 * (Render_ASCII_X * size);
          ASSERT(y0 < Render_ASCII_Y * size);
          ASSERT(x0 < Render_ASCII_X * size);
          atlas[y0 * (Render_ASCII_X * size) + x0] = bitmap[y * w + x];
        }
      }
    }

    stbtt_FreeBitmap(bitmap, NULL);

  }

#ifdef Render_ATLAS_LINES
  for (Index y = 0; y < Render_ASCII_Y; y++) {
    for (Index x = 0; x < Render_ASCII_X * size; x++) {
      const Index y0 = y * size;
      const Index stride = Render_ASCII_X * size;
      atlas[y0 * stride + x] = 0xFF;
    }
  }
#endif

#ifdef Render_WRITE_ATLAS
  stbi_write_png(
      "font_atlas.png",
      Render_ASCII_X * size,
      Render_ASCII_Y * size,
      1,
      atlas,
      Render_ASCII_X * size
      );
#endif

  Byte* const channels = malloc(4 * Render_ASCII_X * size * Render_ASCII_Y * size);
  const Index stride = Render_ASCII_X * size;
  for (Index y = 0; y < Render_ASCII_Y * size; y++) {
    for (Index x = 0; x < Render_ASCII_X * size; x++) {
      const Byte value = atlas[y * stride + x];
      channels[4 * (y * stride + x) + 0] = 0xFF;
      channels[4 * (y * stride + x) + 1] = 0xFF;
      channels[4 * (y * stride + x) + 2] = 0xFF;
      channels[4 * (y * stride + x) + 3] = value;
    }
  }

  const V2S dimensions = { Render_ASCII_X * size, Render_ASCII_Y * size };
  Render_font_atlas = Display_load_image(channels, dimensions);

  free(channels);
  free(atlas);

}

static Void Render_draw_grid_character(V2S point, Char c, U32 color)
{

  const V2S p     = Render_atlas_coordinate(c);
  const V2S tile  = Render_tile_size(Render_dimensions);

  // @rdk: calculate glyph offset properly
  const S32 glyph_offset = Render_dimensions.x / (Model_X * 4);

  Display_Sprite s;
  s.ta.x = (p.x + 0) / (F32) Render_ASCII_X;
  s.ta.y = (p.y - 1) / (F32) Render_ASCII_Y;
  s.tb.x = (p.x + 1) / (F32) Render_ASCII_X;
  s.tb.y = (p.y + 0) / (F32) Render_ASCII_Y;
  s.texture = Render_font_atlas;
  s.color = color;
  s.depth = 0.f;
  s.root.x = (F32) point.x * tile.x + glyph_offset;
  s.root.y = (F32) point.y * tile.y;
  s.size = v2f_of_v2s(tile);
  Display_sprite(s);

}

static Void Render_draw_grid_highlight(V2S point, U32 color, F32 depth)
{
  const V2S tile = Render_tile_size(Render_dimensions);
  Display_Sprite s;
  s.ta.x = 0.f;
  s.ta.y = 0.f;
  s.tb.x = 1.f;
  s.tb.y = 1.f;
  s.texture = Render_white;
  s.color = color;
  s.depth = depth;
  s.root.x = (F32) point.x * tile.x;
  s.root.y = (F32) point.y * tile.y;
  s.size = v2f_of_v2s(tile);
  Display_sprite(s);
}

Void Render_init(V2S dims)
{
  Render_dimensions = dims;
  Display_init(dims, dims);
  const Byte white[] = { 0xFF, 0xFF, 0xFF, 0xFF };
  Render_white = Display_load_image(white, v2s(1, 1));
  Render_load_font(dims.x / Model_X);
}

Void Render_frame()
{

  // @rdk: should be atomic read
  const Model_T* const m = sim_model;

  Display_begin_frame();

  for (Index y = 0; y < Model_Y; y++) {
    for (Index x = 0; x < Model_X; x++) {

      const Model_Value value = m->map[y][x];
      const V2S point = { (S32) x, (S32) y };
      const Char vc = Render_char_table[value.tag];

      if (value.tag == Model_VALUE_LITERAL) {
        const S32 literal = m->map[y][x].literal;
        const Char letter = 'A' + (Char) literal - 10;
        const Char digit = '0' + (Char) literal;
        const Char c = literal > 9 ? letter : digit;
        Render_draw_grid_character(point, c, Render_COLOR_LITERAL);
      } else if (vc != 0) {
        Render_draw_grid_character(point, vc, Render_COLOR_OPERATOR);
      } else {
        Render_draw_grid_character(point, Render_EMPTY_CHARACTER, Render_COLOR_EMPTY);
      }
    }
  }

  Render_draw_grid_highlight(sim_cursor, 0x40FFFFFF, 1.f);

  Display_end_frame();

}

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
