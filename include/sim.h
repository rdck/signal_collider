/*******************************************************************************
 * sim.h - small step semantics
 ******************************************************************************/

#pragma once

#include "message.h"

#define SIM_HISTORY 0x100

extern Model sim_history[SIM_HISTORY];
extern MessageQueue alloc_queue;
extern MessageQueue free_queue;
extern MessageQueue input_queue;

// called from audio thread
Void sim_init();
Void sim_step(F32* audio_out, Index frames);
