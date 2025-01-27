#include <math.h>
#include <SDL3/SDL_log.h>

#include "sim.h"
#include "config.h"
#include "dr_wav.h"
#include "palette.h"
#include "comms.h"

#define SK_ENV_PRIV
#include "env.h"
#include "bigverb.h"

#define SIM_VOICES 0x200
#define VOICE_DURATION 12000
#define REFERENCE_TONE 440
#define REFERENCE_ROOT 33

#define SIM_PI                  3.141592653589793238f
#define SIM_TWELFTH_ROOT_TWO    1.059463094359295264f
#define SIM_EULER               2.718281828459045235f

#define REVERB_DEFAULT_SIZE 0.93f
#define REVERB_DEFAULT_CUTOFF 10000.f

// midi is not implemented yet
#define platform_midi_init(...)
#define platform_midi_note_on(...)
#define platform_midi_note_off(...)

typedef struct SynthVoice {

  // envelope state
  sk_env envelope;

  // elapsed frames
  Index frame;

  // pitch in semitones
  S32 pitch;

  // fractional volume
  F32 volume;

} SynthVoice;

typedef struct SamplerVoice {

  // envelope state
  sk_env envelope;

  // elapsed frames
  Index frame;

  // sound index
  S32 sound;

  // relative pitch
  S32 pitch;

  // fractional volume
  F32 volume;

} SamplerVoice;

// history buffer
ModelGraph sim_history[SIM_HISTORY] = {0};

// index into model history
static Index sim_head = 0;

// frames elapsed since startup
static Index sim_frame = 0;

// default empty palette
static Palette empty_palette = {0};

// palette data
static Palette* sim_palette = &empty_palette;

// global dsp parameters
static F32 sim_global_volume = 1.f;
static Bool sim_reverb_status = true;
static F32 sim_reverb_mix = 0.12f;
static F32 sim_envelope_coefficient = 0.0001f;
static F32 sim_envelope_exponent = 0.3f;
static S32 sim_tempo = 80;

// synth voice data
static SynthVoice sim_synth_voices[SIM_VOICES] = {0};
static Index sim_synth_voice_indices[SIM_VOICES] = {0};
static Index sim_synth_voice_head = 0;

// sampler voice data
static SamplerVoice sim_sampler_voices[SIM_VOICES] = {0};
static Index sim_sampler_voice_indices[SIM_VOICES] = {0};
static Index sim_sampler_voice_head = 0;

// sndkit data
static sk_bigverb* sim_bigverb = NULL;

_Static_assert(MESSAGE_QUEUE_CAPACITY >= SIM_HISTORY);
_Static_assert(MODEL_RADIX == PALETTE_SOUNDS);

