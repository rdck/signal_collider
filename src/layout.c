#include <ctype.h>
#include <SDL3/SDL_log.h>
#include "layout.h"

#define EMPTY_CHARACTER '.'
#define PADDING 6
#define OPERATOR_DESCRIPTION_LINES 3

#define MEMORY_CHARACTERS 16
#define TEMPO_CHARACTERS 9

typedef struct UIContext {
  V2F origin;
  V2S bounds;
  V2S cursor;
} UIContext;

typedef struct MenuArray {
  S32 length;
  const Char** items;
} MenuArray;

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
static const SDL_Color color_menu_highlight = COLOR_STRUCTURE(0x2A, 0x88, 0xAD, 0xFF);

static Void reset_ui_context(UIContext* context, V2F origin, V2S bounds)
{
  context->origin = origin;
  context->bounds = bounds;
  context->cursor = v2s(0, 0);
}

static const SDL_Color sdl_color(U8 r, U8 g, U8 b, U8 a)
{
  const SDL_Color color = COLOR_STRUCTURE(r, g, b, a);
  return color;
}

static const Char* menu_name_table[MENU_CARDINAL] = {
  [ MENU_FILE ] = " File ",
  [ MENU_EDIT ] = " Edit ",
  [ MENU_HELP ] = " Help ",
};

static const Char* file_menu_table[FILE_MENU_CARDINAL] = {
  [ FILE_MENU_NEW ] = "New",
  [ FILE_MENU_SAVE_AS ] = "Save As",
  [ FILE_MENU_EXIT ] = "Exit",
};

static const Char* edit_menu_table[EDIT_MENU_CARDINAL] = {
  [ EDIT_MENU_CUT ] = "Cut",
  [ EDIT_MENU_COPY ] = "Copy",
  [ EDIT_MENU_PASTE ] = "Paste",
};

static const Char* help_menu_table[HELP_MENU_CARDINAL] = {
  [ HELP_MENU_MANUAL ] = "Manual",
};

static const MenuArray menu_table[MENU_CARDINAL] = {
  [ MENU_FILE ] = { .length = FILE_MENU_CARDINAL, .items = file_menu_table },
  [ MENU_EDIT ] = { .length = EDIT_MENU_CARDINAL, .items = edit_menu_table },
  [ MENU_HELP ] = { .length = HELP_MENU_CARDINAL, .items = help_menu_table },
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

static DrawRectangle draw_rectangle(R2F area, SDL_Color color, TextureDescription texture)
{
  DrawRectangle r;
  r.area = area;
  r.color = color;
  r.texture = texture;
  return r;
}

static InteractionRectangle interaction_none(R2F area)
{
  InteractionRectangle r;
  r.tag = INTERACTION_NONE;
  r.area = area;
  return r;
}

static InteractionRectangle interaction_generic(InteractionTag tag, R2F area)
{
  InteractionRectangle r;
  r.tag = tag;
  r.area = area;
  return r;
}

static InteractionRectangle interaction_map(R2F area)
{
  InteractionRectangle r;
  r.tag = INTERACTION_TAG_MAP;
  r.area = area;
  return r;
}

static InteractionRectangle interaction_waveform(R2F area, S32 index)
{
  InteractionRectangle r;
  r.tag = INTERACTION_TAG_WAVEFORM;
  r.area = area;
  r.index = index;
  return r;
}

static InteractionRectangle interaction_sound_scroll(R2F area)
{
  InteractionRectangle r;
  r.tag = INTERACTION_TAG_SOUND_SCROLL;
  r.area = area;
  return r;
}

static InteractionRectangle interaction_menu(R2F area, Menu menu, S32 item)
{
  InteractionRectangle r;
  r.tag = INTERACTION_TAG_MENU;
  r.area = area;
  r.menu_item.menu = menu;
  r.menu_item.item = item;
  return r;
}

static InteractionRectangle interaction_tempo(R2F area)
{
  InteractionRectangle r;
  r.tag = INTERACTION_TAG_TEMPO;
  r.area = area;
  return r;
}

static Void write_draw_rectangle(DrawArena* arena, DrawRectangle rectangle)
{
  if (arena->head < LAYOUT_DRAW_RECTANGLES) {
    arena->buffer[arena->head] = rectangle;
    arena->head += 1;
  } else {
    SDL_Log("layout draw overflow");
  }
}

static Void write_interaction_rectangle(InteractionArena* arena, InteractionRectangle rectangle)
{
  if (arena->head < LAYOUT_INTERACTION_RECTANGLES) {
    arena->buffer[arena->head] = rectangle;
    arena->head += 1;
  } else {
    SDL_Log("layout interaction overflow");
  }
}

static R2F map_tile(V2F camera, S32 tile_size, V2S origin, V2S point)
{
  const V2F tile = { (F32) tile_size, (F32) tile_size };
  const V2F relative = v2f_sub(v2f_of_v2s(point), camera);
  const R2F out = {
    .origin = v2f_add(v2f_of_v2s(origin), v2f_mul(relative, tile)),
    .size = tile,
  };
  return out;
}

static TextureDescription character_texture(TextureName name, Char character)
{
  TextureDescription description;
  description.name = name;
  description.character = character;
  return description;
}

static TextureDescription waveform_texture(S32 index)
{
  TextureDescription description;
  description.name = TEXTURE_WAVEFORM;
  description.index = index;
  return description;
}

static Void draw_text(DrawArena* draw, UIContext* context, const Char* text, V2S glyph)
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
      const V2S offset = v2s_mul(context->cursor, glyph);
      const R2F area = {
        .origin = v2f_add(context->origin, v2f_of_v2s(offset)),
        .size = v2f_of_v2s(glyph),
      };
      write_draw_rectangle(
          draw,
          draw_rectangle(
            area,
            color_literal,
            character_texture(TEXTURE_FONT_SMALL, head[i])));
      context->cursor.x += 1;
    }

    // advance
    head += word_length;

  }
}

