#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>
#include <ctype.h>
#include "render.h"
#include "font.ttf.h"
#include "stb_truetype.h"

#define WORLD_FONT_SIZE 40  // in pixels
#define UI_FONT_SIZE 20     // in pixels
#define ASCII_X 16
#define ASCII_Y 8
#define ASCII_AREA (ASCII_X * ASCII_Y)
#define EMPTY_CHARACTER '.'
#define MIN_CHAR '!'
#define MAX_CHAR '~'
#define COLOR_CHANNELS 4
#define PADDING 6
#define MENU_PADDING 4
#define OPERATOR_DESCRIPTION_LINES 3
#define PANEL_CHARACTERS 44
#define PANEL_WIDTH (PANEL_CHARACTERS * ui_font.glyph.x + 2 * PADDING)
#define MENU_HEIGHT (ui_font.glyph.y + 2 * PADDING)

typedef struct Font {
  SDL_Texture* texture;   // handle to gpu texture
  V2S glyph;              // size of a single glyph, in pixels
} Font;

typedef struct UIContext {
  const Font* font;
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
    "Reads from program memory at relative coordinate <" ATTRIBUTE_X, ", ", ATTRIBUTE_Y ">.",
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

static SDL_Renderer* renderer = NULL;

// font data
static Font world_font = {0};
static Font ui_font = {0};

// cache the world tile size
static S32 world_tile = 0;

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

static V2S font_coordinate(Char c)
{
  V2S out;
  out.x = c % ASCII_X;
  out.y = c / ASCII_X;
  return out;
}

static Bool valid_atlas_point(V2S c, V2S d)
{
  const Bool x = c.x >= 0 && c.x < d.x;
  const Bool y = c.y >= 0 && c.y < d.y;
  return x && y;
}

static Void SDL_SetRenderDrawColorStruct(SDL_Renderer* r, SDL_Color color)
{
  SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
}

static Font load_font(S32 font_size)
{
  // the font that we will return
  Font font = {0};

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
  font.glyph.x = (S32) (scale * raw_advance) + 1;
  font.glyph.y = font_size;
  const S32 char_area = font.glyph.x * font.glyph.y;
  const V2S graph = v2s_mul(v2s(ASCII_X, ASCII_Y), font.glyph);

  // allocate the atlas and clear it to zero
  Byte* const atlas = SDL_calloc(1, ASCII_AREA * char_area);

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
    ASSERT(w <= font.glyph.x);
    ASSERT(h <= font.glyph.y);

    const V2S p = font_coordinate(c);
    const V2S o = v2s_mul(p, font.glyph);
    for (S32 y = 0; y < h; y++) {
      for (S32 x = 0; x < w; x++) {
        const S32 x0 = o.x + xoff + x;
        const S32 y0 = o.y + scaled_ascent + yoff + y;
        if (valid_atlas_point(v2s(x0, y0), graph)) {
          atlas[y0 * graph.x + x0] = bitmap[y * w + x];
        }
      }
    }

    stbtt_FreeBitmap(bitmap, NULL);

  }

#ifdef ATLAS_LINES
  // draw debug line separators in the atlas
  for (Index y = 0; y < ASCII_Y; y++) {
    for (Index x = 0; x < ASCII_X * font.glyph.x; x++) {
      atlas[y * font.glyph.y * graph.x + x] = 0xFF;
    }
  }
  for (Index x = 0; x < ASCII_X; x++) {
    for (Index y = 0; y < ASCII_Y * font.glyph.y; y++) {
      atlas[y * graph.x + x * font.glyph.x] = 0xFF;
    }
  }
#endif

#ifdef WRITE_ATLAS
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
      ASCII_X * font.glyph.x,                 // width
      ASCII_Y * font.glyph.y,                 // height
      SDL_PIXELFORMAT_RGBA32,                 // pixel format
      channels,                               // pixel data
      COLOR_CHANNELS * ASCII_X * font.glyph.x // pitch
      );

  // create gpu texture
  font.texture = SDL_CreateTextureFromSurface(renderer, surface);
  ASSERT(font.texture);

  SDL_DestroySurface(surface);
  SDL_free(channels);
  SDL_free(atlas);
  return font;
}

