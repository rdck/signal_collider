#include <limits.h>
#include <ctype.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_log.h>
#include "view.h"
#include "comms.h"
#include "font.ttf.h"
#include "stb_truetype.h"
#include "dr_wav.h"

#ifdef DEBUG_ATLAS
#include "stb_image_write.h"
#endif

#define FONT_SIZE_LARGE 32 // in pixels
#define FONT_SIZE_SMALL 20 // in pixels

#define ASCII_X 16
#define ASCII_Y 8
#define ASCII_AREA (ASCII_X * ASCII_Y)
#define MIN_CHAR '!'
#define MAX_CHAR '~'
#define PADDING 6
#define PANEL_CHARACTERS 44
#define OPERATOR_DESCRIPTION_LINES 3
#define EMPTY_CHARACTER '.'
#define COLOR_CHANNELS 4

typedef struct UIContext {
  V2S origin;
  V2S bounds;
  V2S cursor;
} UIContext;

// @rdk: rename
typedef struct SampleLoadData {
  View* view;
  S32 index;
} SampleLoadData;

// @rdk: rename
typedef struct FileLoaded {
  S32 index;
} FileLoaded;

#define COLOR_STRUCTURE(rv, gv, bv, av) { .r = rv, .g = gv, .b = bv, .a = av }
static const SDL_Color color_white        = COLOR_STRUCTURE(0xFF, 0xFF, 0xFF, 0xFF);
static const SDL_Color color_empty        = COLOR_STRUCTURE(0xFF, 0xFF, 0xFF, 0x80);
static const SDL_Color color_literal      = COLOR_STRUCTURE(0x80, 0x80, 0xFF, 0xFF);
static const SDL_Color color_pulse        = COLOR_STRUCTURE(0x80, 0xFF, 0x80, 0xFF);
static const SDL_Color color_unpowered    = COLOR_STRUCTURE(0xA0, 0xA0, 0xA0, 0xFF);
static const SDL_Color color_cursor       = COLOR_STRUCTURE(0x80, 0x80, 0xFF, 0x40);
static const SDL_Color color_panel        = COLOR_STRUCTURE(0x10, 0x10, 0x10, 0xFF);
static const SDL_Color color_menu         = COLOR_STRUCTURE(0x20, 0x20, 0x20, 0xFF);
static const SDL_Color color_outline      = COLOR_STRUCTURE(0xA0, 0xA0, 0xA0, 0xFF);
static const SDL_Color color_input        = COLOR_STRUCTURE(0x60, 0x70, 0x80, 0x80);
static const SDL_Color color_sample_slot  = COLOR_STRUCTURE(0x40, 0x40, 0x40, 0xFF);
static const SDL_Color color_dialog       = COLOR_STRUCTURE(0xFF, 0xFF, 0xFF, 0x40);

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

static const Char* noun_table[VALUE_CARDINAL] = {
  [ VALUE_NONE      ] = "EMPTY TILE",
  [ VALUE_LITERAL   ] = "NUMBER",
  [ VALUE_BANG      ] = "BANG",
  [ VALUE_ADD       ] = "ADDER",
  [ VALUE_SUB       ] = "SUBTRACTOR",
  [ VALUE_MUL       ] = "MULTIPLIER",
  [ VALUE_DIV       ] = "DIVIDER",
  [ VALUE_EQUAL     ] = "EQUALITY",
  [ VALUE_GREATER   ] = "COMPARATOR",
  [ VALUE_LESSER    ] = "COMPARATOR",
  [ VALUE_AND       ] = "AND",
  [ VALUE_OR        ] = "OR",
  [ VALUE_ALTER     ] = "INTERPOLATOR",
  [ VALUE_BOTTOM    ] = "MINIMIZER",
  [ VALUE_CLOCK     ] = "CLOCK",
  [ VALUE_DELAY     ] = "DELAY",
  [ VALUE_E         ] = "E",
  [ VALUE_F         ] = "F",
  [ VALUE_G         ] = "G",
  [ VALUE_HOP       ] = "HOPPER",
  [ VALUE_INTERFERE ] = "INTERFERENCE",
  [ VALUE_JUMP      ] = "JUMPER",
  [ VALUE_K         ] = "K",
  [ VALUE_LOAD      ] = "LOADER",
  [ VALUE_MULTIPLEX ] = "MULTIPLEXER",
  [ VALUE_NOTE      ] = "QUANTIZER",
  [ VALUE_ODDMENT   ] = "ODDMENT",
  [ VALUE_P         ] = "P",
  [ VALUE_QUOTE     ] = "QUOTER",
  [ VALUE_RANDOM    ] = "RANDOMIZER",
  [ VALUE_STORE     ] = "STORER",
  [ VALUE_TOP       ] = "MAXIMIZER",
  [ VALUE_U         ] = "U",
  [ VALUE_V         ] = "V",
  [ VALUE_W         ] = "W",
  [ VALUE_SAMPLER   ] = "SAMPLER",
  [ VALUE_SYNTH     ] = "SYNTHESIZER",
  [ VALUE_MIDI      ] = "MIDI",
};

