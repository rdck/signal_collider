#include <limits.h>
#include "view.h"
#include "comms.h"
#include "stb_truetype.h"
#include "font.ttf.h"

#define FONT_SIZE_LARGE 40 // in pixels
#define FONT_SIZE_SMALL 20 // in pixels

static ValueTag value_table[CHAR_MAX + 1] = {
  [ '!' ]       = VALUE_BANG       ,
  [ '+' ]       = VALUE_ADD        ,
  [ '-' ]       = VALUE_SUB        ,
  [ '*' ]       = VALUE_MUL        ,
  [ '/' ]       = VALUE_DIV        ,
  [ '=' ]       = VALUE_EQUAL      ,
  [ '>' ]       = VALUE_GREATER    ,
  [ '<' ]       = VALUE_LESSER     ,
  [ '&' ]       = VALUE_AND        ,
  [ '|' ]       = VALUE_OR         ,
  [ 'a' ]       = VALUE_ALTER      ,
  [ 'b' ]       = VALUE_BOTTOM     ,
  [ 'c' ]       = VALUE_CLOCK      ,
  [ 'd' ]       = VALUE_DELAY      ,
  [ 'h' ]       = VALUE_HOP        ,
  [ 'i' ]       = VALUE_INTERFERE  ,
  [ 'j' ]       = VALUE_JUMP       ,
  [ 'l' ]       = VALUE_LOAD       ,
  [ 'm' ]       = VALUE_MULTIPLEX  ,
  [ 'n' ]       = VALUE_NOTE       ,
  [ 'o' ]       = VALUE_ODDMENT    ,
  [ 'q' ]       = VALUE_QUOTE      ,
  [ 'r' ]       = VALUE_RANDOM     ,
  [ 's' ]       = VALUE_STORE      ,
  [ 't' ]       = VALUE_TOP        ,
  [ 'x' ]       = VALUE_SAMPLER    ,
  [ 'y' ]       = VALUE_SYNTH      ,
  [ 'z' ]       = VALUE_MIDI       ,
};

static Void bake_font(Font* font, S32 font_size)
{
  stbtt_fontinfo font_info = {0};
  const S32 init_result = stbtt_InitFont(&font_info, font_hack, 0);
  ASSERT(init_result);

  const F32 scale = stbtt_ScaleForPixelHeight(&font_info, (F32) font_size);

  // vertical metrics
  S32 ascent = 0;
  S32 descent = 0;
  S32 line_gap = 0;
  stbtt_GetFontVMetrics(&font_info, &ascent, &descent, &line_gap);
  const S32 scaled_ascent = (S32) (scale * ascent) + 1;

  // horizontal metrics
  S32 raw_advance = 0;
  S32 left_side_bearing = 0;
  stbtt_GetCodepointHMetrics(&font_info, 'M', &raw_advance, &left_side_bearing);

  // dimensions
  font->glyph.x = (S32) (scale * raw_advance) + 1;
  font->glyph.y = font_size;
  const S32 char_area = font->glyph.x * font->glyph.y;
  const V2S graph = v2s_mul(v2s(ASCII_X, ASCII_Y), font->glyph);

  // clear atlas
  font->bitmap = SDL_calloc(1, ASCII_AREA * char_area);
  ASSERT(font->bitmap);

  S32 w = 0;
  S32 h = 0;
  S32 xoff = 0;
  S32 yoff = 0;

  // iterate through ASCII characters, rendering bitmaps to atlas
  for (Char c = MIN_CHAR; c <= MAX_CHAR; c++) {

    Byte* const bitmap = stbtt_GetCodepointBitmap(
        &font_info,
        scale, scale,
        c,
        &w, &h,
        &xoff, &yoff
        );
    ASSERT(w <= font->glyph.x);
    ASSERT(h <= font->glyph.y);

    const V2S p = view_atlas_coordinate(c);
    const V2S o = v2s_mul(p, font->glyph);
    for (S32 y = 0; y < h; y++) {
      for (S32 x = 0; x < w; x++) {
        const S32 x0 = o.x + xoff + x;
        const S32 y0 = o.y + scaled_ascent + yoff + y;
        if (view_validate_atlas_coordinate(v2s(x0, y0), graph)) {
          font->bitmap[y0 * graph.x + x0] = bitmap[y * w + x];
        }
      }
    }

    stbtt_FreeBitmap(bitmap, NULL);

  }

#ifdef DEBUG_ATLAS_LINES
  // draw debug line separators in the atlas
  for (Index y = 0; y < ASCII_Y; y++) {
    for (Index x = 0; x < ASCII_X * font->glyph.x; x++) {
      font->bitmap[y * font->glyph.y * graph.x + x] = 0xFF;
    }
  }
  for (Index x = 0; x < ASCII_X; x++) {
    for (Index y = 0; y < ASCII_Y * font->glyph.y; y++) {
      font->bitmap[y * graph.x + x * font->glyph.x] = 0xFF;
    }
  }
#endif

#ifdef DEBUG_ATLAS
  // write the atlas to a file for debugging
  stbi_write_png(
      "font_atlas.png",
      graph.x,
      graph.y,
      1,
      font->bitmap,
      graph.x
      );
#endif
}

