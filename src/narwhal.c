#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "view.h"
#include "sim.h"
#include "render.h"
#include "config.h"
#include "comms.h"

#define WINDOW_TITLE "Narwhal"

// The window size will be a scalar multiple of this.
#define ATOM_X 320
#define ATOM_Y 180

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_AudioStream* stream = NULL;
static Index render_index = 0;
static View view = {0};

// queue buffers
static Index allocation_queue_buffer[MESSAGE_QUEUE_CAPACITY] = {0};

// flag for camera drag
static Bool camera_drag = false;

// half a second of audio should always be enough
static F32 stream_buffer[Config_AUDIO_SAMPLE_RATE] = {0};

#define VALUE_TABLE_CARDINAL 0x100
static Value value_table[VALUE_TABLE_CARDINAL] = {
  [ '!' ]       = { .tag = VALUE_BANG       },
  [ '+' ]       = { .tag = VALUE_ADD        },
  [ '-' ]       = { .tag = VALUE_SUB        },
  [ '*' ]       = { .tag = VALUE_MUL        },
  [ '/' ]       = { .tag = VALUE_DIV        },
  [ '=' ]       = { .tag = VALUE_EQUAL      },
  [ '>' ]       = { .tag = VALUE_GREATER    },
  [ '<' ]       = { .tag = VALUE_LESSER     },
  [ '&' ]       = { .tag = VALUE_AND        },
  [ '|' ]       = { .tag = VALUE_OR         },
  [ 'a' ]       = { .tag = VALUE_ALTER      },
  [ 'b' ]       = { .tag = VALUE_BOTTOM     },
  [ 'c' ]       = { .tag = VALUE_CLOCK      },
  [ 'd' ]       = { .tag = VALUE_DELAY      },
  [ 'h' ]       = { .tag = VALUE_HOP        },
  [ 'i' ]       = { .tag = VALUE_INTERFERE  },
  [ 'j' ]       = { .tag = VALUE_JUMP       },
  [ 'l' ]       = { .tag = VALUE_LOAD       },
  [ 'm' ]       = { .tag = VALUE_MULTIPLEX  },
  [ 'n' ]       = { .tag = VALUE_NOTE       },
  [ 'o' ]       = { .tag = VALUE_ODDMENT    },
  [ 'q' ]       = { .tag = VALUE_QUOTE      },
  [ 'r' ]       = { .tag = VALUE_RANDOM     },
  [ 's' ]       = { .tag = VALUE_STORE      },
  [ 't' ]       = { .tag = VALUE_TOP        },
  [ 'x' ]       = { .tag = VALUE_SAMPLER    },
  [ 'y' ]       = { .tag = VALUE_SYNTH      },
  [ 'z' ]       = { .tag = VALUE_MIDI       },
};

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

static Void update_cursor(Direction d)
{
  const V2S next = add_unit_vector(view.cursor, d);
  if (valid_point(next)) {
    view.cursor = next;
  }
}

static Void SDLCALL narwhal_audio(
    Void* user_data,
    SDL_AudioStream* out,
    S32 additional,
    S32 total)
{
  UNUSED_PARAMETER(user_data);
  UNUSED_PARAMETER(total);
  const S32 frame_bytes = STEREO * sizeof(F32);
  const S32 frames = additional / frame_bytes;
  sim_step(stream_buffer, frames);
  SDL_PutAudioStreamData(out, stream_buffer, frames * frame_bytes);
}

