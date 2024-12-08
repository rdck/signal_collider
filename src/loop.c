#include <string.h>
#include <stdlib.h>
#include "loop.h"
#include "render.h"
#include "sim.h"
#include "message.h"
#include "view.h"
#include "file.h"
#include "palette.h"
#include "log.h"
#include "dr_wav.h"

#define TITLE "Signal Collider"

#define PALETTE_TOKEN "palette"
#define SAVE_TOKEN "save"
#define LOAD_TOKEN "load"
#define QUIT_TOKEN "quit"
#define REVERB_TOKEN "reverb"
#define SIZE_TOKEN "size"
#define CUTOFF_TOKEN "cutoff"
#define MIX_TOKEN "mix"
#define ON_TOKEN "on"
#define OFF_TOKEN "off"
#define VOLUME_TOKEN "volume"
#define ENVELOPE_TOKEN "envelope"
#define COEFFICIENT_TOKEN "coefficient"
#define EXPONENT_TOKEN "exponent"
#define CLEAR_TOKEN "clear"
#define TEMPO_TOKEN "tempo"

#define COMPARE(command, token) strncmp(command, token, sizeof(token) - 1)

#define RESX 320
#define RESY 180

// view data
ViewState view_state = VIEW_STATE_GRID;
Char console[CONSOLE_BUFFER] = {0};
Index console_head = 0;
V2S cursor = {0};

static V2S window_resolution = {0};
static Index render_index = 0;
static Bool loop_exit = false;

