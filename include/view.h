/*******************************************************************************
 * view.h - model of user interface
 ******************************************************************************/

#pragma once

#include "layout.h"

typedef struct View {

  UIState ui;
  SDL_Renderer* renderer;
  SDL_AsyncIOQueue* io_queue;
  Font font_small;
  Font font_large;
  Texture waveforms[MODEL_RADIX];

  DrawRectangle draw_buffer[LAYOUT_DRAW_RECTANGLES];
  InteractionRectangle interaction_buffer[LAYOUT_INTERACTION_RECTANGLES];

  // F32 scale;

} View;

Void view_init(View* view, SDL_Renderer* renderer, F32 scale);
Void view_event(
    View* view,
    const Model* model,
    const Graph* graph,
    const DSPState* dsp,
    const RenderMetrics* metrics,
    const SDL_Event* event);
Void view_step(View* view);
Void view_render(
    View* view,
    const Model* model,
    const Graph* graph,
    const DSPState* dsp,
    const RenderMetrics* metrics);
