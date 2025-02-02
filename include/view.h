/*******************************************************************************
 * view.h - model of user interface
 ******************************************************************************/

#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_asyncio.h>
#include <SDL3/SDL_render.h>
#include "model.h"
#include "linear_algebra.h"

typedef enum Interaction {
  INTERACTION_NONE,
  INTERACTION_CAMERA,
  INTERACTION_MENU,
  INTERACTION_FILE_DIALOG,
  INTERACTION_CARDINAL,
} Interaction;

typedef enum Menu {
  MENU_NONE,
  MENU_FILE,
  MENU_EDIT,
  MENU_CARDINAL,
} Menu;

typedef enum FileMenuItem {
  FILE_MENU_NONE,
  FILE_MENU_NEW,
  FILE_MENU_SAVE_AS,
  FILE_MENU_EXIT,
  FILE_MENU_CARDINAL,
} FileMenuItem;

typedef enum EditMenuItem {
  EDIT_MENU_NONE,
  EDIT_MENU_COPY,
  EDIT_MENU_PASTE,
  EDIT_MENU_CARDINAL,
} EditMenuItem;

typedef struct Font {
  V2S glyph;
  SDL_Texture* texture;
} Font;

typedef struct Texture {
  V2S dimensions;
  SDL_Texture* texture;
} Texture;

typedef struct View {

  SDL_Renderer* renderer;
  SDL_AsyncIOQueue* io_queue;

  Interaction interaction;
  union {
    FileMenuItem file_menu;
    EditMenuItem edit_menu;
  };

  Font font_small;
  Font font_large;
  Texture waveforms[MODEL_RADIX];

  F32 scale;

  // cursor position
  V2S cursor;

  // camera position (measured in tiles)
  V2F camera;

} View;

typedef struct RenderMetrics {
  U64 frame_time;       // in microseconds
  U64 frame_count;      // frames elapsed since startup
  Index render_index;   // index into history buffer
} RenderMetrics;

Void view_init(View* view, SDL_Renderer* renderer, F32 scale);
Void view_event(View* view, const SDL_Event* event);
Void view_step(View* view);
Void view_render(View* view, const ModelGraph* model_graph, const DSPState* dsp, const RenderMetrics* metrics);
