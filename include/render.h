/*******************************************************************************
 * render.h
 ******************************************************************************/

#pragma once

#include <SDL3/SDL_render.h>
#include "model.h"
#include "view.h"

Void render_init(SDL_Renderer* renderer);
Void render_frame(SDL_Renderer* renderer, const View* view, const Model* model);
