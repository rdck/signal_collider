#include "render.h"
#include "display.h"
#include "sim.h"
#include "view.h"
#include "font.ttf.h"
#include "stb_truetype.h"

#ifdef WRITE_ATLAS
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

#define FONT_SIZE 48 // in pixels
#define ASCII_X 16
#define ASCII_Y 8
#define ASCII_AREA (ASCII_X * ASCII_Y)
#define EMPTY_CHARACTER '.'
#define MIN_CHAR '!'
#define MAX_CHAR '~'
#define COLOR_CHANNELS 4

#define COLOR_LITERAL    0xFFFF8080
#define COLOR_POWERED    0xFFFFFFFF
#define COLOR_UNPOWERED  0xFFA0A0A0
#define COLOR_EMPTY      0x80FFFFFF
#define COLOR_CURSOR     0x40FFFFFF
#define COLOR_CONSOLE_BG 0xFF202020

// @rdk: unify
#define SIM_PI                 3.141592653589793238f

static V2S canvas_dimensions = {0};
static TextureID texture_white = 0;
static TextureID texture_font = 0;
static V2S glyph_size = {0};

static Char representation_table[VALUE_CARDINAL] = {
  [ VALUE_LITERAL       ] = 0,
  [ VALUE_BANG          ] = '*',
  [ VALUE_ADD           ] = '+',
  [ VALUE_SUB           ] = '-',
  [ VALUE_MUL           ] = '*',
  [ VALUE_DIV           ] = '/',
  [ VALUE_EQUAL         ] = '=',
  [ VALUE_GREATER       ] = '>',
  [ VALUE_LESSER        ] = '<',
  [ VALUE_AND           ] = '&',
  [ VALUE_OR            ] = '|',
  [ VALUE_CLOCK         ] = 'C',
  [ VALUE_DELAY         ] = 'D',
  [ VALUE_HOP           ] = 'H',
  [ VALUE_JUMP          ] = 'J',
  [ VALUE_GENERATE      ] = 'I',
  [ VALUE_SCALE         ] = 'N',
  [ VALUE_RANDOM        ] = 'R',
  [ VALUE_SAMPLER       ] = 'X',
  [ VALUE_SYNTH         ] = 'Y',
};

static V2S tile_size(V2S canvas)
{
  const S32 tile = MAX(glyph_size.x, glyph_size.y);
  return v2s(tile, tile);
}

static Bool valid_atlas_point(V2S c, V2S d)
{
  const Bool x = c.x >= 0 && c.x < d.x;
  const Bool y = c.y >= 0 && c.y < d.y;
  return x && y;
}

static V2S font_coordinate(Char c)
{
  V2S out;
  out.x = c % ASCII_X;
  out.y = c / ASCII_X;
  return out;
}

static Void load_font(S32 size)
{
  stbtt_fontinfo font = {0};
  const S32 init_result = stbtt_InitFont(&font, font_hack, 0);
  ASSERT(init_result);

  const F32 scale = stbtt_ScaleForPixelHeight(&font, (F32) size);

  // vertical metrics
  S32 ascent = 0;
  S32 descent = 0;
  S32 line_gap = 0;
  stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);
  const S32 scaled_ascent = (S32) (scale * ascent) + 1;

  // horizontal metrics
  S32 raw_advance = 0;
  S32 left_side_bearing = 0;
  stbtt_GetCodepointHMetrics(&font, 'M', &raw_advance, &left_side_bearing);
  const S32 advance = (S32) (scale * raw_advance) + 1;

  // dimensions
  glyph_size = v2s(advance, size);
  const S32 char_area = glyph_size.x * glyph_size.y;
  const V2S graph = v2s_mul(v2s(ASCII_X, ASCII_Y), glyph_size);

  // allocate the atlas and clear it to zero
  Byte* const atlas = malloc(ASCII_AREA * char_area);
  memset(atlas, 0, ASCII_AREA * char_area);

  S32 w = 0;
  S32 h = 0;
  S32 xoff = 0;
  S32 yoff = 0;

  // iterate through ASCII characters, rendering bitmaps to atlas
  for (Char c = MIN_CHAR; c <= MAX_CHAR; c++) {

    Byte* const bitmap = stbtt_GetCodepointBitmap(
        &font,
        scale, scale,
        c,
        &w, &h,
        &xoff, &yoff
        );
    ASSERT(w <= glyph_size.x);
    ASSERT(h <= glyph_size.y);

    const V2S p = font_coordinate(c);
    const V2S o = v2s_mul(p, glyph_size);
    for (S32 y = 0; y < h; y++) {
      for (S32 x = 0; x < w; x++) {
        const S32 x0 = o.x + xoff + x;
        const S32 y0 = o.y + scaled_ascent + yoff + y;
        if (valid_atlas_point(v2s(x0, y0), graph)) {
          atlas[y0 * graph.x + x0] = bitmap[y * w + x];
        }
      }
    }

    stbtt_FreeBitmap(bitmap, NULL);

  }

#ifdef ATLAS_LINES
  // draw debug line separators in the atlas
  for (Index y = 0; y < ASCII_Y; y++) {
    for (Index x = 0; x < ASCII_X * advance; x++) {
      atlas[y * glyph_size.y * graph.x + x] = 0xFF;
    }
  }
  for (Index x = 0; x < ASCII_X; x++) {
    for (Index y = 0; y < ASCII_Y * glyph_size.y; y++) {
      atlas[y * graph.x + x * advance] = 0xFF;
    }
  }
#endif