static Index bpm_to_period(S32 tempo)
{
  return (Config_AUDIO_SAMPLE_RATE * 60) / (tempo * 8);
}

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
  ModelGraph* const model_graph = &sim_history[sim_head];
  Model* const m = &model_graph->model;
  model_step(m, &model_graph->graph);

  // shorthand
  const V2S west = unit_vector(DIRECTION_WEST);

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

        const Index voice_index = pop_synth_voice();
        if (voice_index != INDEX_NONE) {

          // parameter values
          const S32 octave    = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 6))), 0);
          const S32 pitch     = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 5))), 0);
          const S32 velocity  = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 4))), 0);
          const S32 attack    = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 3))), 0);
          const S32 hold      = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 2))), 0);
          const S32 release   = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 1))), 0);

          // curved values
          const F32 curved_attack =
            sim_envelope_coefficient * powf(SIM_EULER, sim_envelope_exponent * attack);
          const F32 curved_hold =
            sim_envelope_coefficient * powf(SIM_EULER, sim_envelope_exponent * hold);
          const F32 curved_release =
            sim_envelope_coefficient * powf(SIM_EULER, sim_envelope_exponent * release);

          // voice to initialize
          SynthVoice* const voice = &sim_synth_voices[voice_index];

          // initialize envelope
          sk_env_init(&voice->envelope, Config_AUDIO_SAMPLE_RATE);
          sk_env_attack(&voice->envelope, curved_attack);
          sk_env_hold(&voice->envelope, curved_hold);
          sk_env_release(&voice->envelope, curved_release);
          sk_env_tick(&voice->envelope, 1.f);

          // initialize parameters
          voice->frame = 0;
          voice->pitch = OCTAVE * octave + pitch;
          voice->volume = (F32) velocity / MODEL_RADIX;

        }
      }

      // process sampler event
      if (value.tag == VALUE_SAMPLER && bang) {

        const Index voice_index = pop_sampler_voice();
        if (voice_index != INDEX_NONE) {

          // parameter positions
          const S32 sound_index = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 7))), INDEX_NONE);
          const S32 offset      = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 6))), 0);
          const S32 velocity    = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 5))), 0);
          const S32 attack      = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 4))), 0);
          const S32 hold        = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 3))), 0);
          const S32 release     = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 2))), 0);
          const S32 pitch       = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 1))), MODEL_RADIX / 2);

          // curved values
          const F32 curved_attack =
            sim_envelope_coefficient * powf(SIM_EULER, sim_envelope_exponent * attack);
          const F32 curved_hold =
            sim_envelope_coefficient * powf(SIM_EULER, sim_envelope_exponent * hold);
          const F32 curved_release =
            sim_envelope_coefficient * powf(SIM_EULER, sim_envelope_exponent * release);

          if (sound_index != INDEX_NONE) {

            // voice to initialize
            SamplerVoice* const voice = &sim_sampler_voices[voice_index];
            PaletteSound* const sound = &sim_palette->sounds[sound_index];

            // initialize envelope
            sk_env_init(&voice->envelope, Config_AUDIO_SAMPLE_RATE);
            sk_env_attack(&voice->envelope, curved_attack);
            sk_env_hold(&voice->envelope, curved_hold);
            sk_env_release(&voice->envelope, curved_release);
            sk_env_tick(&voice->envelope, 1.f);

            // initialize parameters
            voice->frame = (offset * sound->frames) / MODEL_RADIX;
            voice->sound = sound_index;
            voice->pitch = pitch - MODEL_RADIX / 2;
            voice->volume = (F32) velocity / MODEL_RADIX;

          }
        }
      }

      // process sampler event
      if (value.tag == VALUE_MIDI && bang) {

        // parameter values
        const S32 octave    = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 5))), 0);
        const S32 pitch     = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 4))), 0);
        const S32 velocity  = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 3))), 0);
        const S32 channel   = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 2))), 0);
        const S32 device    = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 1))), 0);

        // curved values
        const U32 semitones = OCTAVE * (U32) octave + (U32) pitch;
        const U32 curved_velocity = 3 * (U32) velocity;

        // send midi message
        platform_midi_note_on(device, (U32) channel, semitones, curved_velocity);
        platform_midi_note_off(device, (U32) channel, semitones, curved_velocity);

      }
    }
  }
}

static Void sim_step_synth_voice(Index voice_index, F32* out, Index frames)
{
  ASSERT(voice_index != INDEX_NONE);
  SynthVoice* const voice = &sim_synth_voices[voice_index];
  const F32 hz = to_hz((F32) voice->pitch);

  for (Index i = 0; i < frames; i++) {
    const F32 volume = sk_env_tick(&voice->envelope, 0);
    const Index current_frame = voice->frame + i;
    const F32 sample = sinf(hz * SIM_PI * current_frame / Config_AUDIO_SAMPLE_RATE);
    out[STEREO * i + 0] += sample * volume * voice->volume;
    out[STEREO * i + 1] += sample * volume * voice->volume;
  }

  voice->frame += frames;
  if (voice->envelope.mode == 0) {
    clear_synth_voice(voice_index);
  }
}

