#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_log.h>
#include <ctype.h>
#include "render.h"

#define EMPTY_CHARACTER '.'
#define COLOR_CHANNELS 4

// @rdk: unify with texture
typedef struct GPUFont {
  V2S glyph;
  SDL_Texture* texture;
} GPUFont; 

typedef struct Texture {
  V2S dimensions;
  SDL_Texture* texture;
} Texture;

typedef struct UIContext {
  V2S origin;
  V2S bounds;
  V2S cursor;
} UIContext;

#define COLOR_STRUCTURE(rv, gv, bv, av) { .r = rv, .g = gv, .b = bv, .a = av }
static const SDL_Color color_white     = COLOR_STRUCTURE(0xFF, 0xFF, 0xFF, 0xFF);
static const SDL_Color color_empty     = COLOR_STRUCTURE(0xFF, 0xFF, 0xFF, 0x80);
static const SDL_Color color_literal   = COLOR_STRUCTURE(0x80, 0x80, 0xFF, 0xFF);
static const SDL_Color color_pulse     = COLOR_STRUCTURE(0x80, 0xFF, 0x80, 0xFF);
static const SDL_Color color_unpowered = COLOR_STRUCTURE(0xA0, 0xA0, 0xA0, 0xFF);
static const SDL_Color color_cursor    = COLOR_STRUCTURE(0x80, 0x80, 0xFF, 0x40);
static const SDL_Color color_panel     = COLOR_STRUCTURE(0x10, 0x10, 0x10, 0xFF);
static const SDL_Color color_menu      = COLOR_STRUCTURE(0x20, 0x20, 0x20, 0xFF);
static const SDL_Color color_outline   = COLOR_STRUCTURE(0xA0, 0xA0, 0xA0, 0xFF);
static const SDL_Color color_input     = COLOR_STRUCTURE(0x60, 0x70, 0x80, 0x80);
static const SDL_Color color_sample_slot = COLOR_STRUCTURE(0x40, 0x40, 0x40, 0xFF);
static const SDL_Color color_dialog    = COLOR_STRUCTURE(0xFF, 0xFF, 0xFF, 0x40);

static const View* view = NULL;
static SDL_Renderer* renderer = NULL;
static GPUFont font_small = {0};
static GPUFont font_large = {0};

static Texture waveform_textures[MODEL_RADIX] = {0};

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

static Void load_font(GPUFont* out, const Font* font)
{
  const V2S graph = v2s_mul(v2s(ASCII_X, ASCII_Y), font->glyph);
  const S32 char_area = font->glyph.x * font->glyph.y;

  Byte* const channels = SDL_malloc(COLOR_CHANNELS * ASCII_AREA * char_area);
  for (Index y = 0; y < graph.y; y++) {
    for (Index x = 0; x < graph.x; x++) {
      const Byte alpha = font->bitmap[y * graph.x + x];
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
  out->texture = SDL_CreateTextureFromSurface(renderer, surface);
  out->glyph = font->glyph;
  ASSERT(out->texture);

  SDL_DestroySurface(surface);
  SDL_free(channels);
}

static V2F world_to_screen(V2F camera, V2S point)
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

static Void draw_world_highlight(V2F camera, SDL_Color color, V2S point)
{
  const V2F origin = world_to_screen(camera, point);
  const S32 tile_width = view_tile_size(view);

  // screen coordinates to fill
  SDL_FRect destination;
  destination.x = origin.x;
  destination.y = origin.y;
  destination.w = (F32) tile_width;
  destination.h = (F32) tile_width;

  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &destination);
}

static Void draw_character(const GPUFont* font, V2F origin, Char c)
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
#if 0
  SDL_SetTextureColorMod(font->texture, color.r, color.g, color.b);
  SDL_SetTextureAlphaMod(font->texture, color.a);
#endif
  SDL_RenderTexture(renderer, font->texture, &source, &destination);
}