static const Char* description_table[VALUE_CARDINAL] = {
  [ VALUE_NONE      ] = "Nothingness.",
  [ VALUE_LITERAL   ] = "",
  [ VALUE_BANG      ] = "Activates adjacent operators.",
  [ VALUE_ADD       ] =
    "Adds " ATTRIBUTE_LEFT_ADDEND " to " ATTRIBUTE_RIGHT_ADDEND ".",
  [ VALUE_SUB       ] =
    "Subtracts " ATTRIBUTE_SUBTRAHEND " from " ATTRIBUTE_MINUEND ".",
  [ VALUE_MUL       ] =
    "Multiplies " ATTRIBUTE_MULTIPLIER " by " ATTRIBUTE_MULTIPLICAND ".",
  [ VALUE_DIV       ] =
    "Divides " ATTRIBUTE_DIVIDEND " by " ATTRIBUTE_DIVISOR ".",
  [ VALUE_EQUAL     ] =
    "Produces a bang when " ATTRIBUTE_LEFT_COMPARATE " and " ATTRIBUTE_RIGHT_COMPARATE " are equal.",
  [ VALUE_GREATER   ] =
    "Produces a bang when " ATTRIBUTE_LEFT_COMPARATE " is greater than " ATTRIBUTE_RIGHT_COMPARATE ".",
  [ VALUE_LESSER    ] =
    "Produces a bang when " ATTRIBUTE_LEFT_COMPARATE " is less than " ATTRIBUTE_RIGHT_COMPARATE ".",
  [ VALUE_AND       ] =
    "Performs a bitwise AND when "
      ATTRIBUTE_LEFT_CONJUNCT " and " ATTRIBUTE_RIGHT_CONJUNCT " are both numbers. "
      "Produces a bang when both " ATTRIBUTE_LEFT_CONJUNCT " and " ATTRIBUTE_RIGHT_CONJUNCT
      " are nonempty, otherwise.",
  [ VALUE_OR        ] =
    "Performs a bitwise OR when "
      ATTRIBUTE_LEFT_CONJUNCT " and " ATTRIBUTE_RIGHT_CONJUNCT " are both numbers. "
      "Produces a bang when either " ATTRIBUTE_LEFT_CONJUNCT " or " ATTRIBUTE_RIGHT_CONJUNCT
      " is nonempty, otherwise.",
  [ VALUE_ALTER     ] =
    "Interpolates between " ATTRIBUTE_MINIMUM " and " ATTRIBUTE_MAXIMUM " by " ATTRIBUTE_TIME ".",
  [ VALUE_BOTTOM    ] =
    "Selects the lesser of " ATTRIBUTE_LEFT_COMPARATE " and " ATTRIBUTE_RIGHT_COMPARATE ".",
  [ VALUE_CLOCK     ] =
    "A clock running at rate " ATTRIBUTE_RATE ", modulo " ATTRIBUTE_DIVISOR ".",
  [ VALUE_DELAY     ] =
    "Produces a bang when the equivalent clock would be zero.",
  [ VALUE_E         ] = "E",
  [ VALUE_F         ] = "F",
  [ VALUE_G         ] = "G",
  [ VALUE_HOP       ] = "Transports values west to east.",
  [ VALUE_INTERFERE ] =
    "Write to program memory at relative coordinate <" ATTRIBUTE_X ", " ATTRIBUTE_Y ">.",
  [ VALUE_JUMP      ] = "Transports values north to south.",
  [ VALUE_K         ] = "K",
  [ VALUE_LOAD      ] = "Loads value from register " ATTRIBUTE_REGISTER ".",
  [ VALUE_MULTIPLEX ] =
    "Reads from program memory at relative coordinate <" ATTRIBUTE_X ", " ATTRIBUTE_Y ">.",
  [ VALUE_NOTE      ] =
    "Computes the semitone value of note " ATTRIBUTE_INDEX " of the major scale.",
  [ VALUE_ODDMENT   ] =
    "Divides " ATTRIBUTE_DIVIDEND " by " ATTRIBUTE_DIVISOR " and takes the remainder.",
  [ VALUE_P         ] = "P",
  [ VALUE_QUOTE     ] = "Self reference!",
  [ VALUE_RANDOM    ] =
    "Chooses a random value modulo " ATTRIBUTE_DIVISOR " at rate " ATTRIBUTE_RATE ".",
  [ VALUE_STORE     ] =
    "Stores " ATTRIBUTE_INPUT " into register " ATTRIBUTE_REGISTER ".",
  [ VALUE_TOP       ] =
    "Selects the greater of " ATTRIBUTE_LEFT_COMPARATE " and " ATTRIBUTE_RIGHT_COMPARATE ".",
  [ VALUE_U         ] = "U",
  [ VALUE_V         ] = "V",
  [ VALUE_W         ] = "W",
  [ VALUE_SAMPLER   ] = "A simple sampler.",
  [ VALUE_SYNTH     ] = "A simple sine wave synthesizer.",
  [ VALUE_MIDI      ] = "Sends MIDI.",
};

