#pragma once

// We only include this for the color struct. To reduce coupling with SDL, we
// could use our own color / rectangle types until render time.
#include <SDL3/SDL_pixels.h>
#include "model.h"
#include "rectangle.h"

#define LAYOUT_DRAW_RECTANGLES 0x1000
#define LAYOUT_INTERACTION_RECTANGLES 0x1000
#define LAYOUT_PANEL_CHARACTERS 40

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
  INTERACTION_MENU_SELECT,
  INTERACTION_MENU,
  INTERACTION_MENU_FINALIZE,
  INTERACTION_FILE_DIALOG,
  INTERACTION_CARDINAL,
} Interaction;

typedef enum Menu {
  MENU_NONE,
  MENU_FILE,
  MENU_EDIT,
  MENU_HELP,
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
  EDIT_MENU_CUT,
  EDIT_MENU_COPY,
  EDIT_MENU_PASTE,
  EDIT_MENU_CARDINAL,
} EditMenuItem;

typedef enum HelpMenuItem {
  HELP_MENU_NONE,
  HELP_MENU_MANUAL,
  HELP_MENU_CARDINAL,
} HelpMenuItem;

typedef struct UIState {

  Interaction interaction;

  // which menu the user is interacting with (if any)
  Menu menu;

  // program cursor
  V2S cursor;

  // camera position, in tiles
  V2F camera;

  F32 scroll;

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
  V2F mouse;
  const Model* model;
  const Graph* graph;
  const DSPState* dsp;
  const RenderMetrics* metrics;
} LayoutParameters;

typedef enum InteractionTag {
  INTERACTION_TAG_NONE,
  INTERACTION_TAG_MAP,
  INTERACTION_TAG_WAVEFORM,
  INTERACTION_TAG_SOUND_SCROLL,
  INTERACTION_TAG_MENU,
  INTERACTION_TAG_CARDINAL,
} InteractionTag;

typedef struct MenuItem {
  Menu menu;
  S32 item;
} MenuItem;

typedef struct InteractionRectangle {
  InteractionTag tag;
  union {
    S32 index;
    MenuItem menu_item;
  };
  R2F area;
} InteractionRectangle;

typedef struct DrawRectangle {
  SDL_Color color;
  TextureDescription texture;
  R2F area;
} DrawRectangle;

typedef struct DrawArena {
  DrawRectangle* buffer;
  Index head;
} DrawArena;

typedef struct InteractionArena {
  InteractionRectangle* buffer;
  Index head;
} InteractionArena;

Void layout(
    DrawArena* draw,
    InteractionArena* interaction,
    const UIState* ui,
    const LayoutParameters* parameters);
