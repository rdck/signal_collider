#include "render.h"

Void render_init(SDL_Renderer* renderer)
{
}

Void render_frame(SDL_Renderer* renderer, const View* view, const Model* model)
{
  SDL_SetRenderDrawColorFloat(renderer, 0.3f, 0.2f, 0.1f, SDL_ALPHA_OPAQUE_FLOAT);
  SDL_RenderClear(renderer);
  SDL_RenderPresent(renderer);
}
