#pragma once
#include "message.h"
#include "input.h"

#if 0
typedef struct View_T {
  Model_T* model;
  V2S cursor;
} View_T;

extern View_T* const View_state;
#endif

extern V2S sim_cursor;
extern Model_T* sim_model;

// called from audio thread
Void View_init();
Void View_step(F32* audio_out, Index frames);

// called from render thread
Void View_update(const Input_Frame* input);
