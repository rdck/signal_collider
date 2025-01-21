#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "view.h"
#include "sim.h"
#include "render.h"
#include "config.h"

#define WHAL_TITLE "Narwhal"

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_AudioStream* stream = NULL;
static Index render_index = 0;
static View view = {0};

// half a second of audio should always be enough
static F32 stream_buffer[Config_AUDIO_SAMPLE_RATE] = {0};

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

  if (SDL_CreateWindowAndRenderer(WHAL_TITLE, 640, 480, 0, &window, &renderer) == false) {
    SDL_Log("Failed to create renderer: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (SDL_SetRenderVSync(renderer, 1) == false) {
    SDL_Log("Failed to enable vsync: %s", SDL_GetError());
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

SDL_AppResult SDL_AppEvent(Void* state, SDL_Event* event)
{
  UNUSED_PARAMETER(state);
  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
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
  render_frame(renderer, &view, m);

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
