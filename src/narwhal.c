#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/webaudio.h>
#include <emscripten/em_math.h>
#define AUDIO_THREAD_STACK 0x1000
#define NODE_NAME "Narwhal"
#endif

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

static U64 frame_begin = 0;
static U64 frame_count = 0;

// queue buffers
static Index allocation_queue_buffer[MESSAGE_QUEUE_CAPACITY] = {0};
static Index free_queue_buffer[MESSAGE_QUEUE_CAPACITY] = {0};
static ControlMessage control_queue_buffer[MESSAGE_QUEUE_CAPACITY] = {0};

// half a second of audio should always be enough
static F32 stream_buffer[Config_AUDIO_SAMPLE_RATE] = {0};

#ifdef __EMSCRIPTEN__

static U8 audio_thread_stack[AUDIO_THREAD_STACK] = {0};

Bool narwhal_audio(
    S32 input_count,
    const AudioSampleFrame* inputs,
    S32 output_count,
    AudioSampleFrame* outputs,
    S32 parameter_count,
    const AudioParamFrame* params,
    Void* user_data)
{
  // For now, we'll assume we only need the first output.
  const AudioSampleFrame output = outputs[0];
  sim_step(stream_buffer, output.samplesPerChannel);
  for (S32 channel = 0; channel < output.numberOfChannels && channel < STEREO; channel++) {
    for (S32 i = 0; i < output.samplesPerChannel; i++) {
      output.data[channel * output.samplesPerChannel + i] = stream_buffer[STEREO * i + channel];
    }
  }
  return true;
}

Bool on_canvas_click(S32 event_type, const EmscriptenMouseEvent* mouse_event, Void* user_data)
{
  EMSCRIPTEN_WEBAUDIO_T audio_context = (EMSCRIPTEN_WEBAUDIO_T) user_data;
  if (emscripten_audio_context_state(audio_context) != AUDIO_CONTEXT_STATE_RUNNING) {
    emscripten_resume_audio_context_sync(audio_context);
  }
  return false;
}

Void audio_worklet_processor_created(
    EMSCRIPTEN_WEBAUDIO_T audio_context,
    Bool success,
    Void* user_data)
{
  if (success) {

    S32 output_channel_counts[1] = { STEREO };
    EmscriptenAudioWorkletNodeCreateOptions options = {
      .numberOfInputs       = 0,
      .numberOfOutputs      = 1,
      .outputChannelCounts  = output_channel_counts,
    };

    // create node
    EMSCRIPTEN_AUDIO_WORKLET_NODE_T wasm_audio_worklet =
      emscripten_create_wasm_audio_worklet_node(
          audio_context,
          NODE_NAME,
          &options,
          narwhal_audio,
          0);

    // connect it to audio context destination
    emscripten_audio_node_connect(wasm_audio_worklet, audio_context, 0, 0);

    // resume context on mouse click
    emscripten_set_click_callback("canvas", (Void*) audio_context, 0, on_canvas_click);

  } else {

    SDL_Log("audio worklet processor creation failed");

  }
}

Void audio_thread_initialized(EMSCRIPTEN_WEBAUDIO_T context, Bool success, Void* user_data)
{
  if (success) {
    WebAudioWorkletProcessorCreateOptions opts = {
      .name = NODE_NAME,
    };
    emscripten_create_wasm_audio_worklet_processor_async(
        context,
        &opts,
        &audio_worklet_processor_created,
        0);
  } else {
    SDL_Log("audio thread initialization failed");
  }
}

#else

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

#endif