static Value value_table[0xFF] = {
  [ '=' ]       = { .tag = VALUE_EQUAL      },
  [ '~' ]       = { .tag = VALUE_SYNTH      },
  [ 'c' ]       = { .tag = VALUE_CLOCK      },
  [ 'd' ]       = { .tag = VALUE_DELAY      },
  [ 'r' ]       = { .tag = VALUE_RANDOM     },
  [ '!' ]       = { .tag = VALUE_GENERATE   },
  [ '#' ]       = { .tag = VALUE_SCALE      },
  [ '+' ]       = { .tag = VALUE_ADD        },
  [ '-' ]       = { .tag = VALUE_SUB        },
  [ '*' ]       = { .tag = VALUE_MUL        },
  [ '/' ]       = { .tag = VALUE_DIV        },
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

static Void run_console_command(const Char* command)
{
  // palette command
  if (COMPARE(command, PALETTE_TOKEN) == 0) {

    // parse path
    const Char* const path = command + sizeof(PALETTE_TOKEN);

    // read file
    Index file_size = 0;
    Char* palette_content = (Char*) platform_read_file(path, &file_size);

    // process file if read was successful
    if (palette_content) {

      // array of paths
      Char* lines[PALETTE_SOUNDS] = {0};

      // number of lines read
      Index line_index = 0;

      // pointer to current line
      Char* line = palette_content;

      for (Index i = 0; i < file_size && line_index < PALETTE_SOUNDS; i++) {
        const Char c = palette_content[i];
        if (c == '\r') {
          palette_content[i] = 0;
        } else if (c == '\n') {
          palette_content[i] = 0;
          lines[line_index] = line;
          line_index += 1;
          line = &palette_content[i + 1];
        }
      }

      // For now, we're just allocating this permanently. Once we have more
      // detailed communication between the audio thread and the render thread,
      // we can free it when it's no longer used.
      Palette* const palette = calloc(1, sizeof(*palette));
      ASSERT(palette);

      for (Index i = 0; i < line_index; i++) {
        U32 channels = 0;
        U32 sample_rate = 0;
        U64 frames = 0;
        F32* const sample_data = drwav_open_file_and_read_pcm_frames_f32(
            lines[i],
            &channels,
            &sample_rate,
            &frames,
            NULL);
        if (sample_data) {
          palette->sounds[i].path = lines[i];
          palette->sounds[i].frames = frames;
          palette->sounds[i].interleaved = sample_data;
        }
      }

      // send the message
      Message message = message_palette(palette);
      message_enqueue(&control_queue, message);

    }

  } else if (COMPARE(command, TEMPO_TOKEN) == 0) {

    const Char* const tempo_string = command + sizeof(TEMPO_TOKEN);
    const S32 tempo = atoi(tempo_string);
    message_enqueue(&control_queue, message_tempo(tempo));

  } else if (COMPARE(command, SAVE_TOKEN) == 0) {

    // parse path
    const Char* const path = command + sizeof(SAVE_TOKEN);

    // format data for storage
    const Model* const m = &sim_history[render_index];
    ModelStorage storage = {0};
    strncpy((Char*) &storage.signature, MODEL_SIGNATURE, MODEL_SIGNATURE_BYTES);
    storage.version = MODEL_VERSION;
    memcpy(&storage.map, &m->map, sizeof(m->map));

    // write file
    const Index bytes_written = platform_write_file(path, (Byte*) &storage, sizeof(storage));
    if (bytes_written != sizeof(storage)) {
      platform_log_warn("failed to complete storage write");
    }

  } else if (COMPARE(command, LOAD_TOKEN) == 0) {

    // parse path
    const Char* const path = command + sizeof(LOAD_TOKEN);

    // read file
    Index file_size = 0;
    ModelStorage* const storage = (ModelStorage*) platform_read_file(path, &file_size);

    // queue a load if the signature matches
    if (file_size == (U64) sizeof(ModelStorage)) {
      const S32 signature = strncmp(
          (Char*) storage->signature,
          MODEL_SIGNATURE,
          MODEL_SIGNATURE_BYTES);
      if (signature == 0 && storage->version == MODEL_VERSION) {
        Message message = message_load(storage);
        message_enqueue(&load_queue, message);
      } else {
        platform_log_warn("save file has incorrect signature or version");
      }
    } else {
      platform_log_warn("save file has incorrect file size");
    }

  } else if (COMPARE(command, QUIT_TOKEN) == 0) {

    loop_exit = true;

  } else if (COMPARE(command, VOLUME_TOKEN) == 0) {

    const Char* const volume_string = command + sizeof(VOLUME_TOKEN);
    const F32 volume = (F32) atof(volume_string);
    Message message = message_global_volume(volume);
    message_enqueue(&control_queue, message);

  } else if (COMPARE(command, ENVELOPE_TOKEN) == 0) {

    const Char* subcommand = command + sizeof(ENVELOPE_TOKEN);
    if (COMPARE(subcommand, COEFFICIENT_TOKEN) == 0) {
      const Char* const coefficient_string = subcommand + sizeof(COEFFICIENT_TOKEN);
      const F32 coefficient = (F32) atof(coefficient_string);
      message_enqueue(&control_queue, message_envelope_coefficient(coefficient));
    } else if (COMPARE(subcommand, EXPONENT_TOKEN) == 0) {
      const Char* const coefficient_string = subcommand + sizeof(EXPONENT_TOKEN);
      const F32 coefficient = (F32) atof(coefficient_string);
      message_enqueue(&control_queue, message_envelope_exponent(coefficient));
    }

  } else if (COMPARE(command, REVERB_TOKEN) == 0) {

    const Char* const subcommand = command + sizeof(REVERB_TOKEN);
    if (COMPARE(subcommand, ON_TOKEN) == 0) {
      message_enqueue(&control_queue, message_reverb_status(true));
    } else if (COMPARE(subcommand, OFF_TOKEN) == 0) {
      message_enqueue(&control_queue, message_reverb_status(false));
    } else if (COMPARE(subcommand, SIZE_TOKEN) == 0) {
      const Char* const size_string = subcommand + sizeof(SIZE_TOKEN);
      const F32 size = (F32) atof(size_string);
      message_enqueue(&control_queue, message_reverb_size(size));
    } else if (COMPARE(subcommand, CUTOFF_TOKEN) == 0) {
      const Char* const cutoff_string = subcommand + sizeof(CUTOFF_TOKEN);
      const F32 cutoff = (F32) atof(cutoff_string);
      message_enqueue(&control_queue, message_reverb_cutoff(cutoff));
    } else if (COMPARE(subcommand, MIX_TOKEN) == 0) {
      const Char* const mix_string = subcommand + sizeof(MIX_TOKEN);
      const F32 mix = (F32) atof(mix_string);
      message_enqueue(&control_queue, message_reverb_mix(mix));
    }

  } else if (COMPARE(command, CLEAR_TOKEN) == 0) {

    message_enqueue(&input_queue, message_clear());

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
  // empty the allocation queue
  while (message_queue_length(&alloc_queue) > 0) {

    Message message = {0};
    message_dequeue(&alloc_queue, &message);
    ASSERT(message.tag == MESSAGE_ALLOCATE);
    ASSERT(message.alloc.index >= 0);

    const Message free_message = message_alloc(render_index);
    message_enqueue(&free_queue, free_message);
    render_index = message.alloc.index;
  }

  const Model* const m = &sim_history[render_index];
  render_frame(m);
  return loop_exit ? PROGRAM_STATUS_SUCCESS : PROGRAM_STATUS_LIVE;
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
            run_console_command(console);
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
