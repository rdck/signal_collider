/*******************************************************************************
 * render.h - renderer
 ******************************************************************************/

#pragma once

#include "model.h"

Void render_init(V2S dimensions);
Void render_frame(const Model* m, V2F camera);

V2S render_tile_size();
