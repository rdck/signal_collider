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

#define ASCII_X 16
#define ASCII_Y 8
#define ASCII_AREA (ASCII_X * ASCII_Y)
#define EMPTY_CHARACTER '.'
#define MIN_CHAR '!'
#define MAX_CHAR '~'
#define COLOR_CHANNELS 4

#define COLOR_LITERAL    0xFFFF8080
#define COLOR_OPERATOR   0xFFFFFFFF
#define COLOR_EMPTY      0x80FFFFFF
#define COLOR_CURSOR     0x40FFFFFF

static Index render_index = INDEX_NONE;
static V2S canvas_dimensions = {0};
static TextureID texture_white = 0;
static TextureID texture_font = 0;
static V2S glyph_size = {0};

static Char representation_table[VALUE_CARDINAL] = {
  [ VALUE_LITERAL       ] = 0,
  [ VALUE_BANG          ] = '*',
  [ VALUE_IF            ] = '=',
  [ VALUE_CLOCK         ] = 'C',
  [ VALUE_DELAY         ] = 'D',
  [ VALUE_RANDOM        ] = 'R',
  [ VALUE_ADD           ] = '+',
  [ VALUE_SUB           ] = '-',
  [ VALUE_MUL           ] = '*',
  [ VALUE_GENERATE      ] = '!',
  [ VALUE_SCALE         ] = '#',
  [ VALUE_SYNTH         ] = '~',
};

static V2S tile_size(V2S canvas)
{
  return v2s_div(canvas, v2s(MODEL_X, MODEL_Y));
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
  const S32 init_result = stbtt_InitFont(&font, font_Hack_Regular_ttf, 0);
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
  s.texture = texture_font;
  s.color = color;
  s.depth = 0.f;
  s.root.x = (F32) (point.x * tile.x + delta.x / 2);
  s.root.y = (F32) (point.y * tile.y + delta.y / 2);
  s.size = v2f_of_v2s(glyph_size);
  display_sprite(s);
}

static Void draw_highlight(V2S point, U32 color, F32 depth)
{
  const V2S tile = tile_size(canvas_dimensions);
  Sprite s;
  s.ta.x = 0.f;
  s.ta.y = 0.f;
  s.tb.x = 1.f;
  s.tb.y = 1.f;
  s.texture = texture_white;
  s.color = color;
  s.depth = depth;
  s.root.x = (F32) point.x * tile.x;
  s.root.y = (F32) point.y * tile.y;
  s.size = v2f_of_v2s(tile);
  display_sprite(s);
}

Void render_init(V2S dims)
{
  canvas_dimensions = dims;
  display_init(dims, dims);
  const Byte white[] = { 0xFF, 0xFF, 0xFF, 0xFF };
  texture_white = display_load_image(white, v2s(1, 1));
  load_font(dims.x / MODEL_X);

  for (Index i = 0; i < SIM_HISTORY; i++) {
    const Message msg = message_alloc(i);
    message_enqueue(&free_queue, msg);
  }
}

Void render_frame()
{

  // empty the allocation queue
  while (message_queue_length(&alloc_queue) > 0) {

    Message msg = {0};
    message_dequeue(&alloc_queue, &msg);
    ASSERT(msg.tag == MESSAGE_ALLOCATE);
    ASSERT(msg.alloc.index >= 0);

    // I find it ugly that we check the index every frame, when this condition
    // should always be met after startup.
    if (render_index != INDEX_NONE) {
      const Message free_message = message_alloc(render_index);
      _mm_lfence();
      message_enqueue(&free_queue, free_message);
    }
    render_index = msg.alloc.index;

  }

  // Again, I would prefer if we didn't have to check this each frame.
  if (render_index != INDEX_NONE) {

    const Model* const m = &sim_history[render_index];
    display_begin_frame();

    for (Index y = 0; y < MODEL_Y; y++) {
      for (Index x = 0; x < MODEL_X; x++) {

        const Value value = m->map[y][x];
        const V2S point = { (S32) x, (S32) y };
        const Char vc = representation_table[value.tag];

        if (value.tag == VALUE_LITERAL) {
          const S32 literal = m->map[y][x].literal;
          const Char letter = 'A' + (Char) literal - 10;
          const Char digit = '0' + (Char) literal;
          const Char c = literal > 9 ? letter : digit;
          draw_character(point, c, COLOR_LITERAL);
        } else if (vc != 0) {
          draw_character(point, vc, COLOR_OPERATOR);
        } else {
          draw_character(point, EMPTY_CHARACTER, COLOR_EMPTY);
        }
      }
    }

    draw_highlight(cursor, COLOR_CURSOR, 1.f);
    display_end_frame();

  }
}

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
