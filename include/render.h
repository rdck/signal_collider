/*******************************************************************************
 * render.h
 ******************************************************************************/

#pragma once

#include <SDL3/SDL_render.h>
#include "model.h"
#include "view.h"
#include "sound.h"

typedef struct RenderMetrics {
  U64 frame_time;       // in microseconds
  U64 frame_count;      // frames elapsed since startup
  Index render_index;   // index into history buffer
} RenderMetrics;

Void render_init(SDL_Renderer* renderer, const View* view);
Void render_frame(const ModelGraph* model_graph, const RenderMetrics* metrics);
Void render_waveform(S32 index, Sound sound);
