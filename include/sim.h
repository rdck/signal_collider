/*******************************************************************************
 * sim.h - audio thread
 ******************************************************************************/

#pragma once

#include "model.h"

#define SIM_HISTORY 0x20

extern ModelGraph sim_history[SIM_HISTORY];
extern DSPState dsp_history[SIM_HISTORY];

// called from audio thread
Void sim_init();
Void sim_step(F32* audio_out, Index frames);
