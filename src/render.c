#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>
#include "render.h"
#include "font.ttf.h"
#include "stb_truetype.h"

#define FONT_SIZE 48 // in pixels
#define UI_FONT_SIZE 24 // in pixels
#define ASCII_X 16
#define ASCII_Y 8
#define ASCII_AREA (ASCII_X * ASCII_Y)
#define EMPTY_CHARACTER '.'
#define MIN_CHAR '!'
#define MAX_CHAR '~'
#define COLOR_CHANNELS 4
#define COLOR_STRUCTURE(rv, gv, bv, av) { .r = rv, .g = gv, .b = bv, .a = av }

static const SDL_Color color_white     = COLOR_STRUCTURE(0xFF, 0xFF, 0xFF, 0xFF);
static const SDL_Color color_empty     = COLOR_STRUCTURE(0xFF, 0xFF, 0xFF, 0x80);
static const SDL_Color color_literal   = COLOR_STRUCTURE(0x80, 0x80, 0xFF, 0xFF);
static const SDL_Color color_pulse     = COLOR_STRUCTURE(0x80, 0xFF, 0x80, 0xFF);
static const SDL_Color color_unpowered = COLOR_STRUCTURE(0xA0, 0xA0, 0xA0, 0xFF);
static const SDL_Color color_cursor    = COLOR_STRUCTURE(0x80, 0x80, 0xFF, 0x40);
static const SDL_Color color_graph     = COLOR_STRUCTURE(0xFF, 0xFF, 0xFF, 0x40);

static SDL_Renderer* renderer = NULL;
static SDL_Texture* font_texture = NULL;
static V2S glyph_size = {0};

static Char representation_table[VALUE_CARDINAL] = {
  [ VALUE_LITERAL       ] = 0,
  [ VALUE_BANG          ] = '!',
  [ VALUE_ADD           ] = '+',
  [ VALUE_SUB           ] = '-',
  [ VALUE_MUL           ] = '*',
  [ VALUE_DIV           ] = '/',
  [ VALUE_EQUAL         ] = '=',
  [ VALUE_GREATER       ] = '>',
  [ VALUE_LESSER        ] = '<',
  [ VALUE_AND           ] = '&',
  [ VALUE_OR            ] = '|',
  [ VALUE_ALTER         ] = 'A',
  [ VALUE_BOTTOM        ] = 'B',
  [ VALUE_CLOCK         ] = 'C',
  [ VALUE_DELAY         ] = 'D',
  [ VALUE_HOP           ] = 'H',
  [ VALUE_INTERFERE     ] = 'I',
  [ VALUE_JUMP          ] = 'J',
  [ VALUE_LOAD          ] = 'L',
  [ VALUE_MULTIPLEX     ] = 'M',
  [ VALUE_NOTE          ] = 'N',
  [ VALUE_ODDMENT       ] = 'O',
  [ VALUE_QUOTE         ] = 'Q',
  [ VALUE_RANDOM        ] = 'R',
  [ VALUE_STORE         ] = 'S',
  [ VALUE_TOP           ] = 'T',
  [ VALUE_SAMPLER       ] = 'X',
  [ VALUE_SYNTH         ] = 'Y',
  [ VALUE_MIDI          ] = 'Z',
};

static V2S font_coordinate(Char c)
{
  V2S out;
  out.x = c % ASCII_X;
  out.y = c / ASCII_X;
  return out;
}

static Bool valid_atlas_point(V2S c, V2S d)
{
  const Bool x = c.x >= 0 && c.x < d.x;
  const Bool y = c.y >= 0 && c.y < d.y;
  return x && y;
}