static Void draw_text_line(DrawArena* draw, V2F origin, V2S glyph, const Char* text)
{
  UIContext context;
  context.origin = origin;
  context.bounds = v2s(999, 999);
  context.cursor = v2s(0, 0);
  draw_text(draw, &context, text, glyph);
}

Void layout(
    DrawArena* draw,
    InteractionArena* interaction,
    const UIState* ui,
    const LayoutParameters* parameters)
{
  // shorthand
  const Graph* const graph = parameters->graph;
  const Model* const model = parameters->model;
  const RenderMetrics* const metrics = parameters->metrics;
  const DSPState* const dsp = parameters->dsp;
  const V2S font_small = parameters->font_small;
  const V2S font_large = parameters->font_large;
  const V2S window = parameters->window;
  const V2F camera = ui->camera;
  const V2F mouse = parameters->mouse;

  // initialize arenas
  draw->head = 0;
  interaction->head = 0;

  const S32 menu_height = font_small.y + 2 * PADDING;
  const S32 panel_width = LAYOUT_PANEL_CHARACTERS * font_small.x;
  const S32 tile_size = MAX(font_large.x, font_large.y);

  const V2S map_origin = {
    .x = panel_width,
    .y = menu_height,
  };

  const R2F map_area = {
    .origin = v2f_of_v2s(map_origin),
    .size = v2f((F32) (window.x - 2 * panel_width), (F32) (window.y - menu_height)),
  };

  const TextureDescription white = {
    .name = TEXTURE_WHITE,
  };

  // draw graph highlights
  for (Index i = 0; i < graph->head; i++) {
    const GraphEdge edge = graph->edges[i];
    if (edge.tag == GRAPH_EDGE_INPUT) {
      const R2F area = map_tile(camera, tile_size, map_origin, edge.target);
      write_draw_rectangle(
          draw,
          draw_rectangle(area, color_input, white));
    }
  }

  // draw model
  for (S32 y = 0; y < MODEL_Y; y++) {
    for (S32 x = 0; x < MODEL_X; x++) {

      const Value value = MODEL_INDEX(model, x, y);
      const Char tag_character = representation_table[value.tag];
      const R2F area = map_tile(camera, tile_size, map_origin, v2s(x, y));
      const F32 padding = (tile_size - font_large.x) / 2.f; // assumes font is taller than wide
      const R2F centered = {
        .origin = { area.origin.x + padding, area.origin.y },
        .size = v2f_of_v2s(font_large),
      };

      if (value.tag == VALUE_LITERAL) {
        const S32 literal = MODEL_INDEX(model, x, y).literal;
        const Char c = literal >= 10
          ? 'A' + (Char) literal - 10
          : '0' + (Char) literal;
        write_draw_rectangle(
            draw,
            draw_rectangle(
              centered,
              color_literal,
              character_texture(TEXTURE_FONT_LARGE, c)));
      } else if (tag_character != 0) {
        const SDL_Color color = value.powered
          ? color_white
          : (value.pulse ? color_pulse : color_unpowered);
        write_draw_rectangle(
            draw,
            draw_rectangle(
              centered,
              color,
              character_texture(TEXTURE_FONT_LARGE, tag_character)));
      } else {
        write_draw_rectangle(
            draw,
            draw_rectangle(
              centered,
              color_empty,
              character_texture(TEXTURE_FONT_LARGE, EMPTY_CHARACTER)));
      }

    }
  }

  // draw map cursor highlight
  {
    const R2F area = map_tile(camera, tile_size, map_origin, ui->cursor);
    write_draw_rectangle(draw, draw_rectangle(area, color_cursor, white));
  }

  // draw sample panel background
  {
    const R2F area = {
      .origin = { 0.f, (F32) menu_height },
      .size = { (F32) panel_width, (F32) (window.y - menu_height) },
    };
    write_draw_rectangle(draw, draw_rectangle(area, color_panel, white));
    write_interaction_rectangle(
        interaction,
        interaction_sound_scroll(area));
  }

  // draw sample waveforms and interaction boxes
  const S32 sample_height = 2 * font_small.y;
  const S32 sample_stride = sample_height + 1;
  const S32 sample_overflow = (MODEL_RADIX * sample_stride) - (window.y - menu_height);
  const S32 scroll = (S32) (ui->scroll * sample_overflow);
  for (S32 i = 0; i < MODEL_RADIX; i++) {
    const R2F area = {
      .origin = { 0.f, (F32) (sample_stride * i + menu_height - scroll) },
      .size = { (F32) panel_width, (F32) sample_height },
    };
    write_draw_rectangle(
        draw,
        draw_rectangle(area, color_sample_slot, waveform_texture(i)));
    write_interaction_rectangle(
        interaction,
        interaction_waveform(area, i));
  }

  // draw voice playheads
  for (Index i = 0; i < SIM_VOICES; i++) {
    const DSPSamplerVoice* const voice = &dsp->voices[i];
    if (voice->active) {
      const F32 proportion = voice->frame / voice->length;
      const R2F area = {
        .origin = {
          .x = proportion * panel_width,
          .y = (F32) (sample_stride * voice->sound + menu_height - scroll + 1),
        },
        .size = { 1.f, (F32) (sample_height) },
      };
      write_draw_rectangle(
          draw,
          draw_rectangle(area, color_white, white));
    }
  }

  const R2F right_panel = {
    .origin = { (F32) (window.x - panel_width), (F32) menu_height },
    .size = { (F32) panel_width, (F32) (window.y - menu_height) },
  };

  // draw right panel background
  write_draw_rectangle(draw, draw_rectangle(right_panel, color_panel, white));

  // read hovered value
  const Value cursor_value = model_get(model, ui->cursor);

  // for text processing
  Char buffer[LAYOUT_PANEL_CHARACTERS] = {0};

  UIContext context;
  context.origin.x = right_panel.origin.x + PADDING;
  context.origin.y = (F32) (menu_height + PADDING);
  context.bounds.x = LAYOUT_PANEL_CHARACTERS;
  context.bounds.y = 0; // unused, for now
  context.cursor.x = 0;
  context.cursor.y = 0;

  // draw noun
  draw_text(draw, &context, noun_table[cursor_value.tag], font_small);
  if (is_operator(cursor_value) && cursor_value.powered == false) {
    draw_text(draw, &context, " (unpowered)", font_small);
  }
  draw_text(draw, &context, "\n\n", font_small);

  // draw description
  if (cursor_value.tag == VALUE_LITERAL) {
    SDL_snprintf(
        buffer,
        LAYOUT_PANEL_CHARACTERS,
        "0x%02X = %02d",
        cursor_value.literal,
        cursor_value.literal);
    draw_text(draw, &context, buffer, font_small);
    draw_text(draw, &context, "\n\n", font_small);
  } else {
    draw_text(draw, &context, description_table[cursor_value.tag], font_small);
    draw_text(draw, &context, "\n\n", font_small);
  }

  // draw input edges
  Index inputs = 0;
  for (Index i = 0; i < graph->head; i++) {
    const GraphEdge edge = graph->edges[i];
    if (edge.tag == GRAPH_EDGE_INPUT && v2s_equal(edge.target, ui->cursor)) {
      SDL_snprintf(buffer, LAYOUT_PANEL_CHARACTERS, "%s for %s\n", edge.attribute, noun_table[edge.cause]);
      draw_text(draw, &context, buffer, font_small);
    }
  }

  // draw separator, if necessary
  if (inputs > 0) {
    draw_text(draw, &context, "\n", font_small);
  }

  // draw bottom panel background
  {
    const R2F menu_panel = {
      .origin = { 0.f, (F32) (window.y - menu_height) },
      .size = { (F32) window.x, (F32) menu_height },
    };
    write_draw_rectangle(draw, draw_rectangle(menu_panel, color_panel, white));
    write_interaction_rectangle(interaction, interaction_none(menu_panel));
  }

  // reset text drawing context
  context.origin.x = PADDING;
  context.origin.y = (F32) (window.y - menu_height + PADDING);
  context.cursor = v2s(0, 0);

  // draw memory dimensions
  {
    const R2F area = {
      .origin = {
        (F32) PADDING,
        (F32) (window.y - menu_height + PADDING),
      },
      .size = {
        (F32) (MEMORY_CHARACTERS * font_small.x),
        (F32) font_small.y,
      },
    };
    reset_ui_context(&context, area.origin, v2s(999, 999));
    Char memory_buffer[MEMORY_CHARACTERS] = {0};
    if (ui->interaction == INTERACTION_MEMORY_DIMENSIONS) {
      SDL_snprintf(memory_buffer, MEMORY_CHARACTERS, "%s|", ui->text);
      draw_text(draw, &context, memory_buffer, font_small);
    } else {
      SDL_snprintf(
          memory_buffer,
          TEMPO_CHARACTERS,
          "%dx%d",
          dsp->memory_dimensions.x,
          dsp->memory_dimensions.y);
      draw_text(draw, &context, memory_buffer, font_small);
    }
    write_interaction_rectangle(
        interaction,
        interaction_generic(INTERACTION_TAG_MEMORY_DIMENSIONS, area));
  }

  // draw tempo
  {
    const R2F area = {
      .origin = {
        (F32) (MEMORY_CHARACTERS * font_small.x + PADDING),
        (F32) (window.y - menu_height + PADDING),
      },
      .size = {
        (F32) (TEMPO_CHARACTERS * font_small.x),
        (F32) font_small.y,
      },
    };
    reset_ui_context(&context, area.origin, v2s(999, 999));
    Char tempo_buffer[TEMPO_CHARACTERS] = {0};
    if (ui->interaction == INTERACTION_TEMPO) {
      SDL_snprintf(tempo_buffer, TEMPO_CHARACTERS, "%s|", ui->text);
      draw_text(draw, &context, tempo_buffer, font_small);
    } else {
      SDL_snprintf(tempo_buffer, TEMPO_CHARACTERS, "%dbpm", dsp->tempo);
      draw_text(draw, &context, tempo_buffer, font_small);
    }
    write_interaction_rectangle(interaction, interaction_tempo(area));
  }

  // draw menu background
  {
    const R2F menu_panel = {
      .origin = { 0.f, 0.f },
      .size = { (F32) window.x, (F32) menu_height },
    };
    write_draw_rectangle(draw, draw_rectangle(menu_panel, color_panel, white));
  }

  // reset text drawing context
  context.origin.x = 0.f;
  context.origin.y = PADDING;
  context.cursor = v2s(0, 0);

  // draw menu content
  for (S32 menu_id = 1; menu_id < MENU_CARDINAL; menu_id++) {
    const Char* const menu_name = menu_name_table[menu_id];
    const MenuArray menu = menu_table[menu_id];
    const S32 left = context.cursor.x;
    const S32 span = (S32) strlen(menu_name);
    const R2F area = {
      .origin.x = left * font_small.x,
      .origin.y = PADDING,
      .size.x = span * font_small.x,
      .size.y = font_small.y,
    };
    write_interaction_rectangle(
        interaction,
        interaction_menu(area, menu_id, 0));
    if (v2f_in_r2f(mouse, area) && ui->interaction != INTERACTION_MENU) {
      write_draw_rectangle(
          draw,
          draw_rectangle(area, color_menu_highlight, white));
    }
    if (ui->interaction == INTERACTION_MENU || ui->interaction == INTERACTION_MENU_FINALIZE) {
      if (ui->menu == menu_id) {
        const SDL_Color color = COLOR_STRUCTURE(0x2A, 0x88, 0xAD, 0x80);
        write_draw_rectangle(
            draw,
            draw_rectangle(area, color, white));
        const R2F content_area = {
          .origin.x = area.origin.x,
          .origin.y = area.origin.y + area.size.y,
          .size.x = 32 * font_small.x,
          .size.y = (menu.length - 1) * font_small.y,
        };
        write_draw_rectangle(
            draw,
            draw_rectangle(
              content_area,
              sdl_color(0x20, 0x20, 0x20, 0xE0),
              white));
        for (S32 i = 1; i < menu.length; i++) {
          const V2F origin = {
            content_area.origin.x + font_small.x,
            content_area.origin.y + font_small.y * (i - 1),
          };
          const R2F line_area = {
            .origin.x = content_area.origin.x,
            .origin.y = origin.y,
            .size.x = 32 * font_small.x,
            .size.y = font_small.y,
          };
          if (v2f_in_r2f(mouse, line_area)) {
            write_draw_rectangle(
                draw,
                draw_rectangle(
                  line_area,
                  color_menu_highlight,
                  white));
          }
          draw_text_line(draw, origin, font_small, menu.items[i]);
          write_interaction_rectangle(
              interaction,
              interaction_menu(line_area, menu_id, i));
        }
      }
    }
    draw_text(draw, &context, menu_name, font_small);
  }

  // reset text drawing context
  context.origin.x = (F32) (panel_width + PADDING);
  context.origin.y = (F32) (menu_height + PADDING);
  context.cursor = v2s(0, 0);

  // draw debug metrics
  draw_text(draw, &context, "DEBUG METRICS\n", font_small);
  SDL_snprintf(
      buffer,
      LAYOUT_PANEL_CHARACTERS,
      "frame time: %03llu.%03llums\n",
      metrics->frame_time / KILO,
      metrics->frame_time % KILO);
  draw_text(draw, &context, buffer, font_small);
  SDL_snprintf(buffer, LAYOUT_PANEL_CHARACTERS, "frame count: %03llu\n", metrics->frame_count);
  draw_text(draw, &context, buffer, font_small);
  SDL_snprintf(buffer, LAYOUT_PANEL_CHARACTERS, "history index: %03td\n", metrics->render_index);
  draw_text(draw, &context, buffer, font_small);

  // draw map interaction
  write_interaction_rectangle(
      interaction,
      interaction_map(map_area));

}
