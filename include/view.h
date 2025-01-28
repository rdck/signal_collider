/*******************************************************************************
 * view.h - model of user interface
 ******************************************************************************/

#pragma once

#include <SDL3/SDL_events.h>
#include "prelude.h"
#include "linear_algebra.h"

#define ASCII_X 16
#define ASCII_Y 8
#define ASCII_AREA (ASCII_X * ASCII_Y)
#define MIN_CHAR '!'
#define MAX_CHAR '~'
#define PADDING 6
#define PANEL_CHARACTERS 44
#define OPERATOR_DESCRIPTION_LINES 3

typedef enum Interaction {
  INTERACTION_NONE,
  INTERACTION_CAMERA,
  INTERACTION_MENU,
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
  Byte* bitmap;
} Font;

typedef struct View {

  Interaction interaction;
  union {
    FileMenuItem file_menu;
    EditMenuItem edit_menu;
  };

  Font font_small;
  Font font_large;

  // cursor position
  V2S cursor;

  // camera position (measured in tiles)
  V2F camera;

} View;

Void view_init(View* view);
Void view_event(View* view, const SDL_Event* event);
Void view_step(View* view);

// character position in atlas
V2S view_atlas_coordinate(Char c);

// validate character position in atlas
Bool view_validate_atlas_coordinate(V2S c, V2S dimensions);

S32 view_panel_width(const View* view);
S32 view_menu_height(const View* view);
S32 view_tile_size(const View* view);
