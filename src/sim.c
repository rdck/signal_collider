#include <intrin.h>
#include <math.h>
#include "sim.h"
#include "config.h"

#define SIM_PERIOD 6000
#define SIM_VOICES 0x100
#define VOICE_DURATION 12000
#define REFERENCE_TONE 440
#define REFERENCE_ROOT 33
#define STEREO 2
#define OCTAVE 12

#define SIM_PI                 3.141592653589793238f
#define SIM_TWELFTH_ROOT_TWO   1.059463094359295264f

typedef struct Voice {

  // whether the voice is playing
  Bool active;

  // pitch in semitones
  S32 pitch;

  // fractional volume
  F32 volume;

  // total duration in frames
  S32 duration;

  // elapsed frames
  Index frame;

} Voice;

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
static Voice sim_voices[SIM_VOICES] = {0};
static Index sim_voice_indices[SIM_VOICES] = {0};
static Index sim_voice_head = 0;

_Static_assert(MESSAGE_QUEUE_CAPACITY >= SIM_HISTORY);

static F32 to_hz(F32 pitch)
{
  const F32 power = pitch - REFERENCE_ROOT;
  return REFERENCE_TONE * powf(SIM_TWELFTH_ROOT_TWO, power);
}

static Index pop_voice()
{
  if (sim_voice_head > 0) {
    sim_voice_head -= 1;
    const Index index = sim_voice_indices[sim_voice_head];
    sim_voice_indices[sim_voice_head] = INDEX_NONE;
    return index;
  } else {
    return INDEX_NONE;
  }
}

static Void push_voice(Index index)
{
  if (sim_voice_head < SIM_VOICES) {
    sim_voice_indices[sim_voice_head] = index;
    sim_voice_head += 1;
  }
}

static Void clear_voice(Index index)
{
  if (index >= 0) {
    sim_voices[index] = (Voice) {0};
    push_voice(index);
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
      if (value.tag == VALUE_SYNTH) {
        Bool bang = false;
        for (Direction d = 0; d < DIRECTION_CARDINAL; d++) {
          const V2S c = add_unit_vector(origin, d);
          const Value adj = model_get(m, c);
          if (adj.tag == VALUE_BANG) {
            bang = true;
          }
        }
        if (bang) {
          const Index vi = pop_voice();
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

            Voice* const voice = &sim_voices[vi];
            voice->active = true;
            voice->frame = 0;
            voice->pitch = OCTAVE * octave + pitch;
            voice->duration = read_literal(v_duration, 0);
            // @rdk: use proper maximum value once we've sorted out literals
            voice->volume = read_literal(v_velocity, 8) / 8.f;
          }
        }
      }
    }
  }
}

static Void sim_step_voice(Index vi, F32* out, Index frames)
{
  ASSERT(vi != INDEX_NONE);
  Voice* const voice = &sim_voices[vi];
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
    clear_voice(vi);
  }
}

static Void sim_partial_step(F32* audio_out, Index frames)
{
  for (Index i = 0; i < frames; i++) {
    audio_out[STEREO * i + 0] = 0.f;
    audio_out[STEREO * i + 1] = 0.f;
  }

  // update voices
  for (Index i = 0; i < SIM_VOICES; i++) {
    Voice* const voice = &sim_voices[i];
    if (voice->active) {
      sim_step_voice(i, audio_out, frames);
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
    _mm_sfence();
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
    push_voice(i);
  }

  // initialize model
  Model* const m = &sim_history[0];
  model_init(m);
}