static Char representation_table[VALUE_CARDINAL] = {
  [ VALUE_LITERAL       ] = 0,
  [ VALUE_BANG          ] = '!',
  [ VALUE_ADD           ] = '+',
  [ VALUE_SUB           ] = '-',
  [ VALUE_MUL           ] = '*',
  [ VALUE_DIV           ] = '/',
  [ VALUE_EQUAL         ] = '=',
  [ VALUE_GREATER       ] = '>',
  [ VALUE_LESSER        ] = '<',
  [ VALUE_AND           ] = '&',
  [ VALUE_OR            ] = '|',
  [ VALUE_ALTER         ] = 'A',
  [ VALUE_BOTTOM        ] = 'B',
  [ VALUE_CLOCK         ] = 'C',
  [ VALUE_DELAY         ] = 'D',
  [ VALUE_HOP           ] = 'H',
  [ VALUE_INTERFERE     ] = 'I',
  [ VALUE_JUMP          ] = 'J',
  [ VALUE_LOAD          ] = 'L',
  [ VALUE_MULTIPLEX     ] = 'M',
  [ VALUE_NOTE          ] = 'N',
  [ VALUE_ODDMENT       ] = 'O',
  [ VALUE_QUOTE         ] = 'Q',
  [ VALUE_RANDOM        ] = 'R',
  [ VALUE_STORE         ] = 'S',
  [ VALUE_TOP           ] = 'T',
  [ VALUE_SAMPLER       ] = 'X',
  [ VALUE_SYNTH         ] = 'Y',
  [ VALUE_MIDI          ] = 'Z',
};

static Void SDL_SetRenderDrawColorStruct(SDL_Renderer* r, SDL_Color color)
{
  SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
}

// @rdk: rename
static V2S view_atlas_coordinate(Char c)
{
  V2S out;
  out.x = c % ASCII_X;
  out.y = c / ASCII_X;
  return out;
}

static Bool view_validate_atlas_coordinate(V2S c, V2S d)
{
  const Bool x = c.x >= 0 && c.x < d.x;
  const Bool y = c.y >= 0 && c.y < d.y;
  return x && y;
}

static S32 view_panel_width(const View* view)
{
  return PANEL_CHARACTERS * view->font_small.glyph.x + 2 * PADDING;
}

static S32 view_menu_height(const View* view)
{
  return view->font_small.glyph.y + 2 * PADDING;
}

static S32 view_tile_size(const View* view)
{
  const V2S glyph = view->font_large.glyph;
  return MAX(glyph.x, glyph.y);
}