static Void sim_step_sampler_voice(Index voice_index, F32* out, Index frames)
{
  ASSERT(voice_index != INDEX_NONE);
  SamplerVoice* const voice = &sim_sampler_voices[voice_index];
  const PaletteSound* const sound = &sim_palette->sounds[voice->sound];

  // playback rate
  const F32 rate = powf(SIM_TWELFTH_ROOT_TWO, (F32) voice->pitch);

  // We check this here because the palette can change.
  if (sound->frames > 0) {
    ASSERT(sound->interleaved);
    for (Index i = 0; i < frames; i++) {
      const F32 volume = sk_env_tick(&voice->envelope, 0) * voice->volume;
      const F32 head = rate * (voice->frame + i);
      F32 integral_f32;
      const F32 fractional = modff(head, &integral_f32);
      const Index integral = (Index) integral_f32;
      if (integral < sound->frames - 1) {
        const F32 lhs = f32_lerp(
            sound->interleaved[STEREO * integral + 0],
            sound->interleaved[STEREO * integral + 2],
            fractional);
        const F32 rhs = f32_lerp(
            sound->interleaved[STEREO * integral + 1],
            sound->interleaved[STEREO * integral + 3],
            fractional);
        out[STEREO * i + 0] += volume * lhs;
        out[STEREO * i + 1] += volume * rhs;
      }
    }
  }

  voice->frame += frames;
  if (voice->envelope.mode == 0) {
    clear_sampler_voice(voice_index);
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
    if (voice->envelope.mode) {
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

Void sim_step(F32* audio_out, Index frames)
{
  // clear the output buffer
  memset(audio_out, 0, STEREO * frames * sizeof(F32));

  if (ATOMIC_QUEUE_LENGTH(Index)(&free_queue) > 0) {

    // pull the next slot off the queue
    const Index sentinel = -1;
    const Index slot = ATOMIC_QUEUE_DEQUEUE(Index)(&free_queue, sentinel);
    ASSERT(slot != sentinel);

    // copy model
    memcpy(&sim_history[slot], &sim_history[sim_head], sizeof(sim_history[0]));
    sim_head = slot;

    // the current model
    Model* const m = &sim_history[sim_head].model;

    // process input messages
    while (ATOMIC_QUEUE_LENGTH(ControlMessage)(&control_queue) > 0) {

      // pull a message off the queue
      ControlMessage control_sentinel = {0};
      const ControlMessage message = ATOMIC_QUEUE_DEQUEUE(ControlMessage)(&control_queue, control_sentinel);
      ASSERT(message.tag != CONTROL_MESSAGE_NONE);

      // process the message
      switch (message.tag) {

        case CONTROL_MESSAGE_WRITE:
          {
            model_set(m, message.write.point, message.write.value);
          } break;
        case CONTROL_MESSAGE_POWER:
          {
            const V2S c = message.power.point;
            Value* const value = &m->map[c.y][c.x];
            if (is_operator(*value)) {
              value->powered = ! value->powered;
            }
          } break;

      }

    }

    // compute the audio for this period
    const Index period = bpm_to_period(sim_tempo);
    Index elapsed = 0;
    while (elapsed < frames) {
      const Index residue = sim_frame % period;
      const Index delta = MIN(period - residue, frames - elapsed);
      if (residue == 0) {
        sim_step_model();
      }
      sim_partial_step(audio_out + STEREO * elapsed, delta);
      elapsed += delta;
    }

    // reverberate
    if (sim_reverb_status) {
      for (Index i = 0; i < frames; i++) {
        F32 lhs, rhs;
        sk_bigverb_tick(sim_bigverb, audio_out[2 * i + 0], audio_out[2 * i + 1], &lhs, &rhs);
        const F32 lhs_dry = (1.f - sim_reverb_mix) * audio_out[2 * i + 0];
        const F32 rhs_dry = (1.f - sim_reverb_mix) * audio_out[2 * i + 1];
        const F32 lhs_wet = sim_reverb_mix * lhs;
        const F32 rhs_wet = sim_reverb_mix * rhs;
        audio_out[2 * i + 0] = lhs_dry + lhs_wet;
        audio_out[2 * i + 1] = rhs_dry + rhs_wet;
      }
    }

    // attenuate
    for (Index i = 0; i < frames; i++) {
      audio_out[2 * i + 0] *= sim_global_volume;
      audio_out[2 * i + 1] *= sim_global_volume;
    }

    // update shared pointer
    ATOMIC_QUEUE_ENQUEUE(Index)(&allocation_queue, sim_head);

  } else {

    // @rdk: look into SDL log metadata
    SDL_Log("no free history buffer");

  }
}

Void sim_init()
{
  // initialize midi subsystem
  platform_midi_init();

  // fill voice indices
  for (Index i = 0; i < SIM_VOICES; i++) {
    clear_synth_voice(i);
    clear_sampler_voice(i);
  }

  // initialize sndkit bigverb
  sim_bigverb = sk_bigverb_new(Config_AUDIO_SAMPLE_RATE);
  ASSERT(sim_bigverb);
  sk_bigverb_size(sim_bigverb, REVERB_DEFAULT_SIZE);
  sk_bigverb_cutoff(sim_bigverb, REVERB_DEFAULT_CUTOFF);

  // initialize voice envelopes
  for (Index i = 0; i < SIM_VOICES; i++) {
    SynthVoice* const synth_voice = &sim_synth_voices[i];
    sk_env_init(&synth_voice->envelope, Config_AUDIO_SAMPLE_RATE);
  }
}

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