static Void draw_world_character(V2F camera, SDL_Color color, V2S point, Char c)
{
  const S32 tile_width = view_tile_size(view);
  const V2F origin = world_to_screen(camera, point);
  const F32 delta = (tile_width - font_large.glyph.x) / 2.f;
  const V2F screen = { origin.x + delta, origin.y };
  SDL_SetTextureColorMod(font_large.texture, color.r, color.g, color.b);
  SDL_SetTextureAlphaMod(font_large.texture, color.a);
  draw_character(&font_large, screen, c);
}

// @rdk: We shouldn't set the color for every character.
static Void draw_ui_text(UIContext* context, const Char* text)
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
      const V2S offset = v2s_mul(context->cursor, font_small.glyph);
      const V2F origin = v2f_of_v2s(v2s_add(context->origin, offset));
      draw_character(&font_small, origin, head[i]);
      context->cursor.x += 1;
    }

    // advance
    head += word_length;

  }
}

Void render_init(SDL_Renderer* sdl_renderer, const View* view_pointer)
{
  // store global pointers
  renderer = sdl_renderer;
  view = view_pointer;

  const Bool blend_status = SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  ASSERT(blend_status);

  load_font(&font_small, &view->font_small);
  load_font(&font_large, &view->font_large);
}

Void render_frame(const ModelGraph* model_graph, const RenderMetrics* metrics)
{
  // synonyms
  const Model* const m = &model_graph->model;
  const Graph* const g = &model_graph->graph;

  // clear
  SDL_SetRenderDrawColorFloat(renderer, 0.1f, 0.1f, 0.1f, SDL_ALPHA_OPAQUE_FLOAT);
  SDL_RenderClear(renderer);

  // get output resolution
  V2S window = {0};
  const Bool output_size_status = SDL_GetRenderOutputSize(renderer, &window.x, &window.y);
  ASSERT(output_size_status);

  // draw input highlights
  for (Index i = 0; i < g->head; i++) {
    const GraphEdge edge = g->edges[i];
    if (edge.tag == GRAPH_EDGE_INPUT) {
      draw_world_highlight(view->camera, color_input, edge.target);
    }
  }

  // draw cursor highlight
  draw_world_highlight(view->camera, color_cursor, view->cursor);

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
        draw_world_character(view->camera, color_literal, point, literal_character);
      } else if (tag_character != 0) {
        const SDL_Color color = value.powered
          ? color_white
          : (value.pulse ? color_pulse : color_unpowered);
        draw_world_character(view->camera, color, point, tag_character);
      } else {
        draw_world_character(view->camera, color_empty, point, EMPTY_CHARACTER);
      }

    }
  }

  // draw graph
  const S32 graph_panel_left = window.x - view_panel_width(view);

  // draw sample panel background
  {
    SDL_SetRenderDrawColorStruct(renderer, color_panel);
    const SDL_FRect panel = {
      .x = 0.f,
      .y = (F32) view_menu_height(view),
      .w = (F32) view_panel_width(view),
      .h = (F32) (window.y - view_menu_height(view)),
    };
    SDL_RenderFillRect(renderer, &panel);
  }

  // draw sample waveforms
  const S32 sample_area_height = window.y - view_menu_height(view);
  const S32 sample_height = sample_area_height / MODEL_RADIX;
  SDL_SetRenderDrawColorStruct(renderer, color_sample_slot);
  for (S32 i = 0; i < MODEL_RADIX; i++) {
    Texture* const texture = &waveform_textures[i];
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
      SDL_RenderTexture(renderer, texture->texture, &source, &panel);
    } else {
      SDL_RenderFillRect(renderer, &panel);
    }
  }

  // draw graph panel background
  {
    SDL_SetRenderDrawColorStruct(renderer, color_panel);
    const SDL_FRect panel = {
      .x = (F32) graph_panel_left,
      .y = 0.f,
      .w = (F32) view_panel_width(view),
      .h = (F32) window.y,
    };
    SDL_RenderFillRect(renderer, &panel);
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
  draw_ui_text(&context, noun_table[cursor_value.tag]);
  if (is_operator(cursor_value) && cursor_value.powered == false) {
    draw_ui_text(&context, " (unpowered)");
  }
  draw_ui_text(&context, "\n\n");

  // draw description
  if (cursor_value.tag == VALUE_LITERAL) {
    SDL_snprintf(
        buffer,
        PANEL_CHARACTERS,
        "0x%02X = %02d",
        cursor_value.literal,
        cursor_value.literal);
    draw_ui_text(&context, buffer);
    draw_ui_text(&context, "\n\n");
  } else {
    draw_ui_text(&context, description_table[cursor_value.tag]);
    draw_ui_text(&context, "\n\n");
  }

#if 0
  // draw attributes
  Index attributes = 0;
  for (Index i = 0; i < g->head; i++) {
    const GraphEdge edge = g->edges[i];
    if (edge.tag == GRAPH_EDGE_INPUT && v2s_equal(edge.origin, view->cursor)) {
      const V2S delta = v2s_sub(edge.target, edge.origin);
      SDL_snprintf(buffer, PANEL_CHARACTERS, "<%d, %d> is %s\n", delta.x, delta.y, edge.attribute);
      draw_ui_text(&context, buffer);
      attributes += 1;
    }
  }

  // draw separator, if necessary
  if (attributes > 0) {
    draw_ui_text(&context, "\n");
  }
#endif

  // draw input edges
  Index inputs = 0;
  for (Index i = 0; i < g->head; i++) {
    const GraphEdge edge = g->edges[i];
    if (edge.tag == GRAPH_EDGE_INPUT && v2s_equal(edge.target, view->cursor)) {
      SDL_snprintf(buffer, PANEL_CHARACTERS, "%s for %s\n", edge.attribute, noun_table[edge.cause]);
      draw_ui_text(&context, buffer);
    }
  }

  // draw separator, if necessary
  if (inputs > 0) {
    draw_ui_text(&context, "\n");
  }

  // reset text drawing context
  context.origin = v2s(PADDING, window.y - (4 * font_small.glyph.y + PADDING));
  context.cursor = v2s(0, 0);

  // draw debug metrics
  draw_ui_text(&context, "DEBUG METRICS\n");
  SDL_snprintf(
      buffer,
      PANEL_CHARACTERS,
      "frame time: %03llu.%03llums\n",
      metrics->frame_time / KILO,
      metrics->frame_time % KILO);
  draw_ui_text(&context, buffer);
  SDL_snprintf(buffer, PANEL_CHARACTERS, "frame count: %03llu\n", metrics->frame_count);
  draw_ui_text(&context, buffer);
  SDL_snprintf(buffer, PANEL_CHARACTERS, "history index: %03td\n", metrics->render_index);
  draw_ui_text(&context, buffer);

  // draw menu bar
  {
    SDL_SetRenderDrawColorStruct(renderer, color_menu);
    const SDL_FRect panel = {
      .x = 0.f,
      .y = 0.f,
      .w = (F32) window.x,
      .h = (F32) (font_small.glyph.y + 2 * PADDING),
    };
    SDL_RenderFillRect(renderer, &panel);
  }

  // draw file interaction overlay
  if (view->interaction == INTERACTION_FILE_DIALOG) {
    SDL_SetRenderDrawColorStruct(renderer, color_dialog);
    const SDL_FRect panel = {
      .x = 0.f,
      .y = 0.f,
      .w = (F32) window.x,
      .h = (F32) window.y,
    };
    SDL_RenderFillRect(renderer, &panel);
  }

  // set context for menu bar
  context.origin = v2s(PADDING, PADDING);
  context.cursor = v2s(0, 0);

  draw_ui_text(&context, "File");

  // present
  SDL_RenderPresent(renderer);
}

Void render_waveform(S32 index, Sound sound)
{
  ASSERT(index >= 0);
  ASSERT(index < MODEL_RADIX);

  // @rdk: We should use the display size for this, not the window size.
  V2S window = {0};
  const Bool output_size_status = SDL_GetRenderOutputSize(renderer, &window.x, &window.y);
  ASSERT(output_size_status);

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
  waveform_textures[index].dimensions = v2s(sample_x, sample_y);
  waveform_textures[index].texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (waveform_textures[index].texture == NULL) {
    SDL_Log("Failed to create texture: %s", SDL_GetError());
  }

}