V2S render_tile_size()
{
  const S32 tile = MAX(world_font.glyph.x, world_font.glyph.y);
  return v2s(tile, tile);
}

Void render_init(SDL_Renderer* sdl_renderer, F32 scale)
{
  // store global renderer pointer
  renderer = sdl_renderer;

  const Bool blend_status = SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  ASSERT(blend_status);

  const S32 world_font_size = (S32) (WORLD_FONT_SIZE * scale);
  const S32 ui_font_size = (S32) (UI_FONT_SIZE * scale);

  // load world font
  world_font = load_font(world_font_size);
  world_tile = MAX(world_font.glyph.x, world_font.glyph.y);

  // load ui font
  ui_font = load_font(ui_font_size);
}

// @rdk: We shouldn't set the color for every character.
static Void draw_character(const Font* font, SDL_Color color, V2F origin, Char c)
{
  const V2S uv = font_coordinate(c);

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
  SDL_SetTextureColorMod(font->texture, color.r, color.g, color.b);
  SDL_SetTextureAlphaMod(font->texture, color.a);
  SDL_RenderTexture(renderer, font->texture, &source, &destination);
}

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
      const V2S offset = v2s_mul(context->cursor, context->font->glyph);
      const V2F origin = v2f_of_v2s(v2s_add(context->origin, offset));
      draw_character(context->font, color_white, origin, head[i]);
      context->cursor.x += 1;
    }

    // advance
    head += word_length;

  }
}

static V2F world_to_screen(V2F camera, V2S point)
{
  const V2F tile = { (F32) world_tile, (F32) world_tile };
  const V2F relative = v2f_sub(v2f_of_v2s(point), camera);
  const V2F offset = {
    (F32) PANEL_WIDTH,
    (F32) MENU_HEIGHT,
  };
  return v2f_add(offset, v2f_mul(relative, tile));
}

static Void draw_world_character(V2F camera, SDL_Color color, V2S point, Char c)
{
  const V2F origin = world_to_screen(camera, point);
  const F32 delta = (world_tile - world_font.glyph.x) / 2.f;
  const V2F screen = { origin.x + delta, origin.y };
  draw_character(&world_font, color, screen, c);
}

static Void draw_world_highlight(V2F camera, SDL_Color color, V2S point)
{
  const V2F origin = world_to_screen(camera, point);

  // screen coordinates to fill
  SDL_FRect destination;
  destination.x = origin.x;
  destination.y = origin.y;
  destination.w = (F32) world_tile;
  destination.h = (F32) world_tile;

  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &destination);
}

Void render_frame(const View* view, const ModelGraph* model_graph, const RenderMetrics* metrics)
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
  const S32 graph_panel_left = window.x - PANEL_WIDTH;

#if 0
  // draw sample panel background
  {
    SDL_SetRenderDrawColorStruct(renderer, color_panel);
    const SDL_FRect panel = {
      .x = 0.f,
      .y = 0.f,
      .w = (F32) PANEL_WIDTH,
      .h = (F32) window.y,
    };
    SDL_RenderFillRect(renderer, &panel);
  }
#endif

  // draw graph panel background
  {
    SDL_SetRenderDrawColorStruct(renderer, color_panel);
    const SDL_FRect panel = {
      .x = (F32) graph_panel_left,
      .y = 0.f,
      .w = (F32) PANEL_WIDTH,
      .h = (F32) window.y,
    };
    SDL_RenderFillRect(renderer, &panel);
  }

  // read hovered value
  const Value cursor_value = model_get(m, view->cursor);

  UIContext context;
  context.font = &ui_font;
  context.origin.x = graph_panel_left + PADDING;
  context.origin.y = MENU_HEIGHT + PADDING;
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
  context.origin = v2s(PADDING, window.y - (4 * ui_font.glyph.y + PADDING));
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
  SDL_snprintf(buffer, PANEL_CHARACTERS, "history index: %03lld\n", metrics->render_index);
  draw_ui_text(&context, buffer);

  // draw menu bar
  {
    SDL_SetRenderDrawColorStruct(renderer, color_menu);
    const SDL_FRect panel = {
      .x = 0.f,
      .y = 0.f,
      .w = (F32) window.x,
      .h = (F32) (ui_font.glyph.y + 2 * PADDING),
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

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
