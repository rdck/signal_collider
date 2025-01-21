#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>
#include "render.h"
#include "font.ttf.h"
#include "stb_truetype.h"

#define FONT_SIZE 28 // in pixels
#define ASCII_X 16
#define ASCII_Y 8
#define ASCII_AREA (ASCII_X * ASCII_Y)
#define EMPTY_CHARACTER '.'
#define MIN_CHAR '!'
#define MAX_CHAR '~'
#define COLOR_CHANNELS 4

static SDL_Texture* font_texture = NULL;
static V2S glyph_size = {0};

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

Void render_init(SDL_Renderer* renderer)
{
  stbtt_fontinfo font = {0};
  const S32 init_result = stbtt_InitFont(&font, font_hack, 0);
  ASSERT(init_result);

  const F32 scale = stbtt_ScaleForPixelHeight(&font, FONT_SIZE);

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
  glyph_size = v2s(advance, FONT_SIZE);
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

  // @rdk: We should make this a mono texture.
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

  font_texture = SDL_CreateTextureFromSurface(renderer, surface);
  ASSERT(font_texture);

  SDL_DestroySurface(surface);
  SDL_free(channels);
  SDL_free(atlas);
}

Void render_frame(SDL_Renderer* renderer, const View* view, const Model* model)
{
  // clear
  SDL_SetRenderDrawColorFloat(renderer, 0.1f, 0.1f, 0.1f, SDL_ALPHA_OPAQUE_FLOAT);
  SDL_RenderClear(renderer);

  // draw font texture
  SDL_RenderTexture(renderer, font_texture, NULL, NULL);

  // present
  SDL_RenderPresent(renderer);
}

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
