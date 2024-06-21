#include "view.h"
#include "sim.h"

V2S cursor = {0};

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

Void process_input(const InputFrame* input)
{
  // process arrow keys
  for (Index i = 0; i < MAX_INPUT_EVENTS; i++) {
    const KeyEvent* const event = &input->events[i];
    if (event->state == KEYSTATE_DOWN) {
      const Direction d = arrow_direction(event->code);
      if (d != DIRECTION_NONE) {
        update_cursor(d);
      }
    }
  }

  // process other keys
  for (Index i = 0; i < MAX_INPUT_EVENTS; i++) {
    const Char c = input->chars[i];
    const Value input_value = value_table[c];
    if (input_value.tag != VALUE_NONE) {
      const Message msg = message_write(cursor, input_value);
      message_enqueue(&input_queue, msg);
    }
    if (c == 0x08) {
      const Message msg = message_write(cursor, value_none);
      message_enqueue(&input_queue, msg);
    }
    const S32 lvalue = literal_of_char(c);
    if (lvalue >= 0) {
      const S32 value = literal_of_char(c);
      const Message msg = message_write(cursor, value_literal(value));
      message_enqueue(&input_queue, msg);
    }
  }
}