static Void input_value(V2S cursor, Value value)
{
  const ControlMessage message = control_message_write(cursor, value);
  ATOMIC_QUEUE_ENQUEUE(ControlMessage)(&control_queue, message);
}

static S32 character_literal(Char c)
{
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'A' && c <= 'Z') {
    return c - 'A' + 10;
  } else {
    return -1;
  }
}

static Void update_cursor(View* view, Direction d)
{
  const V2S next = add_unit_vector(view->cursor, d);
  if (valid_point(next)) {
    view->cursor = next;
  }
}

Void view_init(View* view)
{
  memset(view, 0, sizeof(*view));
  bake_font(&view->font_small, FONT_SIZE_SMALL);
  bake_font(&view->font_large, FONT_SIZE_LARGE);
}

Void view_event(View* view, const SDL_Event* event)
{
  const SDL_Keycode keycode = event->key.key;

  switch (view->interaction) {

    case INTERACTION_NONE:
      {

        switch (event->type) {

          case SDL_EVENT_TEXT_INPUT:
            {
              // Presumably there are cases where this string has length > 1,
              // but I'm not sure what those cases are.
              const Char* c = event->text.text;
              while (*c) {
                const S32 literal = character_literal(*c);
                const Value value = {
                  .tag = value_table[*c],
                  .powered = true,
                };
                if (value.tag != VALUE_NONE) {
                  input_value(view->cursor, value);
                } else if (literal >= 0) {
                  input_value(view->cursor, value_literal(literal));
                }
                c += 1;
              }
            } break;

          case SDL_EVENT_KEY_DOWN:
            {
              switch (keycode) {
                // arrow keys
                case SDLK_LEFT:
                  update_cursor(view, DIRECTION_WEST);
                  break;
                case SDLK_RIGHT:
                  update_cursor(view, DIRECTION_EAST);
                  break;
                case SDLK_UP:
                  update_cursor(view, DIRECTION_NORTH);
                  break;
                case SDLK_DOWN:
                  update_cursor(view, DIRECTION_SOUTH);
                  break;
                case SDLK_SPACE:
                  ATOMIC_QUEUE_ENQUEUE(ControlMessage)(&control_queue, control_message_power(view->cursor));
                  break;
                case SDLK_BACKSPACE:
                  input_value(view->cursor, value_none);
                  break;
                default: { }
              }
            } break;

          case SDL_EVENT_MOUSE_BUTTON_DOWN:
            {
              if (event->button.button == SDL_BUTTON_LEFT) {
                view->interaction = INTERACTION_CAMERA;
              }
            } break;


          default: { }

        }

      } break;

    case INTERACTION_CAMERA:
      {
        switch (event->type) {

          case SDL_EVENT_MOUSE_BUTTON_UP:
            {
              view->interaction = INTERACTION_NONE;
            } break;

          case SDL_EVENT_MOUSE_MOTION:
            {
              const V2F relative = { event->motion.xrel, event->motion.yrel };
              const S32 tile_side = view_tile_size(view);
              const V2F tile = { (F32) tile_side, (F32) tile_side };
              view->camera = v2f_sub(view->camera, v2f_div(relative, tile));
            } break;

          default: { }

        }
      } break;

    default: { }

  }

}

Void view_step(View* view)
{
}

V2S view_atlas_coordinate(Char c)
{
  V2S out;
  out.x = c % ASCII_X;
  out.y = c / ASCII_X;
  return out;
}

Bool view_validate_atlas_coordinate(V2S c, V2S d)
{
  const Bool x = c.x >= 0 && c.x < d.x;
  const Bool y = c.y >= 0 && c.y < d.y;
  return x && y;
}

S32 view_panel_width(const View* view)
{
  return PANEL_CHARACTERS * view->font_small.glyph.x + 2 * PADDING;
}

S32 view_menu_height(const View* view)
{
  return view->font_small.glyph.y + 2 * PADDING;
}

S32 view_tile_size(const View* view)
{
  const V2S glyph = view->font_large.glyph;
  return MAX(glyph.x, glyph.y);
}
