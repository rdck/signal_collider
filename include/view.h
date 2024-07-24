/*******************************************************************************
 * view.h - model of user interface
 ******************************************************************************/

#pragma once

#include "prelude.h"

#define CONSOLE_BUFFER 0x200

typedef enum ViewState {
  VIEW_STATE_GRID,
  VIEW_STATE_CONSOLE,
  VIEW_STATE_CARDINAL,
} ViewState;

extern ViewState view_state;
extern V2S cursor;
extern Char console[CONSOLE_BUFFER];
extern Index console_head;
