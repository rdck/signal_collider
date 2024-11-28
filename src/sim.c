#include <math.h>
#include <stdio.h> // for snprintf
#include "sim.h"
#include "config.h"
#include "dr_wav.h"
#include "log.h"
#include "file.h"

#define SIM_PERIOD 4500
#define SIM_VOICES 0x100
#define VOICE_DURATION 12000
#define SIM_SOUNDS 32 // @rdk: unify with PALETTE_SOUND_CARDINAL
#define REFERENCE_TONE 440
#define REFERENCE_ROOT 33
#define STEREO 2
#define OCTAVE 12

#define SIM_PI                 3.141592653589793238f
#define SIM_TWELFTH_ROOT_TWO   1.059463094359295264f

typedef struct SynthVoice {

  // whether the voice is playing
  Bool active;

  // pitch in semitones
  S32 pitch;

  // fractional volume
  F32 volume;

  // envelope duration
  S32 duration;

  // elapsed frames
  Index frame;

} SynthVoice;

typedef struct SamplerVoice {

  // sound index
  S32 sound;

  // envelope duration
  S32 duration;

  // elapsed frames
  Index frame;

  // fractional volume
  F32 volume;

} SamplerVoice;

typedef struct Sound {
  Index frames; // total frame count
  F32* audio_data;
} Sound;

// context during sample load
typedef struct LoadContext {
  S32 loaded;
} LoadContext;

// extern data
Model sim_history[SIM_HISTORY];

// FIFO of input messages from render thread to audio thread
MessageQueue input_queue = {0};

// FIFO of allocation messages from render thread to audio thread
MessageQueue alloc_queue = {0};

// FIFO of allocation messages from audio thread to render thread
MessageQueue free_queue = {0};

// history buffer
Model sim_history[SIM_HISTORY] = {0};

// static audio thread data
static Index sim_tick = 0;
static Index sim_head = 0;
static Index sim_frame = 0;

// synth voice data
static SynthVoice sim_synth_voices[SIM_VOICES] = {0};
static Index sim_synth_voice_indices[SIM_VOICES] = {0};
static Index sim_synth_voice_head = 0;

// sampler voice data
static SamplerVoice sim_sampler_voices[SIM_VOICES] = {0};
static Index sim_sampler_voice_indices[SIM_VOICES] = {0};
static Index sim_sampler_voice_head = 0;

// loaded samples
static Sound sim_sounds[SIM_SOUNDS] = {0};

_Static_assert(MESSAGE_QUEUE_CAPACITY >= SIM_HISTORY);

static F32 to_hz(F32 pitch)
{
  const F32 power = pitch - REFERENCE_ROOT;
  return REFERENCE_TONE * powf(SIM_TWELFTH_ROOT_TWO, power);
}

static Index pop_synth_voice()
{
  if (sim_synth_voice_head > 0) {
    sim_synth_voice_head -= 1;
    const Index index = sim_synth_voice_indices[sim_synth_voice_head];
    sim_synth_voice_indices[sim_synth_voice_head] = INDEX_NONE;
    return index;
  } else {
    return INDEX_NONE;
  }
}

static Void push_synth_voice(Index index)
{
  if (sim_synth_voice_head < SIM_VOICES) {
    sim_synth_voice_indices[sim_synth_voice_head] = index;
    sim_synth_voice_head += 1;
  }
}

static Void clear_synth_voice(Index index)
{
  if (index >= 0) {
    sim_synth_voices[index] = (SynthVoice) {0};
    push_synth_voice(index);
  }
}

static Index pop_sampler_voice()
{
  if (sim_sampler_voice_head > 0) {
    sim_sampler_voice_head -= 1;
    const Index index = sim_sampler_voice_indices[sim_sampler_voice_head];
    sim_sampler_voice_indices[sim_sampler_voice_head] = INDEX_NONE;
    return index;
  } else {
    return INDEX_NONE;
  }
}

static Void push_sampler_voice(Index index)
{
  if (sim_sampler_voice_head < SIM_VOICES) {
    sim_sampler_voice_indices[sim_sampler_voice_head] = index;
    sim_sampler_voice_head += 1;
  }
}

static Void clear_sampler_voice(Index index)
{
  if (index >= 0) {
    sim_sampler_voices[index] = (SamplerVoice) {0};
    sim_sampler_voices[index].sound = INDEX_NONE;
    push_sampler_voice(index);
  }
}

