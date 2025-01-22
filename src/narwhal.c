#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "view.h"
#include "sim.h"
#include "render.h"
#include "config.h"

#define WHAL_TITLE "Narwhal"

#define RESX 320
#define RESY 180

#define VALUE_TABLE_CARDINAL 0x100

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_AudioStream* stream = NULL;
static Index render_index = 0;
static View view = {0};

// half a second of audio should always be enough
static F32 stream_buffer[Config_AUDIO_SAMPLE_RATE] = {0};

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

  SDL_SetAppMetadata(WHAL_TITLE, "0.1.0", "org.k-monk.narwhal");

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == false) {
    SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (SDL_SetHint(SDL_HINT_RENDER_DRIVER, "gpu") == false) {
    SDL_Log("Failed to set render driver hint: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  // get available display IDs
  S32 display_count = 0;
  SDL_DisplayID* const displays = SDL_GetDisplays(&display_count);

  // get bounds of primary display
  SDL_Rect bounds = {0};
  SDL_GetDisplayUsableBounds(displays[0], &bounds);

  // compute scale
  V2S scales = {0};
  scales.x = bounds.w / RESX;
  scales.y = bounds.h / RESY;
  const S32 scale = MIN(scales.x, scales.y);

  // free display IDs
  SDL_free(displays);

  if (SDL_CreateWindowAndRenderer(WHAL_TITLE, scale * RESX, scale * RESY, 0, &window, &renderer) == false) {
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

  sim_init();
  render_init(renderer);

  // initialize the model
  model_init(&sim_history[0]);

  // tell the render thread about the first slot
  const Message zero_message = message_alloc(0);
  message_enqueue(&alloc_queue, zero_message);

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
    const Char c = event->text.text[0];
    const S32 literal = literal_of_char(c);
    const Value value = value_table[c];
    if (value.tag != VALUE_NONE) {
      input_value(value);
    } else if (literal >= 0) {
      input_value(value_literal(literal));
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

      // // literal input
      // case SDLK_0: input_value(value_literal(0)); break;
      // case SDLK_1: input_value(value_literal(1)); break;
      // case SDLK_2: input_value(value_literal(2)); break;
      // case SDLK_3: input_value(value_literal(3)); break;
      // case SDLK_4: input_value(value_literal(4)); break;
      // case SDLK_5: input_value(value_literal(5)); break;
      // case SDLK_6: input_value(value_literal(6)); break;
      // case SDLK_7: input_value(value_literal(7)); break;
      // case SDLK_8: input_value(value_literal(8)); break;
      // case SDLK_9: input_value(value_literal(9)); break;

      // // value input
      // case SDLK_C: input_value(value_clock); break;
      // case SDLK_D: input_value(value_delay); break;
      // case SDLK_M: input_value(value_multiplex); break;
      // case SDLK_X: input_value(value_sampler); break;
      // case SDLK_Y: input_value(value_synth); break;

      case SDLK_SPACE: message_enqueue(&input_queue, message_power(view.cursor)); break;

    }
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(Void* state)
{
  UNUSED_PARAMETER(state);

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

  // get model pointer from index
  const Model* const m = &sim_history[render_index];

  // render a frame
  render_frame(&view, m);

#if 0
  SDL_SetRenderDrawColorFloat(renderer, 0.f, 0.f, 0.f, SDL_ALPHA_OPAQUE_FLOAT);
  SDL_RenderClear(renderer);
  SDL_RenderPresent(renderer);
#endif

  return SDL_APP_CONTINUE;
}

Void SDL_AppQuit(Void* state, SDL_AppResult result)
{
  UNUSED_PARAMETER(state);
  UNUSED_PARAMETER(result);
}