static SDL_Texture* load_font(S32 font_size)
{
  stbtt_fontinfo font = {0};
  const S32 init_result = stbtt_InitFont(&font, font_hack, 0);
  ASSERT(init_result);

  const F32 scale = stbtt_ScaleForPixelHeight(&font, (F32) font_size);

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
  glyph_size = v2s(advance, font_size);
  const S32 char_area = glyph_size.x * glyph_size.y;
  const V2S graph = v2s_mul(v2s(ASCII_X, ASCII_Y), glyph_size);

  // allocate the atlas and clear it to zero
  Byte* const atlas = SDL_calloc(1, ASCII_AREA * char_area);

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

  Byte* const channels = SDL_malloc(COLOR_CHANNELS * ASCII_AREA * char_area);
  for (Index y = 0; y < graph.y; y++) {
    for (Index x = 0; x < graph.x; x++) {
      const Byte alpha = atlas[y * graph.x + x];
      channels[COLOR_CHANNELS * (y * graph.x + x) + 0] = 0xFF;
      channels[COLOR_CHANNELS * (y * graph.x + x) + 1] = 0xFF;
      channels[COLOR_CHANNELS * (y * graph.x + x) + 2] = 0xFF;
      channels[COLOR_CHANNELS * (y * graph.x + x) + 3] = alpha;
    }
  }

  SDL_Surface* const surface = SDL_CreateSurfaceFrom(
      ASCII_X * glyph_size.x,                 // width
      ASCII_Y * glyph_size.y,                 // height
      SDL_PIXELFORMAT_RGBA32,                 // pixel format
      channels,                               // pixel data
      COLOR_CHANNELS * ASCII_X * glyph_size.x // pitch
      );

  SDL_Texture* const texture = SDL_CreateTextureFromSurface(renderer, surface);
  ASSERT(texture);

  SDL_DestroySurface(surface);
  SDL_free(channels);
  SDL_free(atlas);
  return texture;
}

V2S render_tile_size()
{
  const S32 tile = MAX(glyph_size.x, glyph_size.y);
  return v2s(tile, tile);
}

Void render_init(SDL_Renderer* sdl_renderer)
{
  // store global renderer pointer
  renderer = sdl_renderer;

  const Bool blend_status = SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  ASSERT(blend_status);

  font_texture = load_font(FONT_SIZE);
}

static Void draw_character(V2F camera, V2S point, Char c, SDL_Color color)
{
  const V2S p     = font_coordinate(c);
  const V2S tile  = render_tile_size();
  const V2S delta = v2s_sub(tile, glyph_size);
  const V2F relative = v2f_sub(v2f_of_v2s(point), camera);

  SDL_FRect source;
  source.x = (F32) p.x * glyph_size.x;
  source.y = (F32) p.y * glyph_size.y;
  source.w = (F32) glyph_size.x;
  source.h = (F32) glyph_size.y;
  
  SDL_FRect destination;
  destination.x = relative.x * tile.x + (delta.x / 2);
  destination.y = relative.y * tile.y + (delta.y / 2);
  destination.w = (F32) glyph_size.x;
  destination.h = (F32) glyph_size.y;

  SDL_SetTextureColorMod(font_texture, color.r, color.g, color.b);
  SDL_SetTextureAlphaMod(font_texture, color.a);
  SDL_RenderTexture(renderer, font_texture, &source, &destination);
}

static Void draw_highlight(V2F camera, V2S point, SDL_Color color)
{
  const V2S tile = render_tile_size();
  const V2F relative = v2f_sub(v2f_of_v2s(point), camera);

  SDL_FRect destination;
  destination.x = relative.x * tile.x;
  destination.y = relative.y * tile.y;
  destination.w = (F32) tile.x;
  destination.h = (F32) tile.y;

  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &destination);
}

Void render_frame(const View* view, const Model* m, const RenderMetrics* metrics)
{
  // clear
  SDL_SetRenderDrawColorFloat(renderer, 0.1f, 0.1f, 0.1f, SDL_ALPHA_OPAQUE_FLOAT);
  SDL_RenderClear(renderer);

  // draw model
  for (S32 y = 0; y < MODEL_Y; y++) {
    for (S32 x = 0; x < MODEL_X; x++) {

      const Value value = m->map[y][x];
      const V2S point = { (S32) x, (S32) y };
      const Char tag_character = representation_table[value.tag];

      if (value.tag == VALUE_LITERAL) {
        const S32 literal = m->map[y][x].literal;
        const Char letter = 'A' + (Char) literal - 10;
        const Char digit = '0' + (Char) literal;
        const Char literal_character = literal > 9 ? letter : digit;
        draw_character(view->camera, point, literal_character, color_literal);
      } else if (tag_character != 0) {
        const SDL_Color color = value.powered
          ? color_white
          : (value.pulse ? color_pulse : color_unpowered);
        draw_character(view->camera, point, tag_character, color);
      } else {
        draw_character(view->camera, point, EMPTY_CHARACTER, color_empty);
      }

    }
  }

  // draw cursor highlight
  draw_highlight(view->camera, view->cursor, color_cursor);

  // present
  SDL_RenderPresent(renderer);
}

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