static Void sim_step_model()
{
  // step model
  Model* const m = &sim_history[sim_head];
  model_step(m);

  // process synth events
  for (Index y = 0; y < MODEL_Y; y++) {
    for (Index x = 0; x < MODEL_X; x++) {
      const V2S origin = { (S32) x, (S32) y };
      const Value value = m->map[y][x];

      // check for adjacent bang
      Bool bang = false;
      for (Direction d = 0; d < DIRECTION_CARDINAL; d++) {
        const V2S c = add_unit_vector(origin, d);
        const Value adj = model_get(m, c);
        if (adj.tag == VALUE_BANG) {
          bang = true;
        }
      }

      // process synth event
      if (value.tag == VALUE_SYNTH && bang) {
        const Index vi = pop_synth_voice();
        if (vi != INDEX_NONE) {

          const V2S uv = unit_vector(DIRECTION_EAST);

          // parameter positions
          const V2S p_octave    = v2s_add(origin, v2s_scale(uv, 1));
          const V2S p_pitch     = v2s_add(origin, v2s_scale(uv, 2));
          const V2S p_duration  = v2s_add(origin, v2s_scale(uv, 3));
          const V2S p_velocity  = v2s_add(origin, v2s_scale(uv, 4));

          // parameter values
          const Value v_octave    = model_get(m, p_octave);
          const Value v_pitch     = model_get(m, p_pitch);
          const Value v_duration  = model_get(m, p_duration);
          const Value v_velocity  = model_get(m, p_velocity);

          const S32 pitch   = read_literal(v_pitch, 0);
          const S32 octave  = read_literal(v_octave, 0);

          SynthVoice* const voice = &sim_synth_voices[vi];
          voice->active = true;
          voice->frame = 0;
          voice->pitch = OCTAVE * octave + pitch;
          voice->duration = read_literal(v_duration, 0);
          // @rdk: use proper maximum value once we've sorted out literals
          voice->volume = read_literal(v_velocity, 8) / 8.f;
        }
      }

      // process sampler event
      if (value.tag == VALUE_SAMPLER && bang) {
        const Index vi = pop_sampler_voice();
        if (vi != INDEX_NONE) {

          const V2S uv = unit_vector(DIRECTION_EAST);

          // parameter positions
          const V2S p_sound     = v2s_add(origin, v2s_scale(uv, 1));
          const V2S p_duration  = v2s_add(origin, v2s_scale(uv, 2));

          // parameter values
          const Value v_sound     = model_get(m, p_sound);
          const Value v_duration  = model_get(m, p_duration);

          // literal values
          const S32 sound_index   = read_literal(v_sound, INDEX_NONE);
          const S32 duration      = read_literal(v_duration, 0);

          if (sound_index != INDEX_NONE)
          {
            const Sound* const sound = &sim_sounds[sound_index];
            if (sound->frames > 0) {
              SamplerVoice* const voice = &sim_sampler_voices[vi];
              voice->sound = sound_index;
              voice->duration = duration;
              voice->frame = 0;
              voice->volume = 1.f;
            }
          }
        }
      }
    }
  }
}

static Void sim_step_synth_voice(Index vi, F32* out, Index frames)
{
  ASSERT(vi != INDEX_NONE);
  SynthVoice* const voice = &sim_synth_voices[vi];
  const Index duration = (voice->duration + 1) * VOICE_DURATION;
  const F32 pitch = (F32) voice->pitch;
  const F32 hz = to_hz(pitch);

  const Index remaining = duration - voice->frame;
  const Index delta = MIN(frames, remaining);

  for (Index i = 0; i < delta; i++) {
    const Index iframe = voice->frame + i;
    const F32 sin_value = sinf(hz * SIM_PI * iframe / Config_AUDIO_SAMPLE_RATE);
    const F32 volume = (duration - iframe) / (F32) duration;
    const F32 sample = sin_value * volume * voice->volume;
    out[STEREO * i + 0] += sample;
    out[STEREO * i + 1] += sample;
  }
  voice->frame += delta;
  if (delta == remaining) {
    clear_synth_voice(vi);
  }
}