static V2F world_to_screen(const View* view, V2F camera, V2S point)
{
  const S32 tile_width = view_tile_size(view);
  const V2F tile = { (F32) tile_width, (F32) tile_width };
  const V2F relative = v2f_sub(v2f_of_v2s(point), camera);
  const V2F offset = {
    (F32) view_panel_width(view),
    (F32) view_menu_height(view),
  };
  return v2f_add(offset, v2f_mul(relative, tile));
}

static Void draw_world_highlight(View* view, SDL_Color color, V2S point)
{
  const V2F origin = world_to_screen(view, view->camera, point);
  const S32 tile_width = view_tile_size(view);

  // screen coordinates to fill
  SDL_FRect destination;
  destination.x = origin.x;
  destination.y = origin.y;
  destination.w = (F32) tile_width;
  destination.h = (F32) tile_width;

  SDL_SetRenderDrawColor(view->renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(view->renderer, &destination);
}

static Void load_font(Font* font, SDL_Renderer* renderer, S32 font_size)
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
  Byte* const atlas = SDL_calloc(1, ASCII_AREA * char_area);
  ASSERT(atlas);

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
          atlas[y0 * graph.x + x0] = bitmap[y * w + x];
        }
      }
    }

    stbtt_FreeBitmap(bitmap, NULL);

  }

#ifdef DEBUG_ATLAS_LINES
  // draw debug line separators in the atlas
  for (Index y = 0; y < ASCII_Y; y++) {
    for (Index x = 0; x < ASCII_X * font->glyph.x; x++) {
      atlas[y * font->glyph.y * graph.x + x] = 0xFF;
    }
  }
  for (Index x = 0; x < ASCII_X; x++) {
    for (Index y = 0; y < ASCII_Y * font->glyph.y; y++) {
      atlas[y * graph.x + x * font->glyph.x] = 0xFF;
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
      atlas,
      graph.x
      );
#endif

  Byte* const channels = SDL_malloc(COLOR_CHANNELS * ASCII_AREA * char_area);
  for (Index y = 0; y < graph.y; y++) {
    for (Index x = 0; x < graph.x; x++) {
      const Byte alpha = atlas[y * graph.x + x];
      channels[COLOR_CHANNELS * (y * graph.x + x) + 0] = 0xFF;
      channels[COLOR_CHANNELS * (y * graph.x + x) + 1] = 0xFF;
      channels[COLOR_CHANNELS * (y * graph.x + x) + 2] = 0xFF;
      channels[COLOR_CHANNELS * (y * graph.x + x) + 3] = alpha;
    }
  }

  SDL_Surface* const surface = SDL_CreateSurfaceFrom(
      ASCII_X * font->glyph.x,                  // width
      ASCII_Y * font->glyph.y,                  // height
      SDL_PIXELFORMAT_RGBA32,                   // pixel format
      channels,                                 // pixel data
      COLOR_CHANNELS * ASCII_X * font->glyph.x  // pitch
      );

  // create gpu texture
  font->texture = SDL_CreateTextureFromSurface(renderer, surface);
  ASSERT(font->texture);

  SDL_DestroySurface(surface);
  SDL_free(channels);
}

static Void draw_character(SDL_Renderer* renderer, const Font* font, V2F origin, Char c)
{
  const V2S uv = view_atlas_coordinate(c);

  // texture coordinates for the glyph
  SDL_FRect source;
  source.x = (F32) (uv.x * font->glyph.x);
  source.y = (F32) (uv.y * font->glyph.y);
  source.w = (F32) font->glyph.x;
  source.h = (F32) font->glyph.y;

  // screen coordinates to fill
  SDL_FRect destination;
  destination.x = origin.x;
  destination.y = origin.y;
  destination.w = (F32) font->glyph.x;
  destination.h = (F32) font->glyph.y;

  // draw
  SDL_RenderTexture(renderer, font->texture, &source, &destination);
}