#ifdef WRITE_ATLAS
  // write the atlas to a file for debugging
  stbi_write_png(
      "font_atlas.png",
      graph.x,
      graph.y,
      1,
      atlas,
      graph.x
      );
#endif

  // Our shader expects four channels, not one. We can add a mono texture
  // shader later, if needed.
  Byte* const channels = malloc(COLOR_CHANNELS * ASCII_AREA * char_area);
  for (Index y = 0; y < graph.y; y++) {
    for (Index x = 0; x < graph.x; x++) {
      const Byte alpha = atlas[y * graph.x + x];
      channels[COLOR_CHANNELS * (y * graph.x + x) + 0] = 0xFF;
      channels[COLOR_CHANNELS * (y * graph.x + x) + 1] = 0xFF;
      channels[COLOR_CHANNELS * (y * graph.x + x) + 2] = 0xFF;
      channels[COLOR_CHANNELS * (y * graph.x + x) + 3] = alpha;
    }
  }

  texture_font = display_load_image(channels, graph);

  free(channels);
  free(atlas);
}

static Void draw_character(V2S point, Char c, U32 color)
{
  const V2S p     = font_coordinate(c);
  const V2S tile  = tile_size(canvas_dimensions);
  const V2S delta = v2s_sub(tile, glyph_size);

  Sprite s;
  s.ta.x = (p.x + 0) / (F32) ASCII_X;
  s.ta.y = (p.y + 0) / (F32) ASCII_Y;
  s.tb.x = (p.x + 1) / (F32) ASCII_X;
  s.tb.y = (p.y + 1) / (F32) ASCII_Y;
  s.color = color;
  s.root.x = (F32) (point.x * tile.x + delta.x / 2);
  s.root.y = (F32) (point.y * tile.y + delta.y / 2);
  s.size = v2f_of_v2s(glyph_size);
  display_draw_sprite_struct(s);
}

static Void draw_console_character(V2S point, Char c, U32 color)
{
  const V2S p     = font_coordinate(c);
  Sprite s;
  s.ta.x = (p.x + 0) / (F32) ASCII_X;
  s.ta.y = (p.y + 0) / (F32) ASCII_Y;
  s.tb.x = (p.x + 1) / (F32) ASCII_X;
  s.tb.y = (p.y + 1) / (F32) ASCII_Y;
  s.color = color;
  s.root.x = (F32) (point.x * glyph_size.x);
  s.root.y = (F32) (point.y * glyph_size.y);
  s.size = v2f_of_v2s(glyph_size);
  display_draw_sprite_struct(s);
}

static Void draw_highlight(V2S point, U32 color)
{
  const V2S tile = tile_size(canvas_dimensions);
  Sprite s;
  s.ta.x = 0.f;
  s.ta.y = 0.f;
  s.tb.x = 1.f;
  s.tb.y = 1.f;
  s.color = color;
  s.root.x = (F32) point.x * tile.x;
  s.root.y = (F32) point.y * tile.y;
  s.size = v2f_of_v2s(tile);
  display_draw_sprite_struct(s);
}

Void render_init(V2S dimensions)
{
  canvas_dimensions = dimensions;
  display_init(dimensions, dimensions);
  const Byte white[] = { 0xFF, 0xFF, 0xFF, 0xFF };
  texture_white = display_load_image(white, v2s(1, 1));
  // load_font(dimensions.x / MODEL_X);
  load_font(FONT_SIZE);
}

Void render_frame(const Model* m)
{
  // mark the beginning of the frame
  display_begin_frame();

  // draw the text
  display_begin_draw(texture_font);

  for (Index y = 0; y < MODEL_Y; y++) {
    for (Index x = 0; x < MODEL_X; x++) {

      const Value value = m->map[y][x];
      const V2S point = { (S32) x, (S32) y };
      const Char tag_character = representation_table[value.tag];

      if (value.tag == VALUE_LITERAL) {
        const S32 literal = m->map[y][x].literal;
        const Char letter = 'A' + (Char) literal - 10;
        const Char digit = '0' + (Char) literal;
        const Char literal_character = literal > 9 ? letter : digit;
        draw_character(point, literal_character, COLOR_LITERAL);
      } else if (tag_character != 0) {
        const U32 color = value.powered ? COLOR_POWERED : COLOR_UNPOWERED;
        draw_character(point, tag_character, color);
      } else {
        draw_character(point, EMPTY_CHARACTER, COLOR_EMPTY);
      }
    }
  }

  display_end_draw();

  // draw the cursor highlight
  display_begin_draw(texture_white);
  draw_highlight(cursor, COLOR_CURSOR);
  display_end_draw();

  // draw the console, if active
  if (view_state == VIEW_STATE_CONSOLE) {

    // draw the console background
    display_begin_draw(texture_white);
    Sprite s;
    s.ta.x = 0.f;
    s.ta.y = 0.f;
    s.tb.x = 1.f;
    s.tb.y = 1.f;
    s.color = COLOR_CONSOLE_BG;
    s.root.x = 0.f;
    s.root.y = 0.f;
    s.size.x = (F32) canvas_dimensions.x;
    s.size.y = (F32) glyph_size.y;
    display_draw_sprite_struct(s);
    display_end_draw();

    display_begin_draw(texture_font);
    draw_console_character(v2s(0, 0), ':', COLOR_WHITE);
    for (S32 i = 0; i < CONSOLE_BUFFER; i++) {
      const Char c = console[i];
      if (c > 0) {
        const V2S point = { i + 1, 0 };
        draw_console_character(point, c, COLOR_WHITE);
      }
    }
    display_end_draw();
  }

  // mark the end of the frame
  display_end_frame();
}

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
