#include <limits.h>
#include <ctype.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_asyncio.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_misc.h>
#include <SDL3/SDL_stdinc.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/webaudio.h>
#include <emscripten/em_math.h>
#define AUDIO_THREAD_STACK 0x1000
#define NODE_NAME "Clavier-36"
#endif

#include "sim.h"
#include "config.h"
#include "comms.h"
#include "layout.h"
#include "stb_truetype.h"
#include "dr_wav.h"
#include "font.ttf.h"

#ifdef DEBUG_ATLAS
#include "stb_image_write.h"
#endif

#define WINDOW_TITLE "Clavier-36"
#define MANUAL_URL "https://github.com/rdck/Clavier-36"

// The starting window size will be a scalar multiple of this.
#define ATOM_X 320
#define ATOM_Y 180

#define FONT_SIZE_LARGE 32 // in pixels
#define FONT_SIZE_SMALL 20 // in pixels

// font atlas parameters
#define ASCII_X 16
#define ASCII_Y 8
#define ASCII_AREA (ASCII_X * ASCII_Y)
#define MIN_CHAR '!'
#define MAX_CHAR '~'
#define COLOR_CHANNELS 4

typedef struct Font {
  V2S glyph;
  SDL_Texture* texture;
} Font;

typedef struct Texture {
  V2S dimensions;
  SDL_Texture* texture;
} Texture;

typedef struct LoadResult {
  S32 index;
} LoadResult;

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_AsyncIOQueue* io_queue = NULL;

static V2S window_size = {0};

static Font font_small = {0};
static Font font_large = {0};
static Texture waveforms[MODEL_RADIX] = {0};

static ProgramHistory program_history = {0};
static UIState ui = {
  .cursor = { MODEL_DEFAULT_X / 2, MODEL_DEFAULT_Y / 2 },
  .camera = { MODEL_DEFAULT_X / 2.f, MODEL_DEFAULT_Y / 2.f },
  .zoom = 1.f,
};
static RenderMetrics metrics = {0};

static Index render_index = 0;

static U64 frame_begin = 0;
static U64 frame_count = 0;

static S32 sample_selection_index = INDEX_NONE;

// layout buffers
static DrawRectangle draw_buffer[LAYOUT_DRAW_RECTANGLES] = {0};
static InteractionRectangle interaction_buffer[LAYOUT_INTERACTION_RECTANGLES] = {0};

// queue buffers
static Index allocation_queue_buffer[MESSAGE_QUEUE_CAPACITY] = {0};
static Index free_queue_buffer[MESSAGE_QUEUE_CAPACITY] = {0};
static ControlMessage control_queue_buffer[MESSAGE_QUEUE_CAPACITY] = {0};

// half a second of audio should always be enough
static F32 stream_buffer[Config_AUDIO_SAMPLE_RATE] = {0};

static ValueTag value_table[CHAR_MAX + 1] = {
  [ '!' ]       = VALUE_BANG       ,
  [ '+' ]       = VALUE_ADD        ,
  [ '-' ]       = VALUE_SUB        ,
  [ '*' ]       = VALUE_MUL        ,
  [ '/' ]       = VALUE_DIV        ,
  [ '=' ]       = VALUE_EQUAL      ,
  [ '>' ]       = VALUE_GREATER    ,
  [ '<' ]       = VALUE_LESSER     ,
  [ '&' ]       = VALUE_AND        ,
  [ '|' ]       = VALUE_OR         ,
  [ 'a' ]       = VALUE_ALTER      ,
  [ 'b' ]       = VALUE_BOTTOM     ,
  [ 'c' ]       = VALUE_CLOCK      ,
  [ 'd' ]       = VALUE_DELAY      ,
  [ 'h' ]       = VALUE_HOP        ,
  [ 'i' ]       = VALUE_INTERFERE  ,
  [ 'j' ]       = VALUE_JUMP       ,
  [ 'l' ]       = VALUE_LOAD       ,
  [ 'm' ]       = VALUE_MULTIPLEX  ,
  [ 'n' ]       = VALUE_NOTE       ,
  [ 'o' ]       = VALUE_ODDMENT    ,
  [ 'q' ]       = VALUE_QUOTE      ,
  [ 'r' ]       = VALUE_RANDOM     ,
  [ 's' ]       = VALUE_STORE      ,
  [ 't' ]       = VALUE_TOP        ,
  [ 'x' ]       = VALUE_SAMPLER    ,
  [ 'y' ]       = VALUE_SYNTH      ,
  [ 'z' ]       = VALUE_MIDI       ,
};

