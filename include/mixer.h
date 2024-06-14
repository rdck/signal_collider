/*******************************************************************************
 * AUDIO MIXER INTERFACE
 ******************************************************************************/

#ifndef MIXER_H
#define MIXER_H

#include "prelude.h"

Void mixer_init();

Void mixer_step(F32* buffer, S32 frames);

// data in source buffer is expected to remain constant for the duration of playback
Void mixer_play(const F32* source, S32 length);

#endif // MIXER_H