static Void draw_world_character(View* view, SDL_Color color, V2S point, Char c)
{
  const S32 tile_width = view_tile_size(view);
  const V2F origin = world_to_screen(view, view->camera, point);
  const F32 delta = (tile_width - view->font_large.glyph.x) / 2.f;
  const V2F screen = { origin.x + delta, origin.y };
  // @rdk: pull color state out
  SDL_SetTextureColorMod(view->font_large.texture, color.r, color.g, color.b);
  SDL_SetTextureAlphaMod(view->font_large.texture, color.a);
  draw_character(view->renderer, &view->font_large, screen, c);
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

static Void render_waveform(View* view, S32 index, Sound sound)
{
  ASSERT(index >= 0);
  ASSERT(index < MODEL_RADIX);

  V2S window = {0};
  if (SDL_GetRenderOutputSize(view->renderer, &window.x, &window.y) == false) {
    SDL_Log("Failed to get render output size: %s", SDL_GetError());
  }

  // @rdk: pull this out
  const S32 sample_x = view_panel_width(view);
  const S32 sample_y = (window.y - view_menu_height(view)) / MODEL_RADIX;

  Byte* const channels = SDL_calloc(1, sample_x * sample_y);
  ASSERT(channels);

  const Index frames_per_pixel = sound.frames / sample_x;
  for (Index i = 0; i < sample_x; i++) {

    // compute max for period
    F32 max = 0.f;
    const Index start = frames_per_pixel * i;
    for (Index j = 0; j < frames_per_pixel; j++) {
      const F32 l = sound.samples[2 * (start + j) + 0];
      const F32 r = sound.samples[2 * (start + j) + 1];
      max = MAX(max, MAX(l, r));
    }

    // fill line
    const Index line_height = MIN(sample_y, (Index) (max * sample_y));
    const Index top = (sample_y - line_height) / 2;
    for (Index j = 0; j < line_height; j++) {
      const Index k = top + j;
      channels[k * sample_x + i] = 0x15;
    }

  }

  SDL_Surface* const surface = SDL_CreateSurfaceFrom(
      sample_x,               // width
      sample_y,               // height
      SDL_PIXELFORMAT_RGB332, // pixel format
      channels,               // pixel data
      sample_x                // pitch
      );
  ASSERT(surface);

  // @rdk: free previous texture, if one exists
  // upload texture
  view->waveforms[index].dimensions = v2s(sample_x, sample_y);
  view->waveforms[index].texture = SDL_CreateTextureFromSurface(view->renderer, surface);
  if (view->waveforms[index].texture == NULL) {
    SDL_Log("Failed to create texture: %s", SDL_GetError());
  }

}

static Void SDLCALL open_sample_callback(Void* user_data, const Char* const* file_list, S32 filter)
{
  UNUSED_PARAMETER(filter);
  SampleLoadData* const data = user_data;
  if (file_list) {
    const Char* const path = file_list[0];
    if (path) {
      FileLoaded* const loaded = SDL_malloc(sizeof(*loaded));
      loaded->index = data->index;
      const Bool load_file_result = SDL_LoadFileAsync(path, data->view->io_queue, loaded);
      if (load_file_result == false) {
        SDL_Log("SDL_LoadFileAsync failed: %s", SDL_GetError());
        SDL_free(loaded);
      }
    }
  }
  data->view->interaction = INTERACTION_NONE;
  SDL_free(data);
}

static Void draw_ui_text(View* view, UIContext* context, const Char* text)
{
  const Char* head = text;
  while (*head) {

    // skip whitespace
    while (isspace(*head)) {
      if (*head == '\n') {
        context->cursor.x = 0;
        context->cursor.y += 1;
      } else if (*head == ' ') {
        context->cursor.x += 1;
      }
      head += 1;
    }

    // measure word length
    const Char* tail = head;
    while (*tail && isspace(*tail) == false) {
      tail += 1;
    }
    const S32 word_length = (S32) (tail - head);

    // advance line if word will overflow
    if (context->cursor.x + word_length > context->bounds.x) {
      context->cursor.x = 0;
      context->cursor.y += 1;
    }

    // draw word
    for (S32 i = 0; i < word_length; i++) {
      const V2S offset = v2s_mul(context->cursor, view->font_small.glyph);
      const V2F origin = v2f_of_v2s(v2s_add(context->origin, offset));
      draw_character(view->renderer, &view->font_small, origin, head[i]);
      context->cursor.x += 1;
    }

    // advance
    head += word_length;

  }
}


Void view_init(View* view, SDL_Renderer* renderer, F32 scale)
{
  memset(view, 0, sizeof(*view));

  view->renderer = renderer;
  view->scale = scale;
  view->io_queue = SDL_CreateAsyncIOQueue();

  // set blend mode
  if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND) == false) {
    SDL_Log("Failed to set render draw blend mode: %s", SDL_GetError());
  }

  load_font(&view->font_small, renderer, (S32) (scale * FONT_SIZE_SMALL));
  load_font(&view->font_large, renderer, (S32) (scale * FONT_SIZE_LARGE));
}