SDL_AppResult SDL_AppInit(Void** state, S32 argc, Char** argv)
{
  UNUSED_PARAMETER(state);
  UNUSED_PARAMETER(argc);
  UNUSED_PARAMETER(argv);

  SDL_SetAppMetadata(WINDOW_TITLE, "0.1.0", "org.k-monk.narwhal");

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == false) {
    SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

#ifdef REQUEST_GPU_DRIVER
  if (SDL_SetHint(SDL_HINT_RENDER_DRIVER, "gpu") == false) {
    SDL_Log("Failed to set render driver hint: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
#endif

  // get available display IDs
  S32 display_count = 0;
  SDL_DisplayID* const displays = SDL_GetDisplays(&display_count);

  // get bounds of primary display
  SDL_Rect bounds = {0};
  SDL_GetDisplayUsableBounds(displays[0], &bounds);

  // Compute the scale factor. We want to use the largest multiple of
  // (ATOM_X, ATOM_Y) that will fit on the primary display.
  V2S scales = {0};
  scales.x = bounds.w / ATOM_X;
  scales.y = bounds.h / ATOM_Y;
  const S32 scale = MIN(scales.x, scales.y);

  // free display IDs
  SDL_free(displays);

  const Bool window_status = SDL_CreateWindowAndRenderer(
      WINDOW_TITLE,     // window title
      scale * ATOM_X,   // width
      scale * ATOM_Y,   // height
      0,                // window flags
      &window,          // window handle
      &renderer);       // renderer handle
  if (window_status == false) {
    SDL_Log("Failed to create renderer: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (SDL_SetRenderVSync(renderer, 1) == false) {
    SDL_Log("Failed to enable vsync: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (SDL_StartTextInput(window) == false) {
    SDL_Log("Failed to start text input: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  ATOMIC_QUEUE_INIT(Index)(&allocation_queue, allocation_queue_buffer, MESSAGE_QUEUE_CAPACITY);

  sim_init();
  render_init(renderer);

  // initialize the model
  model_init(&sim_history[0]);

  // tell the render thread about the first slot
  ATOMIC_QUEUE_ENQUEUE(Index)(&allocation_queue, 0);

  // tell the audio thread about the rest of the array
  for (Index i = 1; i < SIM_HISTORY; i++) {
    const Message message = message_alloc(i);
    message_enqueue(&free_queue, message);
  }

  // create audio stream
  SDL_AudioSpec spec;
  spec.channels = STEREO;
  spec.format = SDL_AUDIO_F32;
  spec.freq = Config_AUDIO_SAMPLE_RATE;
  stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, narwhal_audio, NULL);
  if (stream == NULL) {
    SDL_Log("Failed to create audio stream: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  SDL_ResumeAudioStreamDevice(stream);

  return SDL_APP_CONTINUE;
}

static Void input_value(Value v)
{
  message_enqueue(&input_queue, message_write(view.cursor, v));
}

SDL_AppResult SDL_AppEvent(Void* state, SDL_Event* event)
{
  UNUSED_PARAMETER(state);

  // get active keyboard modifiers
  SDL_Keymod keymod = SDL_GetModState();

  // get key event keycode
  const SDL_Keycode keycode = event->key.key;

  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
  }

  if (event->type == SDL_EVENT_TEXT_INPUT) {
    // Presumably there are cases where this string has length > 1, but I'm not
    // sure what those cases are.
    const Char* c = event->text.text;
    while (*c) {
      const S32 literal = character_literal(*c);
      const Value value = value_table[*c];
      if (value.tag != VALUE_NONE) {
        input_value(value);
      } else if (literal >= 0) {
        input_value(value_literal(literal));
      }
      c += 1;
    }
  }

  if (event->type == SDL_EVENT_KEY_DOWN) {

    switch (keycode) {

      // arrow keys
      case SDLK_LEFT:
        update_cursor(DIRECTION_WEST);
        break;
      case SDLK_RIGHT:
        update_cursor(DIRECTION_EAST);
        break;
      case SDLK_UP:
        update_cursor(DIRECTION_NORTH);
        break;
      case SDLK_DOWN:
        update_cursor(DIRECTION_SOUTH);
        break;

      case SDLK_SPACE:
        message_enqueue(&input_queue, message_power(view.cursor));
        break;

      case SDLK_BACKSPACE:
        input_value(value_none);
        break;

    }
  }

  if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    if (event->button.button == SDL_BUTTON_LEFT) {
      camera_drag = true;
    }
  }

  if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
    if (camera_drag && event->button.button == SDL_BUTTON_LEFT) {
      camera_drag = false;
    }
  }

  if (camera_drag && event->type == SDL_EVENT_MOUSE_MOTION) {
    const V2F relative = { event->motion.xrel, event->motion.yrel };
    const V2F tile = v2f_of_v2s(render_tile_size());
    view.camera = v2f_sub(view.camera, v2f_div(relative, tile));
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(Void* state)
{
  UNUSED_PARAMETER(state);

  // empty the allocation queue
  while (ATOMIC_QUEUE_LENGTH(Index)(&allocation_queue) > 0) {
    const Index sentinel = -1;
    const Index allocation_message = ATOMIC_QUEUE_DEQUEUE(Index)(&allocation_queue, sentinel);
    ASSERT(allocation_message != sentinel);
    const Message free_message = message_alloc(render_index);
    message_enqueue(&free_queue, free_message);
    render_index = allocation_message;
  }

  // get model pointer from index
  const Model* const m = &sim_history[render_index];

  // render a frame
  render_frame(&view, m);

  return SDL_APP_CONTINUE;
}

Void SDL_AppQuit(Void* state, SDL_AppResult result)
{
  UNUSED_PARAMETER(state);
  UNUSED_PARAMETER(result);
}