#ifdef __EMSCRIPTEN__

static U8 audio_thread_stack[AUDIO_THREAD_STACK] = {0};

Bool clavier_audio(
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
          clavier_audio,
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

static Void SDLCALL clavier_audio(
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

static ProgramHistory allocate_history(S32 length, V2S dimensions)
{
  ProgramHistory history;
  history.dimensions = dimensions;
  const S32 area = dimensions.x * dimensions.y;
  history.register_file = SDL_calloc(length, sizeof(RegisterFile));
  history.memory = SDL_calloc(length, area * sizeof(Value));
  history.graph = SDL_calloc(length, GRAPH_FACTOR * area * sizeof(GraphEdge));
  ASSERT(history.register_file);
  ASSERT(history.memory);
  ASSERT(history.graph);
  return history;
}

static Void SDLCALL sample_chosen(Void* user_data, const Char* const* file_list, S32 filter)
{
  UNUSED_PARAMETER(filter);
  UNUSED_PARAMETER(user_data);

  if (file_list) {

    // In this case, multiple selection is not meaningful.
    const Char* const path = file_list[0];

    if (path) {
      LoadResult* const load = SDL_malloc(sizeof(*load));
      load->index = sample_selection_index;
      if (SDL_LoadFileAsync(path, io_queue, load) == false) {
        SDL_Log("SDL_LoadFileAsync failed: %s", SDL_GetError());
      }
    }

  }

  // reset ui state
  ui.interaction = INTERACTION_NONE;
  sample_selection_index = INDEX_NONE;
}

static V2S atlas_coordinate(Char c)
{
  V2S out;
  out.x = c % ASCII_X;
  out.y = c / ASCII_X;
  return out;
}

static Void load_font(Font* font, S32 font_size)
{
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
  font->glyph.x = (S32) (scale * raw_advance) + 1;
  font->glyph.y = font_size;
  const S32 char_area = font->glyph.x * font->glyph.y;
  const V2S graph = v2s_mul(v2s(ASCII_X, ASCII_Y), font->glyph);

  // clear atlas
  Byte* const atlas = SDL_calloc(1, ASCII_AREA * char_area);
  ASSERT(atlas);

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
    ASSERT(w <= font->glyph.x);
    ASSERT(h <= font->glyph.y);

    const V2S p = atlas_coordinate(c);
    const V2S o = v2s_mul(p, font->glyph);
    for (S32 y = 0; y < h; y++) {
      for (S32 x = 0; x < w; x++) {
        const S32 x0 = o.x + xoff + x;
        const S32 y0 = o.y + scaled_ascent + yoff + y;
        const Bool bx = x0 >= 0 && x0 < graph.x;
        const Bool by = y0 >= 0 && y0 < graph.y;
        if (bx && by) {
          atlas[y0 * graph.x + x0] = bitmap[y * w + x];
        } else {
#ifdef LOG_ATLAS_OVERFLOW
          SDL_Log("invalid atlas coordinate: %c <%d, %d>", c, x, y);
#endif
        }
      }
    }

    stbtt_FreeBitmap(bitmap, NULL);

  }

#ifdef DEBUG_ATLAS_LINES
  // draw debug line separators in the atlas
  for (Index y = 0; y < ASCII_Y; y++) {
    for (Index x = 0; x < ASCII_X * font->glyph.x; x++) {
      atlas[y * font->glyph.y * graph.x + x] = 0xFF;
    }
  }
  for (Index x = 0; x < ASCII_X; x++) {
    for (Index y = 0; y < ASCII_Y * font->glyph.y; y++) {
      atlas[y * graph.x + x * font->glyph.x] = 0xFF;
    }
  }
#endif

#ifdef DEBUG_ATLAS
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
      ASCII_X * font->glyph.x,                  // width
      ASCII_Y * font->glyph.y,                  // height
      SDL_PIXELFORMAT_RGBA32,                   // pixel format
      channels,                                 // pixel data
      COLOR_CHANNELS * ASCII_X * font->glyph.x  // pitch
      );

  // create gpu texture
  font->texture = SDL_CreateTextureFromSurface(renderer, surface);
  ASSERT(font->texture);

  SDL_DestroySurface(surface);
  SDL_free(channels);
}

