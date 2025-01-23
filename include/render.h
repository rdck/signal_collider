/*******************************************************************************
 * render.h
 ******************************************************************************/

#pragma once

#include <SDL3/SDL_render.h>
#include "model.h"
#include "view.h"

Void render_init(SDL_Renderer* renderer);
Void render_frame(const View* view, const Model* model);

// @rdk: This doesn't feel like it should be the job of the renderer.
V2S render_tile_size();