SDL_AppResult SDL_AppInit(Void** state, S32 argc, Char** argv)
{
  UNUSED_PARAMETER(state);
  UNUSED_PARAMETER(argc);
  UNUSED_PARAMETER(argv);

  SDL_SetAppMetadata(WINDOW_TITLE, "0.1.0", "org.k-monk.narwhal");

#ifdef __EMSCRIPTEN__
  const SDL_InitFlags sdl_init_flags = SDL_INIT_VIDEO;
#else
  const SDL_InitFlags sdl_init_flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO;
#endif

  if (SDL_Init(sdl_init_flags) == false) {
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
#ifdef TEN_EIGHTY
  const S32 scale = MIN(6, MIN(scales.x, scales.y));
#else
  const S32 scale = MIN(scales.x, scales.y);
#endif

  // free display IDs
  SDL_free(displays);

  const Bool window_status = SDL_CreateWindowAndRenderer(
      WINDOW_TITLE,         // window title
      scale * ATOM_X,       // width
      scale * ATOM_Y,       // height
      SDL_WINDOW_RESIZABLE, // window flags
      &window,              // window handle
      &renderer);           // renderer handle
  if (window_status == false) {
    SDL_Log("Failed to create renderer: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  const Char* render_driver = SDL_GetRenderDriver(0);
  SDL_Log("render driver: %s", render_driver);

  if (SDL_SetRenderVSync(renderer, 1) == false) {
    SDL_Log("Failed to enable vsync: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (SDL_StartTextInput(window) == false) {
    SDL_Log("Failed to start text input: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

#ifdef TEN_EIGHTY
  const F32 dpi_scaling = 1.f;
#else
  const F32 dpi_scaling = SDL_GetWindowDisplayScale(window);
#endif
  if (dpi_scaling <= 0.f) {
    SDL_Log("Failed to get window display scale: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  ATOMIC_QUEUE_INIT(Index)(&allocation_queue, allocation_queue_buffer, MESSAGE_QUEUE_CAPACITY);
  ATOMIC_QUEUE_INIT(Index)(&free_queue, free_queue_buffer, MESSAGE_QUEUE_CAPACITY);
  ATOMIC_QUEUE_INIT(ControlMessage)(&control_queue, control_queue_buffer, MESSAGE_QUEUE_CAPACITY);

  sim_init();
  view_init(&view);
  render_init(renderer, &view);

  // We can free the bitmaps once they've been uploaded to the GPU.
  SDL_free(view.font_small.bitmap);
  SDL_free(view.font_large.bitmap);

  // initialize the model
  model_init(&sim_history[0].model);

  // tell the render thread about the first slot
  ATOMIC_QUEUE_ENQUEUE(Index)(&allocation_queue, 0);

  // tell the audio thread about the rest of the array
  for (Index i = 1; i < SIM_HISTORY; i++) {
    ATOMIC_QUEUE_ENQUEUE(Index)(&free_queue, i);
  }

#ifdef __EMSCRIPTEN__

  EMSCRIPTEN_WEBAUDIO_T context = emscripten_create_audio_context(0);
  emscripten_start_wasm_audio_worklet_thread_async(
      context,
      audio_thread_stack,
      sizeof(audio_thread_stack),
      &audio_thread_initialized,
      0);

#else

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

#endif

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(Void* state, SDL_Event* event)
{
  UNUSED_PARAMETER(state);

#if 0
  // get active keyboard modifiers
  SDL_Keymod keymod = SDL_GetModState();
#endif

  view_event(&view, event);

  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(Void* state)
{
  UNUSED_PARAMETER(state);

  // measure time
  const U64 next_begin = SDL_GetPerformanceCounter();
  const U64 frequency = SDL_GetPerformanceFrequency();

  // empty the allocation queue
  while (ATOMIC_QUEUE_LENGTH(Index)(&allocation_queue) > 0) {
    const Index sentinel = -1;
    const Index allocation_message = ATOMIC_QUEUE_DEQUEUE(Index)(&allocation_queue, sentinel);
    ASSERT(allocation_message != sentinel);
    ATOMIC_QUEUE_ENQUEUE(Index)(&free_queue, render_index);
    render_index = allocation_message;
  }

  // get model pointer from index
  const ModelGraph* const model_graph = &sim_history[render_index];

  // update view
  view_step(&view);

  // render a frame
  RenderMetrics metrics;
  metrics.frame_time = (next_begin - frame_begin) * MEGA / frequency;
  metrics.frame_count = frame_count;
  metrics.render_index = render_index;
  render_frame(model_graph, &metrics);

  // update time
  frame_begin = next_begin;
  frame_count += 1;

  return SDL_APP_CONTINUE;
}

Void SDL_AppQuit(Void* state, SDL_AppResult result)
{
  UNUSED_PARAMETER(state);
  UNUSED_PARAMETER(result);
}