static Void SDL_SetRenderDrawColorStruct(SDL_Renderer* r, SDL_Color color)
{
  SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
}

static Void input_value(V2S cursor, Value value)
{
  const ControlMessage message = control_message_write(cursor, value);
  ATOMIC_QUEUE_ENQUEUE(ControlMessage)(&control_queue, message);
}

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
  const V2S next = add_unit_vector(ui.cursor, d);
  if (valid_point(program_history.dimensions, next)) {
    ui.cursor = next;
  }
}

static Void render_waveform(S32 index, Sound sound)
{
  ASSERT(index >= 0);
  ASSERT(index < MODEL_RADIX);

  const S32 sample_x = LAYOUT_PANEL_CHARACTERS * font_small.glyph.x;
  const S32 channel_height = font_small.glyph.y;
  const S32 sample_y = 2 * channel_height;
  const Index frames_per_pixel = sound.frames / sample_x;

  Byte* const channels = SDL_calloc(1, sample_x * sample_y);
  ASSERT(channels);

  for (Index i = 0; i < sample_x; i++) {
    for (S32 channel = 0; channel < STEREO; channel++) {

      // compute max for period
      F32 max = 0.f;
      const Index start = frames_per_pixel * i;
      for (Index j = 0; j < frames_per_pixel; j++) {
        const F32 sample = sound.samples[STEREO * (start + j) + channel];
        max = MAX(max, sample);
      }

      // fill line
      const Index line_height = MIN(channel_height, (Index) (max * channel_height));
      const Index top = channel_height * channel + (channel_height - line_height) / 2;
      for (Index j = 0; j < line_height; j++) {
        const Index k = top + j;
        channels[k * sample_x + i] = 0x37;
      }

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

  // upload texture
  waveforms[index].dimensions = v2s(sample_x, sample_y);
  waveforms[index].texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (waveforms[index].texture == NULL) {
    SDL_Log("Failed to create texture: %s", SDL_GetError());
  }

  // @rdk: free previous texture, if one exists
  SDL_free(channels);

}

SDL_AppResult SDL_AppInit(Void** state, S32 argc, Char** argv)
{
  UNUSED_PARAMETER(state);
  UNUSED_PARAMETER(argc);
  UNUSED_PARAMETER(argv);

  SDL_SetAppMetadata(WINDOW_TITLE, "0.1.0", "org.k-monk.clavier-36");

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

  // free display IDs
  SDL_free(displays);

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

  window_size.x = scale * ATOM_X;
  window_size.y = scale * ATOM_Y;

  const Bool window_status = SDL_CreateWindowAndRenderer(
      WINDOW_TITLE,         // window title
      window_size.x,        // width
      window_size.y,        // height
      SDL_WINDOW_RESIZABLE, // window flags
      &window,              // window handle
      &renderer);           // renderer handle
  if (window_status == false) {
    SDL_Log("Failed to create renderer: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  const SDL_PropertiesID properties_id = SDL_GetRendererProperties(renderer);
  if (properties_id) {
    const Char* render_driver = SDL_GetStringProperty(
        properties_id, SDL_PROP_RENDERER_NAME_STRING,
        "NONE");
    SDL_Log("render driver: %s", render_driver);
  }

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

  io_queue = SDL_CreateAsyncIOQueue();
  if (io_queue == NULL) {
    SDL_Log("Failed to create async IO queue: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  // set blend mode
  if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND) == false) {
    SDL_Log("Failed to set render draw blend mode: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  load_font(&font_small, (S32) (dpi_scaling * FONT_SIZE_SMALL));
  load_font(&font_large, (S32) (dpi_scaling * FONT_SIZE_LARGE));

  ATOMIC_QUEUE_INIT(Index)(&allocation_queue, allocation_queue_buffer, MESSAGE_QUEUE_CAPACITY);
  ATOMIC_QUEUE_INIT(Index)(&free_queue, free_queue_buffer, MESSAGE_QUEUE_CAPACITY);
  ATOMIC_QUEUE_INIT(ControlMessage)(&control_queue, control_queue_buffer, MESSAGE_QUEUE_CAPACITY);

  const V2S dimensions = { MODEL_DEFAULT_X, MODEL_DEFAULT_Y };
  program_history = allocate_history(SIM_HISTORY, dimensions);
  const ProgramHistory secondary = allocate_history(1, dimensions);
  sim_init(program_history, secondary);

  Model model = {
    .dimensions = program_history.dimensions,
    .register_file = program_history.register_file,
    .memory = program_history.memory,
  };

  // initialize the model
  model_init(&model);

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

  SDL_AudioStream* stream = NULL;
  SDL_AudioSpec spec;
  spec.channels = STEREO;
  spec.format = SDL_AUDIO_F32;
  spec.freq = Config_AUDIO_SAMPLE_RATE;
  stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, clavier_audio, NULL);
  if (stream == NULL) {
    SDL_Log("Failed to create audio stream: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  SDL_ResumeAudioStreamDevice(stream);

#endif

  return SDL_APP_CONTINUE;
}

static Void compute_layout(DrawArena* draw, InteractionArena* interaction, V2F mouse)
{
  const Index area = program_history.dimensions.x * program_history.dimensions.y;
  const Model model = {
    .dimensions = program_history.dimensions,
    .register_file = &program_history.register_file[render_index],
    .memory = &program_history.memory[render_index * area],
  };

  // get dsp pointer from index
  const DSPState* const dsp = &dsp_history[render_index];

  // get graph pointer from index
  const GraphEdge* const graph = &program_history.graph[render_index * GRAPH_FACTOR * area];

  const LayoutParameters layout_parameters = {
    .window = window_size,
    .font_small = font_small.glyph,
    .font_large = font_large.glyph,
    .mouse = mouse,
    .model = &model,
    .graph = graph,
    .dsp = dsp,
    .metrics = &metrics,
  };
  layout(draw, interaction, &ui, &layout_parameters);
}

static Void clear_text_interaction(UIState* state)
{
  memset(state->text, 0, sizeof(state->text));
  state->text_head = 0;
}

static SDL_AppResult event_handler(const SDL_Event* event)
{
  // set this to indicate termination
  SDL_AppResult status = SDL_APP_CONTINUE;

  // shorthand
  const SDL_Keycode keycode = event->key.key;
  
  // @rdk: unify with logic in layout
  const S32 tile_size = MAX(font_large.glyph.x, font_large.glyph.y);

  // get cursor position
  V2F mouse = {0};
  const SDL_MouseButtonFlags mouse_flags = SDL_GetMouseState(&mouse.x, &mouse.y);
  UNUSED_PARAMETER(mouse_flags);

  // compute layout
  DrawArena draw = {
    .head = 0,
    .buffer = draw_buffer,
  };
  InteractionArena interaction = {
    .head = 0,
    .buffer = interaction_buffer,
  };
  compute_layout(&draw, &interaction, mouse);

  // detect mouse hover
  const InteractionRectangle* hover = NULL;
  for (Index i = 0; i < interaction.head; i++) {
    const InteractionRectangle* const r = &interaction.buffer[i];
    const Bool inx = mouse.x >= r->area.origin.x && mouse.x < r->area.origin.x + r->area.size.x;
    const Bool iny = mouse.y >= r->area.origin.y && mouse.y < r->area.origin.y + r->area.size.y;
    if (inx && iny) {
      hover = r;
    }
  }

  // detect background interaction
  Bool sound_scroll = false;
  for (Index i = 0; i < interaction.head; i++) {
    const InteractionRectangle* const r = &interaction.buffer[i];
    const Bool inx = mouse.x >= r->area.origin.x && mouse.x < r->area.origin.x + r->area.size.x;
    const Bool iny = mouse.y >= r->area.origin.y && mouse.y < r->area.origin.y + r->area.size.y;
    if (inx && iny) {
      if (r->tag == INTERACTION_TAG_SOUND_SCROLL) {
        sound_scroll = true;
      }
    }
  }

  // compute hover delta
  V2F hover_position = {0};
  if (hover) {
    hover_position = v2f_sub(mouse, hover->area.origin);
  }
  
  switch (ui.interaction) {

    case INTERACTION_NONE:
      {

        switch (event->type) {

          case SDL_EVENT_TEXT_INPUT:
            {
              // Presumably there are cases where this string has length > 1,
              // but I'm not sure what those cases are.
              const Char* c = event->text.text;
              while (*c) {
                const S32 literal = character_literal(*c);
                const Value value = {
                  .tag = value_table[*c],
                  .powered = true,
                };
                if (value.tag != VALUE_NONE) {
                  input_value(ui.cursor, value);
                } else if (literal >= 0) {
                  input_value(ui.cursor, value_literal(literal));
                }
                c += 1;
              }
            } break;

          case SDL_EVENT_KEY_DOWN:
            {
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
                  ATOMIC_QUEUE_ENQUEUE(ControlMessage)(&control_queue, control_message_power(ui.cursor));
                  break;
                case SDLK_BACKSPACE:
                  input_value(ui.cursor, value_none);
                  break;
                default: { }
              }
            } break;

          case SDL_EVENT_MOUSE_BUTTON_DOWN:
            {
              if (hover && event->button.button == SDL_BUTTON_LEFT) {

                switch (hover->tag) {

                  case INTERACTION_TAG_MAP:
                    {
                      ui.interaction = INTERACTION_CAMERA;
                    } break;

                  case INTERACTION_TAG_WAVEFORM:
                    {
                      sample_selection_index = hover->index;
                      ui.interaction = INTERACTION_FILE_DIALOG;
                      SDL_ShowOpenFileDialog(
                          sample_chosen,
                          NULL,
                          NULL,
                          NULL,
                          0,
                          NULL,
                          false);
                    } break;

                  case INTERACTION_TAG_MENU:
                    {
                      ui.interaction = INTERACTION_MENU_SELECT;
                    } break;

                  case INTERACTION_TAG_MEMORY_DIMENSIONS:
                    {
                      clear_text_interaction(&ui);
                      ui.interaction = INTERACTION_MEMORY_DIMENSIONS;
                    } break;

                  case INTERACTION_TAG_TEMPO:
                    {
                      clear_text_interaction(&ui);
                      ui.interaction = INTERACTION_TEMPO;
                    } break;

                }

              }
            } break;

          case SDL_EVENT_MOUSE_WHEEL:
            {
              if (sound_scroll) {
                ui.scroll -= event->wheel.y / 8.f;
                ui.scroll = CLAMP(0.f, 1.f, ui.scroll);
              } else if (hover && hover->tag == INTERACTION_TAG_MAP) {
                ui.zoom += event->wheel.y / 20.f;
                ui.zoom = CLAMP(0.25f, 1.f, ui.zoom);
                SDL_Log("zoom: %f", ui.zoom);
              }
            } break;

          default: { }

        }

      } break;

    case INTERACTION_CAMERA:
      {
        switch (event->type) {

          case SDL_EVENT_MOUSE_BUTTON_UP:
            {
              ui.interaction = INTERACTION_NONE;
            } break;

          case SDL_EVENT_MOUSE_MOTION:
            {
              const V2F relative = { event->motion.xrel, event->motion.yrel };
              const V2F tile = { (F32) tile_size, (F32) tile_size };
              ui.camera = v2f_sub(ui.camera, v2f_div(relative, tile));
            } break;

          default: { }

        }
      } break;

    case INTERACTION_MENU_SELECT:
      {
        switch (event->type) {
          case SDL_EVENT_MOUSE_BUTTON_UP:
            {
              if (hover && event->button.button == SDL_BUTTON_LEFT) {
                if (hover->tag == INTERACTION_TAG_MENU) {
                  ui.interaction = INTERACTION_MENU;
                  ui.menu = hover->menu_item.menu;
                }
              }
            } break;
        }
      } break;

    case INTERACTION_MENU:
      {
        switch (event->type) {
          case SDL_EVENT_MOUSE_MOTION:
            {
              if (hover && hover->tag == INTERACTION_TAG_MENU) {
                if (hover->menu_item.item == 0) {
                  ui.menu = hover->menu_item.menu;
                }
              }
            } break;
          case SDL_EVENT_MOUSE_BUTTON_DOWN:
            {
              ui.interaction = INTERACTION_MENU_FINALIZE;
            } break;
        }
      } break;

    case INTERACTION_MENU_FINALIZE:
      {
        switch (event->type) {
          case SDL_EVENT_MOUSE_BUTTON_UP:
            {
              if (event->button.button == SDL_BUTTON_LEFT) {
                if (hover && hover->tag == INTERACTION_TAG_MENU) {
                  switch (hover->menu_item.menu) {
                    case MENU_FILE:
                      {
                        switch (hover->menu_item.item) {
                          case FILE_MENU_EXIT:
                            {
                              status = SDL_APP_SUCCESS;
                            } break;
                        }
                      } break;
                    case MENU_HELP:
                      {
                        switch (hover->menu_item.item) {
                          case HELP_MENU_MANUAL:
                            {
                              if (SDL_OpenURL(MANUAL_URL) == false) {
                                SDL_Log("Failed to open url: %s", SDL_GetError());
                              }
                            } break;
                        }
                      } break;
                  }
                }
                ui.interaction = INTERACTION_NONE;
                ui.menu = MENU_NONE;
              }
            } break;
        }
      } break;

    case INTERACTION_MEMORY_DIMENSIONS:
    case INTERACTION_TEMPO:
      {
        switch (event->type) {
          case SDL_EVENT_KEY_DOWN:
            {
              switch (keycode) {
                case SDLK_BACKSPACE:
                  {
                    if (ui.text_head > 0) {
                      ui.text_head -= 1;
                      ui.text[ui.text_head] = 0;
                    }
                  } break;
                case SDLK_ESCAPE:
                  {
                    ui.interaction = INTERACTION_NONE;
                  } break;
                case SDLK_RETURN:
                  {
                    switch (ui.interaction) {
                      case INTERACTION_MEMORY_DIMENSIONS:
                        {
                          // @rdk: I'm sure this is unsafe, somehow. Fix it later.
                          Char* const xstr = ui.text;
                          Char* const separator = SDL_strchr(ui.text, 'x');
                          Char* const ystr = separator + 1;
                          if (separator) {
                            *separator = 0;
                            const S32 x = SDL_atoi(xstr);
                            const S32 y = SDL_atoi(ystr);
                            if (x > 0 && y > 0) {
                              const V2S dimensions = { x, y };
                              program_history = allocate_history(SIM_HISTORY, dimensions);
                              const ProgramHistory secondary = allocate_history(1, dimensions);
                              ui.cursor.x = MIN(ui.cursor.x, x - 1);
                              ui.cursor.y = MIN(ui.cursor.y, y - 1);
                              ATOMIC_QUEUE_ENQUEUE(ControlMessage)(
                                  &control_queue,
                                  control_message_memory_resize(program_history, secondary));
                            }
                          }
                        } break;
                      case INTERACTION_TEMPO:
                        {
                          const S32 tempo = SDL_atoi(ui.text);
                          if (tempo > 0) {
                            ATOMIC_QUEUE_ENQUEUE(ControlMessage)(
                                &control_queue,
                                control_message_tempo(tempo));
                          }
                        } break;
                    }
                    ui.interaction = INTERACTION_NONE;
                  } break;
              }
            } break;
          case SDL_EVENT_TEXT_INPUT:
            {
              const Char* c = event->text.text;
              // @rdk: This should limit text input based on context.
              while (*c && ui.text_head < LAYOUT_TEXT_INPUT - 1) {
                ui.text[ui.text_head] = *c;
                ui.text_head += 1;
                c += 1;
              }
            } break;
        }
      } break;

    default: { }

  }

  return status;

}

SDL_AppResult SDL_AppEvent(Void* state, SDL_Event* event)
{
  UNUSED_PARAMETER(state);

  switch (event->type) {

    case SDL_EVENT_WINDOW_RESIZED:
      window_size.x = event->window.data1;
      window_size.y = event->window.data2;
      return SDL_APP_CONTINUE;

    case SDL_EVENT_WINDOW_SHOWN:
    case SDL_EVENT_WINDOW_HIDDEN:
    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_MOVED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_METAL_VIEW_RESIZED:
    case SDL_EVENT_WINDOW_MINIMIZED:
    case SDL_EVENT_WINDOW_MAXIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_MOUSE_ENTER:
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
    case SDL_EVENT_WINDOW_HIT_TEST:
    case SDL_EVENT_WINDOW_ICCPROF_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED:
    case SDL_EVENT_WINDOW_OCCLUDED:
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
    case SDL_EVENT_WINDOW_DESTROYED:
    case SDL_EVENT_WINDOW_HDR_STATE_CHANGED:
      return SDL_APP_CONTINUE;

    case SDL_EVENT_QUIT:
      return SDL_APP_SUCCESS;

    default:
      return event_handler(event);

  }
}

SDL_AppResult SDL_AppIterate(Void* state)
{
  UNUSED_PARAMETER(state);

  // measure time
  const U64 next_begin = SDL_GetPerformanceCounter();
  const U64 frequency = SDL_GetPerformanceFrequency();

  // process io queue
  Bool finished = false;
  while (finished == false) {

    SDL_AsyncIOOutcome outcome = {0};
    if (SDL_GetAsyncIOResult(io_queue, &outcome)) {

      ASSERT(outcome.buffer);
      ASSERT(outcome.bytes_transferred > 0);
      LoadResult* const load = outcome.userdata;

      // init drwav context
      drwav wav = {0};
      const Bool init_status = drwav_init_memory(
          &wav,
          outcome.buffer,
          outcome.bytes_transferred,
          NULL);
      ASSERT(init_status);
      ASSERT(wav.totalPCMFrameCount > 0);

      // For now, we abort on mono audio files.
      if (wav.channels == STEREO) {
        F32* const samples = SDL_malloc(wav.totalPCMFrameCount * wav.channels * sizeof(*samples));
        const U64 read = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, samples);
        ASSERT(read == wav.totalPCMFrameCount);
        const Sound sound = {
          .frames = wav.totalPCMFrameCount,
          .samples = samples,
        };
        render_waveform(load->index, sound);

        ATOMIC_QUEUE_ENQUEUE(ControlMessage)(
            &control_queue,
            control_message_sound(load->index, sound));

      } else {
        SDL_Log("wav file must be stereo");
      }

      SDL_free(load);

    } else {

      finished = true;

    }
  }

  // empty the allocation queue
  while (ATOMIC_QUEUE_LENGTH(Index)(&allocation_queue) > 0) {
    const Index sentinel = -1;
    const Index allocation_message = ATOMIC_QUEUE_DEQUEUE(Index)(&allocation_queue, sentinel);
    ASSERT(allocation_message != sentinel);
    ATOMIC_QUEUE_ENQUEUE(Index)(&free_queue, render_index);
    render_index = allocation_message;
  }

  // update render metrics
  metrics.frame_time = (next_begin - frame_begin) * MEGA / frequency;
  metrics.frame_count = frame_count;
  metrics.render_index = render_index;

  // clear
  SDL_SetRenderDrawColorFloat(renderer, 0.1f, 0.1f, 0.1f, SDL_ALPHA_OPAQUE_FLOAT);
  SDL_RenderClear(renderer);

  DrawArena draw = {
    .head = 0,
    .buffer = draw_buffer,
  };
  InteractionArena interaction = {
    .head = 0,
    .buffer = interaction_buffer,
  };

  // get cursor position
  V2F mouse = {0};
  const SDL_MouseButtonFlags mouse_flags = SDL_GetMouseState(&mouse.x, &mouse.y);
  UNUSED_PARAMETER(mouse_flags);

  compute_layout(&draw, &interaction, mouse);

  // render
  for (Index i = 0; i < draw.head; i++) {

    const DrawRectangle* const r = &draw.buffer[i];
    ASSERT(r->texture.name != TEXTURE_NONE);

    switch (r->texture.name) {

      case TEXTURE_FONT_SMALL:
      case TEXTURE_FONT_LARGE:
        {
          const Char c = r->texture.character;
          const V2S uv = atlas_coordinate(c);
          const Font* const font = r->texture.name == TEXTURE_FONT_SMALL
            ? &font_small
            : &font_large;

          // texture coordinates for the glyph
          SDL_FRect source;
          source.x = (F32) (uv.x * font->glyph.x);
          source.y = (F32) (uv.y * font->glyph.y);
          source.w = (F32) font->glyph.x;
          source.h = (F32) font->glyph.y;

          // screen coordinates to fill
          SDL_FRect destination;
          destination.x = r->area.origin.x;
          destination.y = r->area.origin.y;
          destination.w = r->area.size.x;
          destination.h = r->area.size.y;

          // draw
          SDL_SetTextureColorMod(font_large.texture, r->color.r, r->color.g, r->color.b);
          SDL_SetTextureAlphaMod(font_large.texture, r->color.a);
          SDL_RenderTexture(renderer, font->texture, &source, &destination);
        } break;

      case TEXTURE_WHITE:
        {
          // screen coordinates to fill
          SDL_FRect destination;
          destination.x = r->area.origin.x;
          destination.y = r->area.origin.y;
          destination.w = r->area.size.x;
          destination.h = r->area.size.y;

          // draw
          SDL_SetRenderDrawColorStruct(renderer, r->color);
          SDL_RenderFillRect(renderer, &destination);
        } break;

      case TEXTURE_WAVEFORM:
        {
          // texture coordinates
          SDL_FRect source;
          source.x = 0.f;
          source.y = 0.f;
          source.w = r->area.size.x;
          source.h = r->area.size.y;

          // screen coordinates to fill
          SDL_FRect destination;
          destination.x = r->area.origin.x;
          destination.y = r->area.origin.y;
          destination.w = r->area.size.x;
          destination.h = r->area.size.y;

          const Texture* const texture = &waveforms[r->texture.index];
          if (texture->texture) {
            SDL_SetTextureColorMod(texture->texture, 0xFF, 0xFF, 0xFF);
            SDL_SetTextureAlphaMod(texture->texture, 0xFF);
            SDL_RenderTexture(renderer, texture->texture, &source, &destination);
          } else {
            SDL_SetRenderDrawColorStruct(renderer, r->color);
            SDL_RenderFillRect(renderer, &destination);
          }
        } break;

      default: { }

    }

  }

  // present
  SDL_RenderPresent(renderer);

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

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#ifdef DEBUG_ATLAS
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif
