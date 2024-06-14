/*******************************************************************************
 * input.h - mouse and keyboard input
 ******************************************************************************/

#pragma once

// If there are more events than this in a frame, they are ignored.
#define Input_MAX_EVENTS 32

typedef enum {
  Input_KEYCODE_NONE = 0,
  Input_KEYCODE_MOUSE_LEFT,
  Input_KEYCODE_MOUSE_RIGHT,
  Input_KEYCODE_ESCAPE,
  Input_KEYCODE_ENTER,
  Input_KEYCODE_BACKSPACE,
  Input_KEYCODE_TAB,
  Input_KEYCODE_F1,
  Input_KEYCODE_F2,
  Input_KEYCODE_F3,
  Input_KEYCODE_F4,
  Input_KEYCODE_F5,
  Input_KEYCODE_F6,
  Input_KEYCODE_F7,
  Input_KEYCODE_F8,
  Input_KEYCODE_F9,
  Input_KEYCODE_F10,
  Input_KEYCODE_F11,
  Input_KEYCODE_F12,
  Input_KEYCODE_SPACE,
  Input_KEYCODE_ARROW_LEFT,
  Input_KEYCODE_ARROW_RIGHT,
  Input_KEYCODE_ARROW_UP,
  Input_KEYCODE_ARROW_DOWN,
  Input_KEYCODE_0,
  Input_KEYCODE_1,
  Input_KEYCODE_2,
  Input_KEYCODE_3,
  Input_KEYCODE_4,
  Input_KEYCODE_5,
  Input_KEYCODE_6,
  Input_KEYCODE_7,
  Input_KEYCODE_8,
  Input_KEYCODE_9,
  Input_KEYCODE_A,
  Input_KEYCODE_B,
  Input_KEYCODE_C,
  Input_KEYCODE_D,
  Input_KEYCODE_E,
  Input_KEYCODE_F,
  Input_KEYCODE_G,
  Input_KEYCODE_H,
  Input_KEYCODE_I,
  Input_KEYCODE_J,
  Input_KEYCODE_K,
  Input_KEYCODE_L,
  Input_KEYCODE_M,
  Input_KEYCODE_N,
  Input_KEYCODE_O,
  Input_KEYCODE_P,
  Input_KEYCODE_Q,
  Input_KEYCODE_R,
  Input_KEYCODE_S,
  Input_KEYCODE_T,
  Input_KEYCODE_U,
  Input_KEYCODE_V,
  Input_KEYCODE_W,
  Input_KEYCODE_X,
  Input_KEYCODE_Y,
  Input_KEYCODE_Z,
  Input_KEYCODE_PLUS,
  Input_KEYCODE_MINUS,
  Input_KEYCODE_CARDINAL,
} Input_KeyCode;

typedef enum {
  Input_KEYSTATE_UP = 0,
  Input_KEYSTATE_DOWN = 1,
} Input_KeyState;

typedef struct {
  Input_KeyCode code;
  Input_KeyState state;
} Input_KeyEvent;

typedef struct {
  Input_KeyEvent events[Input_MAX_EVENTS];
  Char chars[Input_MAX_EVENTS];
  V2S mouse;
} Input_Frame;
