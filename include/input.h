/*******************************************************************************
 * input.h - mouse and keyboard input
 ******************************************************************************/

#pragma once

#include "linear_algebra.h"

// If there are more events than this in a frame, they are ignored.
#define MAX_INPUT_EVENTS 32

typedef enum {
  KEYCODE_NONE = 0,
  KEYCODE_MOUSE_LEFT,
  KEYCODE_MOUSE_RIGHT,
  KEYCODE_ESCAPE,
  KEYCODE_ENTER,
  KEYCODE_BACKSPACE,
  KEYCODE_TAB,
  KEYCODE_F1,
  KEYCODE_F2,
  KEYCODE_F3,
  KEYCODE_F4,
  KEYCODE_F5,
  KEYCODE_F6,
  KEYCODE_F7,
  KEYCODE_F8,
  KEYCODE_F9,
  KEYCODE_F10,
  KEYCODE_F11,
  KEYCODE_F12,
  KEYCODE_SPACE,
  KEYCODE_ARROW_LEFT,
  KEYCODE_ARROW_RIGHT,
  KEYCODE_ARROW_UP,
  KEYCODE_ARROW_DOWN,
  KEYCODE_0,
  KEYCODE_1,
  KEYCODE_2,
  KEYCODE_3,
  KEYCODE_4,
  KEYCODE_5,
  KEYCODE_6,
  KEYCODE_7,
  KEYCODE_8,
  KEYCODE_9,
  KEYCODE_A,
  KEYCODE_B,
  KEYCODE_C,
  KEYCODE_D,
  KEYCODE_E,
  KEYCODE_F,
  KEYCODE_G,
  KEYCODE_H,
  KEYCODE_I,
  KEYCODE_J,
  KEYCODE_K,
  KEYCODE_L,
  KEYCODE_M,
  KEYCODE_N,
  KEYCODE_O,
  KEYCODE_P,
  KEYCODE_Q,
  KEYCODE_R,
  KEYCODE_S,
  KEYCODE_T,
  KEYCODE_U,
  KEYCODE_V,
  KEYCODE_W,
  KEYCODE_X,
  KEYCODE_Y,
  KEYCODE_Z,
  KEYCODE_PLUS,
  KEYCODE_MINUS,
  KEYCODE_CARDINAL,
} KeyCode;

typedef enum {
  KEYSTATE_UP = 0,
  KEYSTATE_DOWN = 1,
} KeyState;

typedef struct {
  KeyCode code;
  KeyState state;
} KeyEvent;

typedef struct {
  KeyEvent events[MAX_INPUT_EVENTS];
  Char chars[MAX_INPUT_EVENTS];
  V2S mouse;
} InputFrame;
