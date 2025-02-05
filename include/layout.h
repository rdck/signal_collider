#pragma once

// We only include this for the color struct. To reduce coupling with SDL, we
// could use our own color / rectangle types until render time.
#include <SDL3/SDL_pixels.h>
#include "model.h"
#include "rectangle.h"

#define LAYOUT_INTERACTION_RECTANGLES 0x1000
#define LAYOUT_DRAW_RECTANGLES 0x1000

typedef enum TextureName {
  TEXTURE_NONE,
  TEXTURE_WHITE,
  TEXTURE_FONT_SMALL,
  TEXTURE_FONT_LARGE,
  TEXTURE_WAVEFORM,
  TEXTURE_CARDINAL,
} TextureName;

typedef struct TextureDescription {
  TextureName name;
  union {
    Char character;
    S32 index;
  };
} TextureDescription;

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
  FILE_MENU_SAVE,
  FILE_MENU_SAVE_AS,
  FILE_MENU_EXIT,
  FILE_MENU_CARDINAL,
} FileMenuItem;

typedef enum EditMenuItem {
  EDIT_MENU_NONE,
  EDIT_MENU_CUT,
  EDIT_MENU_COPY,
  EDIT_MENU_PASTE,
  EDIT_MENU_CARDINAL,
} EditMenuItem;

typedef struct UIState {

  Interaction interaction;

  // which menu the user is interacting with (if any)
  Menu menu;

  // program cursor
  V2S cursor;

  // camera position, in tiles
  V2F camera;

} UIState;

typedef struct RenderMetrics {
  U64 frame_time;       // in microseconds
  U64 frame_count;      // frames elapsed since startup
  Index render_index;   // index into history buffer
} RenderMetrics;

typedef struct LayoutParameters {
  V2S window;
  V2S font_small;
  V2S font_large;
  const Model* model;
  const Graph* graph;
  const DSPState* dsp;
  const RenderMetrics* metrics;
} LayoutParameters;

typedef struct InteractionRectangle {
  R2F area;
} InteractionRectangle;

typedef struct DrawRectangle {
  R2F area;
  SDL_Color color;
  TextureDescription texture;
} DrawRectangle;

Void layout(
    InteractionRectangle* interaction,
    DrawRectangle* draw,
    const UIState* ui,
    const LayoutParameters* parameters);
