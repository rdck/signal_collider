#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>
#include "render.h"
#include "font.ttf.h"
#include "stb_truetype.h"

#define WORLD_FONT_SIZE 48 // in pixels
#define UI_FONT_SIZE 28 // in pixels
#define ASCII_X 16
#define ASCII_Y 8
#define ASCII_AREA (ASCII_X * ASCII_Y)
#define EMPTY_CHARACTER '.'
#define MIN_CHAR '!'
#define MAX_CHAR '~'
#define COLOR_CHANNELS 4
#define METRICS_BUFFER 0x100
#define COLOR_STRUCTURE(rv, gv, bv, av) { .r = rv, .g = gv, .b = bv, .a = av }

static const SDL_Color color_white     = COLOR_STRUCTURE(0xFF, 0xFF, 0xFF, 0xFF);
static const SDL_Color color_empty     = COLOR_STRUCTURE(0xFF, 0xFF, 0xFF, 0x80);
static const SDL_Color color_literal   = COLOR_STRUCTURE(0x80, 0x80, 0xFF, 0xFF);
static const SDL_Color color_pulse     = COLOR_STRUCTURE(0x80, 0xFF, 0x80, 0xFF);
static const SDL_Color color_unpowered = COLOR_STRUCTURE(0xA0, 0xA0, 0xA0, 0xFF);
static const SDL_Color color_cursor    = COLOR_STRUCTURE(0x80, 0x80, 0xFF, 0x40);
static const SDL_Color color_graph     = COLOR_STRUCTURE(0xFF, 0xFF, 0xFF, 0x40);

typedef struct Font {
  SDL_Texture* texture;   // handle to gpu texture
  V2S glyph;              // size of a single glyph, in pixels
} Font;

static SDL_Renderer* renderer = NULL;

// font data
static Font world_font = {0};
static Font ui_font = {0};

// cache the world tile size
static S32 world_tile = 0;

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