Void view_event(View* view, const SDL_Event* event)
{
  // calculate window size
  V2S window = {0};
  if (SDL_GetRenderOutputSize(view->renderer, &window.x, &window.y) == false) {
    SDL_Log("Failed to get render output size: %s", SDL_GetError());
  }

  // shorthand
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
              const SDL_Point mouse = {
                (S32) event->button.x,
                (S32) event->button.y,
              };
              const SDL_Rect program_area = {
                .x = view_panel_width(view),
                .y = view_menu_height(view),
                .w = window.x - 2 * view_panel_width(view),
                .h = window.y - view_menu_height(view),
              };
              const SDL_Rect sample_area = {
                .x = 0,
                .y = view_menu_height(view),
                .w = view_panel_width(view),
                .h = window.y - view_menu_height(view),
              };
              const Bool in_sample_area = SDL_PointInRect(&mouse, &sample_area);
              const Bool in_program_area = SDL_PointInRect(&mouse, &program_area);

              // @rdk: pull this out
              const S32 sample_area_height = window.y - view_menu_height(view);
              const S32 sample_height = sample_area_height / MODEL_RADIX;

              if (event->button.button == SDL_BUTTON_LEFT) {
                if (in_program_area) {
                  view->interaction = INTERACTION_CAMERA;
                } else if (in_sample_area && event->button.clicks > 1) {
                  const S32 sample_index = (mouse.y - view_menu_height(view)) / sample_height;
                  if (sample_index >= 0 && sample_index < MODEL_RADIX) {
                    SampleLoadData* const data = SDL_malloc(sizeof(*data));
                    data->view = view;
                    data->index = sample_index;
                    SDL_ShowOpenFileDialog(
                        open_sample_callback,
                        data,
                        NULL,
                        NULL,
                        0,
                        NULL,
                        false);
                    view->interaction = INTERACTION_FILE_DIALOG;
                  }
                }
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
  Bool finished = false;
  while (finished == false) {
    SDL_AsyncIOOutcome outcome = {0};
    const Bool result = SDL_GetAsyncIOResult(view->io_queue, &outcome);
    if (result) {
      ASSERT(outcome.buffer);
      ASSERT(outcome.bytes_transferred > 0);
      FileLoaded* const file_loaded = outcome.userdata;
      drwav wav = {0};
      const Bool init_status = drwav_init_memory(
          &wav,
          outcome.buffer,
          outcome.bytes_transferred,
          NULL);
      ASSERT(init_status);
      ASSERT(wav.totalPCMFrameCount > 0);
      if (wav.channels == STEREO) {
        // @rdk: Free this later.
        F32* const samples = SDL_malloc(wav.totalPCMFrameCount * wav.channels * sizeof(*samples));
        const U64 read = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, samples);
        ASSERT(read == wav.totalPCMFrameCount);
        const Sound sound = {
          .frames = wav.totalPCMFrameCount,
          .samples = samples,
        };
        // @rdk: It seems wrong that we need bidirectional dependency for this.
        render_waveform(view, file_loaded->index, sound);

        ATOMIC_QUEUE_ENQUEUE(ControlMessage)(
            &control_queue,
            control_message_sound(file_loaded->index, sound));

      } else {
        SDL_Log("wav file must be stereo");
      }
      SDL_free(file_loaded);
    } else {
      finished = true;
    }
  }
}

