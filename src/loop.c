#include "loop.h"
#include "render.h"
#include "sim.h"
#include "message.h"
#include "view.h"

#define TITLE "Signal Collider"

#define RESX 320
#define RESY 180

// view data
ViewState view_state = VIEW_STATE_GRID;
Char console[CONSOLE_BUFFER] = {0};
Index console_head = 0;
V2S cursor = {0};

static V2S window_resolution = {0};

static Value value_table[0xFF] = {
  [ '=' ]       = { .tag = VALUE_IF         },
  [ '~' ]       = { .tag = VALUE_SYNTH      },
  [ 'c' ]       = { .tag = VALUE_CLOCK      },
  [ 'd' ]       = { .tag = VALUE_DELAY      },
  [ 'r' ]       = { .tag = VALUE_RANDOM     },
  [ '!' ]       = { .tag = VALUE_GENERATE   },
  [ '#' ]       = { .tag = VALUE_SCALE      },
  [ '+' ]       = { .tag = VALUE_ADD        },
  [ '-' ]       = { .tag = VALUE_SUB        },
  [ '*' ]       = { .tag = VALUE_MUL        },
  [ '$' ]       = { .tag = VALUE_SAMPLER    },
};

static S32 literal_of_char(Char c)
{
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'A' && c <= 'Z') {
    return c - 'A' + 10;
  } else {
    return -1;
  }
}

static Void update_cursor(Direction d)
{
  const V2S cursor_next = add_unit_vector(cursor, d);
  if (valid_point(cursor_next)) {
    cursor = cursor_next;
  }
}

static Direction arrow_direction(KeyCode code)
{
  switch (code) {
    case KEYCODE_ARROW_LEFT:
      return DIRECTION_WEST;
    case KEYCODE_ARROW_RIGHT:
      return DIRECTION_EAST;
    case KEYCODE_ARROW_UP:
      return DIRECTION_NORTH;
    case KEYCODE_ARROW_DOWN:
      return DIRECTION_SOUTH;
    default:
      return DIRECTION_NONE;
  }
}

ProgramStatus loop_config(ProgramConfig* config, const SystemInfo* system)
{
  config->title     = TITLE;
  config->caption   = TITLE;

  // determine render scale
  V2S scales;
  scales.x = system->display.x / RESX - 1;
  scales.y = system->display.y / RESY - 1;
  const S32 scale = MIN(scales.x, scales.y); 

  // set window resolution
  window_resolution = v2s_scale(v2s(RESX, RESY), scale);
  config->resolution = window_resolution;

  return PROGRAM_STATUS_LIVE;
}

ProgramStatus loop_init()
{
  // initialize audio thread and render thread
  sim_init();
  render_init(window_resolution);

  // initialize model history
  model_init(&sim_history[0]);

  // tell the render thread about the first slot
  const Message zero_message = message_alloc(0);
  message_enqueue(&alloc_queue, zero_message);

  // tell the audio thread about the rest of the array
  for (Index i = 1; i < SIM_HISTORY; i++) {
    const Message message = message_alloc(i);
    message_enqueue(&free_queue, message);
  }

  return PROGRAM_STATUS_LIVE;
}

ProgramStatus loop_video()
{
  render_frame();
  return PROGRAM_STATUS_LIVE;
}

ProgramStatus loop_audio(F32* out, Index frames)
{
  sim_step(out, frames);
  return PROGRAM_STATUS_LIVE;
}

Void loop_event(const Event* event)
{
  const KeyEvent* const key = &event->key;
  const Char c = event->character.character;

  switch (view_state) {

    case VIEW_STATE_GRID:
      {

        if (event->tag == EVENT_KEY && key->state == KEYSTATE_DOWN) {
          switch (key->code) {
            case KEYCODE_ARROW_LEFT:
              update_cursor(DIRECTION_WEST);
              break;
            case KEYCODE_ARROW_RIGHT:
              update_cursor(DIRECTION_EAST);
              break;
            case KEYCODE_ARROW_UP:
              update_cursor(DIRECTION_NORTH);
              break;
            case KEYCODE_ARROW_DOWN:
              update_cursor(DIRECTION_SOUTH);
              break;
            default: { }
          }
        }

        if (event->tag == EVENT_CHARACTER) {

          if (c == ':') {
            view_state = VIEW_STATE_CONSOLE;
          } else {
            const Value input_value = value_table[c];
            if (input_value.tag != VALUE_NONE) {
              const Message message = message_write(cursor, input_value);
              message_enqueue(&input_queue, message);
            }
            if (c == 0x08) {
              const Message message = message_write(cursor, value_none);
              message_enqueue(&input_queue, message);
            }
            const S32 lvalue = literal_of_char(c);
            if (lvalue >= 0) {
              const S32 value = literal_of_char(c);
              const Message message = message_write(cursor, value_literal(value));
              message_enqueue(&input_queue, message);
            }
          }

        }

      } break;

    case VIEW_STATE_CONSOLE:
      {

        if (event->tag == EVENT_CHARACTER) {

          if (c == '\r') {
            memset(console, 0, sizeof(console));
            console_head = 0;
            view_state = VIEW_STATE_GRID;
          } else if (c == '\b') {
            if (console_head > 0) {
              console_head -= 1;
              console[console_head] = 0;
            }
          } else {
            if (console_head < CONSOLE_BUFFER) {
              console[console_head] = c;
              console_head += 1;
            }
          }
        }

      } break;

  }

}

Void loop_terminate()
{
}