static Font load_font(S32 font_size)
{
  // the font that we will return
  Font font = {0};

  stbtt_fontinfo font_info = {0};
  const S32 init_result = stbtt_InitFont(&font_info, font_hack, 0);
  ASSERT(init_result);

  const F32 scale = stbtt_ScaleForPixelHeight(&font_info, (F32) font_size);

  // vertical metrics
  S32 ascent = 0;
  S32 descent = 0;
  S32 line_gap = 0;
  stbtt_GetFontVMetrics(&font_info, &ascent, &descent, &line_gap);
  const S32 scaled_ascent = (S32) (scale * ascent) + 1;

  // horizontal metrics
  S32 raw_advance = 0;
  S32 left_side_bearing = 0;
  stbtt_GetCodepointHMetrics(&font_info, 'M', &raw_advance, &left_side_bearing);

  // dimensions
  font.glyph.x = (S32) (scale * raw_advance) + 1;
  font.glyph.y = font_size;
  const S32 char_area = font.glyph.x * font.glyph.y;
  const V2S graph = v2s_mul(v2s(ASCII_X, ASCII_Y), font.glyph);

  // allocate the atlas and clear it to zero
  Byte* const atlas = SDL_calloc(1, ASCII_AREA * char_area);

  S32 w = 0;
  S32 h = 0;
  S32 xoff = 0;
  S32 yoff = 0;

  // iterate through ASCII characters, rendering bitmaps to atlas
  for (Char c = MIN_CHAR; c <= MAX_CHAR; c++) {

    Byte* const bitmap = stbtt_GetCodepointBitmap(
        &font_info,
        scale, scale,
        c,
        &w, &h,
        &xoff, &yoff
        );
    ASSERT(w <= font.glyph.x);
    ASSERT(h <= font.glyph.y);

    const V2S p = font_coordinate(c);
    const V2S o = v2s_mul(p, font.glyph);
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
    for (Index x = 0; x < ASCII_X * font.glyph.x; x++) {
      atlas[y * font.glyph.y * graph.x + x] = 0xFF;
    }
  }
  for (Index x = 0; x < ASCII_X; x++) {
    for (Index y = 0; y < ASCII_Y * font.glyph.y; y++) {
      atlas[y * graph.x + x * font.glyph.x] = 0xFF;
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
      ASCII_X * font.glyph.x,                 // width
      ASCII_Y * font.glyph.y,                 // height
      SDL_PIXELFORMAT_RGBA32,                 // pixel format
      channels,                               // pixel data
      COLOR_CHANNELS * ASCII_X * font.glyph.x // pitch
      );

  // create gpu texture
  font.texture = SDL_CreateTextureFromSurface(renderer, surface);
  ASSERT(font.texture);

  SDL_DestroySurface(surface);
  SDL_free(channels);
  SDL_free(atlas);
  return font;
}

V2S render_tile_size()
{
  const S32 tile = MAX(world_font.glyph.x, world_font.glyph.y);
  return v2s(tile, tile);
}

Void render_init(SDL_Renderer* sdl_renderer)
{
  // store global renderer pointer
  renderer = sdl_renderer;

  const Bool blend_status = SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  ASSERT(blend_status);

  // load world font
  world_font = load_font(WORLD_FONT_SIZE);
  world_tile = MAX(world_font.glyph.x, world_font.glyph.y);

  // load ui font
  ui_font = load_font(UI_FONT_SIZE);
}

static Void draw_character(const Font* font, SDL_Color color, V2F origin, Char c)
{
  const V2S uv = font_coordinate(c);

  // texture coordinates for the glyph
  SDL_FRect source;
  source.x = (F32) (uv.x * font->glyph.x);
  source.y = (F32) (uv.y * font->glyph.y);
  source.w = (F32) font->glyph.x;
  source.h = (F32) font->glyph.y;

  // screen coordinates to fill
  SDL_FRect destination;
  destination.x = origin.x;
  destination.y = origin.y;
  destination.w = (F32) font->glyph.x;
  destination.h = (F32) font->glyph.y;

  // draw
  SDL_SetTextureColorMod(font->texture, color.r, color.g, color.b);
  SDL_SetTextureAlphaMod(font->texture, color.a);
  SDL_RenderTexture(renderer, font->texture, &source, &destination);
}

static Void draw_text(const Font* font, SDL_Color color, V2F origin, const Char* text)
{
  V2F cursor = origin;

  while (*text) {
    if (*text == '\n') {
      cursor.x = origin.x;
      cursor.y += (F32) font->glyph.y;
    } else {
      draw_character(font, color, cursor, *text);
      cursor.x += font->glyph.x;
    }
    text += 1;
  }
}

static V2F world_to_screen(V2F camera, V2S point)
{
  const V2F tile = { (F32) world_tile, (F32) world_tile };
  const V2F relative = v2f_sub(v2f_of_v2s(point), camera);
  return v2f_mul(relative, tile);
}

static Void draw_world_character(V2F camera, SDL_Color color, V2S point, Char c)
{
  const V2F origin = world_to_screen(camera, point);
  const F32 delta = (world_tile - world_font.glyph.x) / 2.f;
  const V2F screen = { origin.x + delta, origin.y };
  draw_character(&world_font, color, screen, c);
}

static Void draw_world_highlight(V2F camera, SDL_Color color, V2S point)
{
  const V2F origin = world_to_screen(camera, point);

  // screen coordinates to fill
  SDL_FRect destination;
  destination.x = origin.x;
  destination.y = origin.y;
  destination.w = (F32) world_tile;
  destination.h = (F32) world_tile;

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
        draw_world_character(view->camera, color_literal, point, literal_character);
      } else if (tag_character != 0) {
        const SDL_Color color = value.powered
          ? color_white
          : (value.pulse ? color_pulse : color_unpowered);
        draw_world_character(view->camera, color, point, tag_character);
      } else {
        draw_world_character(view->camera, color_empty, point, EMPTY_CHARACTER);
      }

    }
  }

  // draw performance metrics
  Char metrics_buffer[METRICS_BUFFER] = {0};
  SDL_snprintf(
      metrics_buffer,
      METRICS_BUFFER,
      "DEBUG METRICS\nframe time: %03llu.%03llums\nframe count: %llu\nhistory index: %03lld",
      metrics->frame_time / KILO,
      metrics->frame_time % KILO,
      metrics->frame_count,
      metrics->render_index);
  draw_text(&ui_font, color_white, v2f(0.f, 0.f), metrics_buffer);

  // draw cursor highlight
  draw_world_highlight(view->camera, color_cursor, view->cursor);

  // present
  SDL_RenderPresent(renderer);
}

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
