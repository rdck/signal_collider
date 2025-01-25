/*******************************************************************************
 * render.h
 ******************************************************************************/

#pragma once

#include <SDL3/SDL_render.h>
#include "model.h"
#include "view.h"

typedef struct RenderMetrics {
  U64 frame_time;       // in microseconds
  U64 frame_count;      // frames elapsed since startup
  Index render_index;   // index into history buffer
} RenderMetrics;

Void render_init(SDL_Renderer* renderer);
Void render_frame(const View* view, const ModelGraph* model_graph, const RenderMetrics* metrics);

// @rdk: This doesn't feel like it should be the job of the renderer.
V2S render_tile_size();