Void view_render(View* view, const ModelGraph* model_graph, const DSPState* dsp, const RenderMetrics* metrics)
{
  // synonyms
  const Model* const m = &model_graph->model;
  const Graph* const g = &model_graph->graph;

  // clear
  SDL_SetRenderDrawColorFloat(view->renderer, 0.1f, 0.1f, 0.1f, SDL_ALPHA_OPAQUE_FLOAT);
  SDL_RenderClear(view->renderer);

  // get output resolution
  V2S window = {0};
  if (SDL_GetRenderOutputSize(view->renderer, &window.x, &window.y) == false) {
    SDL_Log("Failed to get render output size: %s", SDL_GetError());
  }

  // draw input highlights
  for (Index i = 0; i < g->head; i++) {
    const GraphEdge edge = g->edges[i];
    if (edge.tag == GRAPH_EDGE_INPUT) {
      draw_world_highlight(view, color_input, edge.target);
    }
  }

  // draw cursor highlight
  draw_world_highlight(view, color_cursor, view->cursor);

  // draw model
  for (S32 y = 0; y < MODEL_Y; y++) {
    for (S32 x = 0; x < MODEL_X; x++) {

      const Value value = m->map[y][x];
      const V2S point = { (S32) x, (S32) y };
      const Char tag_character = representation_table[value.tag];

      if (value.tag == VALUE_LITERAL) {
        const S32 literal = m->map[y][x].literal;
        const Char letter = 'A' + (Char) literal - 10;
        const Char digit = '0' + (Char) literal;
        const Char literal_character = literal > 9 ? letter : digit;
        draw_world_character(view, color_literal, point, literal_character);
      } else if (tag_character != 0) {
        const SDL_Color color = value.powered
          ? color_white
          : (value.pulse ? color_pulse : color_unpowered);
        draw_world_character(view, color, point, tag_character);
      } else {
        draw_world_character(view, color_empty, point, EMPTY_CHARACTER);
      }

    }
  }

  // draw sample panel background
  {
    SDL_SetRenderDrawColorStruct(view->renderer, color_panel);
    const SDL_FRect panel = {
      .x = 0.f,
      .y = (F32) view_menu_height(view),
      .w = (F32) view_panel_width(view),
      .h = (F32) (window.y - view_menu_height(view)),
    };
    SDL_RenderFillRect(view->renderer, &panel);
  }

  // draw sample waveforms
  const S32 sample_area_height = window.y - view_menu_height(view);
  const S32 sample_height = sample_area_height / MODEL_RADIX;
  SDL_SetRenderDrawColorStruct(view->renderer, color_sample_slot);
  for (S32 i = 0; i < MODEL_RADIX; i++) {
    Texture* const texture = &view->waveforms[i];
    const SDL_FRect panel = {
      .x = 1.f,
      .y = (F32) (sample_height * i + view_menu_height(view) + 1),
      .w = (F32) (view_panel_width(view) - 2),
      .h = (F32) (sample_height - 2),
    };
    if (texture->texture) {
      const SDL_FRect source = {
        .x = 0.f,
        .y = 0.f,
        .w = (F32) texture->dimensions.x,
        .h = (F32) texture->dimensions.y,
      };
      SDL_RenderTexture(view->renderer, texture->texture, &source, &panel);
    } else {
      SDL_RenderFillRect(view->renderer, &panel);
    }
  }

  // draw voice playheads
  SDL_SetRenderDrawColorStruct(view->renderer, color_white);
  for (Index i = 0; i < SIM_VOICES; i++) {
    const DSPSamplerVoice* voice = &dsp->voices[i];
    if (voice->active) {
      const F32 proportion = voice->frame / voice->length;
      const SDL_FRect destination = {
        .x = proportion * view_panel_width(view),
        .y = (F32) (sample_height * voice->sound + view_menu_height(view) + 1),
        .w = 1.f,
        .h = (F32) (sample_height - 2),
      };
      SDL_RenderFillRect(view->renderer, &destination);
      SDL_Log("active");
    }
  }

  // draw graph
  const S32 graph_panel_left = window.x - view_panel_width(view);

  // draw graph panel background
  {
    SDL_SetRenderDrawColorStruct(view->renderer, color_panel);
    const SDL_FRect panel = {
      .x = (F32) graph_panel_left,
      .y = 0.f,
      .w = (F32) view_panel_width(view),
      .h = (F32) window.y,
    };
    SDL_RenderFillRect(view->renderer, &panel);
  }

  // read hovered value
  const Value cursor_value = model_get(m, view->cursor);

  UIContext context;
  context.origin.x = graph_panel_left + PADDING;
  context.origin.y = view_menu_height(view) + PADDING;
  context.bounds.x = PANEL_CHARACTERS;
  context.bounds.y = 0; // unused, for now
  context.cursor.x = 0;
  context.cursor.y = 0;

  // for text processing
  Char buffer[PANEL_CHARACTERS] = {0};

  // draw noun
  draw_ui_text(view, &context, noun_table[cursor_value.tag]);
  if (is_operator(cursor_value) && cursor_value.powered == false) {
    draw_ui_text(view, &context, " (unpowered)");
  }
  draw_ui_text(view, &context, "\n\n");

  // draw description
  if (cursor_value.tag == VALUE_LITERAL) {
    SDL_snprintf(
        buffer,
        PANEL_CHARACTERS,
        "0x%02X = %02d",
        cursor_value.literal,
        cursor_value.literal);
    draw_ui_text(view, &context, buffer);
    draw_ui_text(view, &context, "\n\n");
  } else {
    draw_ui_text(view, &context, description_table[cursor_value.tag]);
    draw_ui_text(view, &context, "\n\n");
  }

#if 0
  // draw attributes
  Index attributes = 0;
  for (Index i = 0; i < g->head; i++) {
    const GraphEdge edge = g->edges[i];
    if (edge.tag == GRAPH_EDGE_INPUT && v2s_equal(edge.origin, view->cursor)) {
      const V2S delta = v2s_sub(edge.target, edge.origin);
      SDL_snprintf(buffer, PANEL_CHARACTERS, "<%d, %d> is %s\n", delta.x, delta.y, edge.attribute);
      draw_ui_text(view, &context, buffer);
      attributes += 1;
    }
  }

  // draw separator, if necessary
  if (attributes > 0) {
    draw_ui_text(view, &context, "\n");
  }
#endif

  // draw input edges
  Index inputs = 0;
  for (Index i = 0; i < g->head; i++) {
    const GraphEdge edge = g->edges[i];
    if (edge.tag == GRAPH_EDGE_INPUT && v2s_equal(edge.target, view->cursor)) {
      SDL_snprintf(buffer, PANEL_CHARACTERS, "%s for %s\n", edge.attribute, noun_table[edge.cause]);
      draw_ui_text(view, &context, buffer);
    }
  }

  // draw separator, if necessary
  if (inputs > 0) {
    draw_ui_text(view, &context, "\n");
  }

  // reset text drawing context
  context.origin = v2s(PADDING, window.y - (4 * view->font_small.glyph.y + PADDING));
  context.cursor = v2s(0, 0);

  // draw debug metrics
  draw_ui_text(view, &context, "DEBUG METRICS\n");
  SDL_snprintf(
      buffer,
      PANEL_CHARACTERS,
      "frame time: %03llu.%03llums\n",
      metrics->frame_time / KILO,
      metrics->frame_time % KILO);
  draw_ui_text(view, &context, buffer);
  SDL_snprintf(buffer, PANEL_CHARACTERS, "frame count: %03llu\n", metrics->frame_count);
  draw_ui_text(view, &context, buffer);
  SDL_snprintf(buffer, PANEL_CHARACTERS, "history index: %03td\n", metrics->render_index);
  draw_ui_text(view, &context, buffer);

  // draw menu bar
  {
    SDL_SetRenderDrawColorStruct(view->renderer, color_menu);
    const SDL_FRect panel = {
      .x = 0.f,
      .y = 0.f,
      .w = (F32) window.x,
      .h = (F32) (view->font_small.glyph.y + 2 * PADDING),
    };
    SDL_RenderFillRect(view->renderer, &panel);
  }

  // draw file interaction overlay
  if (view->interaction == INTERACTION_FILE_DIALOG) {
    SDL_SetRenderDrawColorStruct(view->renderer, color_dialog);
    const SDL_FRect panel = {
      .x = 0.f,
      .y = 0.f,
      .w = (F32) window.x,
      .h = (F32) window.y,
    };
    SDL_RenderFillRect(view->renderer, &panel);
  }

  // set context for menu bar
  context.origin = v2s(PADDING, PADDING);
  context.cursor = v2s(0, 0);

  draw_ui_text(view, &context, "File");

  // present
  SDL_RenderPresent(view->renderer);
}

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#ifdef DEBUG_ATLAS
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif
