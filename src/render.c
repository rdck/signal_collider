#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_log.h>
#include "render.h"

typedef struct GPUFont {
  V2S glyph;
  SDL_Texture* texture;
} GPUFont; 

static Void load_font(GPUFont* out, const Font* font)
{
}

Void render_frame(const ModelGraph* model_graph, const RenderMetrics* metrics)
{
}

Void render_waveform(S32 index, Sound sound)
{
  ASSERT(index >= 0);
  ASSERT(index < MODEL_RADIX);

  // @rdk: We should use the display size for this, not the window size.
  V2S window = {0};
  const Bool output_size_status = SDL_GetRenderOutputSize(renderer, &window.x, &window.y);
  ASSERT(output_size_status);

  // @rdk: pull this out
  const S32 sample_x = view_panel_width(view);
  const S32 sample_y = (window.y - view_menu_height(view)) / MODEL_RADIX;

  Byte* const channels = SDL_calloc(1, sample_x * sample_y);
  ASSERT(channels);

  const Index frames_per_pixel = sound.frames / sample_x;
  for (Index i = 0; i < sample_x; i++) {

    // compute max for period
    F32 max = 0.f;
    const Index start = frames_per_pixel * i;
    for (Index j = 0; j < frames_per_pixel; j++) {
      const F32 l = sound.samples[2 * (start + j) + 0];
      const F32 r = sound.samples[2 * (start + j) + 1];
      max = MAX(max, MAX(l, r));
    }

    // fill line
    const Index line_height = MIN(sample_y, (Index) (max * sample_y));
    const Index top = (sample_y - line_height) / 2;
    for (Index j = 0; j < line_height; j++) {
      const Index k = top + j;
      channels[k * sample_x + i] = 0x15;
    }

  }

  SDL_Surface* const surface = SDL_CreateSurfaceFrom(
      sample_x,               // width
      sample_y,               // height
      SDL_PIXELFORMAT_RGB332, // pixel format
      channels,               // pixel data
      sample_x                // pitch
      );
  ASSERT(surface);

  // @rdk: free previous texture, if one exists
  // upload texture
  waveform_textures[index].dimensions = v2s(sample_x, sample_y);
  waveform_textures[index].texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (waveform_textures[index].texture == NULL) {
    SDL_Log("Failed to create texture: %s", SDL_GetError());
  }

}
