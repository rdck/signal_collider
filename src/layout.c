#include <SDL3/SDL_log.h>
#include "layout.h"

typedef struct DrawArena {
  DrawRectangle* buffer;
  Index head;
} DrawArena;

typedef struct InteractionArena {
  InteractionRectangle* buffer;
  Index head;
} InteractionArena;

static Void draw_rectangle(DrawArena* arena, DrawRectangle rectangle)
{
  if (arena->head < LAYOUT_DRAW_RECTANGLES) {
    arena->buffer[arena->head] = rectangle;
    arena->head += 1;
  } else {
    SDL_Log("layout draw overflow");
  }
}

static Void interaction_rectangle(InteractionArena* arena, InteractionRectangle rectangle)
{
  if (arena->head < LAYOUT_INTERACTION_RECTANGLES) {
    arena->buffer[arena->head] = rectangle;
    arena->head += 1;
  } else {
    SDL_Log("layout interaction overflow");
  }
}

Void layout(
    InteractionRectangle* interaction,
    DrawRectangle* draw,
    const UIState* ui,
    const LayoutParameters* parameters)
{
  // initialize output arenas
  DrawArena draw_arena = {
    .buffer = draw,
    .head = 0,
  };
  InteractionArena interaction_arena = {
    .buffer = interaction,
    .head = 0,
  };
}