static Void sim_step_sampler_voice(Index vi, F32* out, Index frames)
{
  ASSERT(vi != INDEX_NONE);
  SamplerVoice* const voice = &sim_sampler_voices[vi];
  const Sound* const sound = &sim_sounds[voice->sound];
  const Index duration = (voice->duration + 1) * VOICE_DURATION;

  for (Index i = 0; i < frames; i++) {
    const Index iframe = voice->frame + i;
    const Index wrap = iframe % sound->frames;
    // const F32 volume = voice->volume * (duration - iframe) / (F32) duration;
    const F32 volume = 1.f;
    out[STEREO * i + 0] += volume * sound->audio_data[STEREO * wrap + 0];
    out[STEREO * i + 1] += volume * sound->audio_data[STEREO * wrap + 1];
  }
  voice->frame += frames;
  if (voice->frame >= duration) {
    clear_sampler_voice(vi);
  }
}

static Void sim_partial_step(F32* audio_out, Index frames)
{
  for (Index i = 0; i < frames; i++) {
    audio_out[STEREO * i + 0] = 0.f;
    audio_out[STEREO * i + 1] = 0.f;
  }

  // update synth voices
  for (Index i = 0; i < SIM_VOICES; i++) {
    SynthVoice* const voice = &sim_synth_voices[i];
    if (voice->active) {
      sim_step_synth_voice(i, audio_out, frames);
    }
  }

  // update sampler voices
  for (Index i = 0; i < SIM_VOICES; i++) {
    SamplerVoice* const voice = &sim_sampler_voices[i];
    if (voice->sound != INDEX_NONE) {
      sim_step_sampler_voice(i, audio_out, frames);
    }
  }

  sim_frame += frames;
}

static Void process_message(Message msg)
{
  Model* const m = &sim_history[sim_head];

  if (msg.tag == MESSAGE_WRITE) {
    model_set(m, msg.write.point, msg.write.value);
  }
}

#define MAX_PATH 0x400
static Void load_sample(const Char* path, Void* user_data)
{
  LoadContext* const context = user_data;

  // extend the path
  Char extended_path[MAX_PATH] = {0};
  snprintf(extended_path, MAX_PATH, "sample/%s", path);

  // load sample
  if (context->loaded < SIM_SOUNDS) {
    U32 channels;
    U32 sample_rate;
    drwav_uint64 frame_count;
    F32* sample_data = drwav_open_file_and_read_pcm_frames_f32(extended_path, &channels, &sample_rate, &frame_count, NULL);
    if (sample_data) {
      sim_sounds[context->loaded].audio_data = sample_data;
      sim_sounds[context->loaded].frames = frame_count;
      context->loaded += 1;
    } else {
      platform_log_info("load_sample: failed to decode wav file");
    }
  }
}

Void sim_step(F32* audio_out, Index frames)
{
  const Index next_tick = sim_tick + SIM_PERIOD;
  const Index delta = MIN(frames, next_tick - sim_frame);

  Message free_message = {0};
  message_dequeue(&free_queue, &free_message);

  if (free_message.tag == MESSAGE_ALLOCATE) {

    ASSERT(free_message.alloc.index >= 0);

    // copy model
    const Index next_head = free_message.alloc.index;
    memcpy(&sim_history[next_head], &sim_history[sim_head], sizeof(Model));
    sim_head = next_head;

    while (message_queue_length(&input_queue) > 0) {
      Message msg = {0};
      message_dequeue(&input_queue, &msg);
      process_message(msg);
    }

    if (delta < frames) {
      const Index remaining = frames - delta;
      sim_partial_step(audio_out, delta);
      sim_step_model();
      sim_tick += SIM_PERIOD;
      sim_partial_step(audio_out + STEREO * delta, remaining);
    } else {
      sim_partial_step(audio_out, frames);
    }

    // update shared pointer
    const Message msg = message_alloc(sim_head);
    message_enqueue(&alloc_queue, msg);

  } else {
    memset(audio_out, 0, STEREO * frames * sizeof(F32));
  }
}

Void sim_init()
{
  // fill voice indices
  for (Index i = 0; i < SIM_VOICES; i++) {
    clear_synth_voice(i);
    clear_sampler_voice(i);
  }

  // initialize model
  Model* const m = &sim_history[0];
  model_init(m);

  // load samples from disk
  LoadContext context = {0};
  platform_enumerate_directory("sample", load_sample, &context);
}

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
