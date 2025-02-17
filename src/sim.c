#define SK_ENV_PRIV
#include <math.h>
#include <SDL3/SDL_log.h>
#include "sim.h"
#include "config.h"
#include "palette.h"
#include "comms.h"
#include "bigverb.h"
#include "env.h"

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

  // start time
  S32 start;

  // relative pitch
  S32 pitch;

  // fractional volume
  F32 volume;

} SamplerVoice;

// history buffers
DSPState dsp_history[SIM_HISTORY] = {0};

// palette
static Sound sim_palette[MODEL_RADIX] = {0};

// history pointers
static ProgramHistory sim_history = {0};
static ProgramHistory sim_backup = {0};

// index into model history
static Index sim_head = 0;

// frames elapsed since startup
static Index sim_frame = 0;

// global dsp parameters
static F32 sim_global_volume = 1.f;
static Bool sim_reverb_status = true;
static F32 sim_reverb_mix = 0.12f;
static F32 sim_envelope_coefficient = 0.0001f;
static F32 sim_envelope_exponent = 0.3f;
static S32 sim_tempo = 80;
static Bool sim_pause = false;

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

_Static_assert(
    MESSAGE_QUEUE_CAPACITY >= SIM_HISTORY,
    "message queue capacity must be greater than simulation history"
    );

_Static_assert(
    MODEL_RADIX == PALETTE_SOUNDS,
    "invalid palette size"
    );

static ProgramHistory lookup_history_index(Index index)
{
  const Index area = sim_history.dimensions.x * sim_history.dimensions.y;
  ProgramHistory program;
  program.dimensions = sim_history.dimensions;
  if (index >= 0) {
    // @rdk: Pull this logic out, to be shared with clavier module.
    program.register_file   = &sim_history.register_file[index];
    program.memory          = &sim_history.memory[index * area];
    program.graph           = &sim_history.graph[index * GRAPH_FACTOR * area];
  } else {
    program.register_file   = sim_backup.register_file;
    program.memory          = sim_backup.memory;
    program.graph           = sim_backup.graph;
  }
  return program;
}

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

static Void sim_step_model(Model* m, GraphEdge* graph)
{
  // const Index area = sim_history.dimensions.x * sim_history.dimensions.y;
  model_step(m, graph);

  // shorthand
  const V2S west = unit_vector(DIRECTION_WEST);

  // process synth events
  for (Index y = 0; y < sim_history.dimensions.y; y++) {
    for (Index x = 0; x < sim_history.dimensions.x; x++) {

      const V2S origin = { (S32) x, (S32) y };
      const Value value = MODEL_INDEX(m, x, y);

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

        // parameter positions
        const S32 sound_index = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 7))), INDEX_NONE);
        const S32 offset      = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 6))), 0);
        const S32 velocity    = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 5))), 0);
        const S32 attack      = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 4))), 0);
        const S32 hold        = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 3))), 0);
        const S32 release     = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 2))), 0);
        const S32 pitch       = read_literal(model_get(m, v2s_add(origin, v2s_scale(west, 1))), MODEL_RADIX / 2);

        if (voice_index != INDEX_NONE && sound_index != INDEX_NONE) {

          Sound* const sound = &sim_palette[sound_index];
          if (sound->samples) {

            ASSERT(sound->frames > 0);

            // voice to initialize
            SamplerVoice* const voice = &sim_sampler_voices[voice_index];

            // curved values
            const F32 curved_attack =
              sim_envelope_coefficient * powf(SIM_EULER, sim_envelope_exponent * attack);
            const F32 curved_hold =
              sim_envelope_coefficient * powf(SIM_EULER, sim_envelope_exponent * hold);
            const F32 curved_release =
              sim_envelope_coefficient * powf(SIM_EULER, sim_envelope_exponent * release);

            // initialize envelope
            sk_env_init(&voice->envelope, Config_AUDIO_SAMPLE_RATE);
            sk_env_attack(&voice->envelope, curved_attack);
            sk_env_hold(&voice->envelope, curved_hold);
            sk_env_release(&voice->envelope, curved_release);
            sk_env_tick(&voice->envelope, 1.f);

            // initialize parameters
            voice->start = offset;
            voice->frame = 0;
            voice->sound = sound_index;
            voice->pitch = pitch - MODEL_RADIX / 2;
            voice->volume = (F32) velocity / MODEL_RADIX;

          }
        }
      }

      // process midi event
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

static F32 sim_sampler_voice_frame(const SamplerVoice* voice, Index length)
{
  const Index start = (voice->start * length) / MODEL_RADIX;
  const F32 rate = powf(SIM_TWELFTH_ROOT_TWO, (F32) voice->pitch);
  const F32 head = start + (rate * voice->frame);
  return fmodf(head, (F32) length);
}

static Void sim_step_sampler_voice(Index voice_index, F32* out, Index frames)
{
  ASSERT(voice_index != INDEX_NONE);
  SamplerVoice* const voice = &sim_sampler_voices[voice_index];
  const Sound* const sound = &sim_palette[voice->sound];

  // We check this here because the palette can change.
  if (sound->samples) {
    ASSERT(sound->frames > 0);
    for (Index i = 0; i < frames; i++) {

      const F32 volume = sk_env_tick(&voice->envelope, 0) * voice->volume;
      const F32 playhead = sim_sampler_voice_frame(voice, sound->frames);

      // decompose
      F32 integral = 0.f;
      const F32 fractional = modff(playhead, &integral);

      const Index src = (Index) integral;
      const Index dst = (src + 1) % sound->frames;

      const F32 lhs = f32_lerp(
          sound->samples[STEREO * src + 0],
          sound->samples[STEREO * dst + 0],
          fractional);
      const F32 rhs = f32_lerp(
          sound->samples[STEREO * src + 1],
          sound->samples[STEREO * dst + 1],
          fractional);

      out[STEREO * i + 0] += volume * lhs;
      out[STEREO * i + 1] += volume * rhs;
      voice->frame += 1;

    }
  }

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

  Index nxt_head = INDEX_NONE;
  if (ATOMIC_QUEUE_LENGTH(Index)(&free_queue) > 0) {
    const Index sentinel = -1;
    nxt_head = ATOMIC_QUEUE_DEQUEUE(Index)(&free_queue, sentinel);
    ASSERT(nxt_head != sentinel);
  }

  const ProgramHistory last = lookup_history_index(sim_head);
  ProgramHistory next = lookup_history_index(nxt_head);
  const Index area = sim_history.dimensions.x * sim_history.dimensions.y;

  if (last.memory != next.memory) {
    memcpy(next.register_file , last.register_file  , sizeof(RegisterFile));
    memcpy(next.memory        , last.memory         , area * sizeof(Value));
    memcpy(next.graph         , last.graph          , GRAPH_FACTOR * area * sizeof(GraphEdge));
  }

  // the current dsp state
  DSPState backup_dsp = {0};
  DSPState* const dsp_state = nxt_head >= 0 ? &dsp_history[nxt_head] : &backup_dsp;

  // process input messages
  while (ATOMIC_QUEUE_LENGTH(ControlMessage)(&control_queue) > 0) {

    // pull a message off the queue
    ControlMessage sentinel = {0};
    const ControlMessage message = ATOMIC_QUEUE_DEQUEUE(ControlMessage)(&control_queue, sentinel);
    ASSERT(message.tag != CONTROL_MESSAGE_NONE);

    // process the message
    switch (message.tag) {

      case CONTROL_MESSAGE_WRITE:
        {
          Model model = {
            .dimensions = sim_history.dimensions,
            .register_file = next.register_file,
            .memory = next.memory,
          };
          model_set(&model, message.write.point, message.write.value);
        } break;

      case CONTROL_MESSAGE_POWER:
        {
          Model model = {
            .dimensions = sim_history.dimensions,
            .register_file = next.register_file,
            .memory = next.memory,
          };
          const V2S c = message.power.point;
          Value* const value = &MODEL_INDEX(&model, c.x, c.y);
          if (is_operator(*value)) {
            value->powered = ! value->powered;
          }
        } break;

      case CONTROL_MESSAGE_SOUND:
        {
          // @rdk: Don't forget to send a message back to the render thread.
          ASSERT(message.sound.slot >= 0);
          ASSERT(message.sound.sound.frames > 0);
          ASSERT(message.sound.sound.samples);
          sim_palette[message.sound.slot] = message.sound.sound;
        } break;

      case CONTROL_MESSAGE_TEMPO:
        {
          ASSERT(message.tempo > 0);
          sim_tempo = message.tempo;
        } break;

      case CONTROL_MESSAGE_MEMORY_RESIZE:
        {
          // @rdk: Don't forget to send a message back to the render thread.
          const ResizeMessage* const msg = &message.resize;
          const ProgramHistory previous = next;
          ASSERT(msg->primary.dimensions.x > 0);
          ASSERT(msg->primary.dimensions.y > 0);
          ASSERT(v2s_equal(msg->primary.dimensions, msg->secondary.dimensions));
          sim_history = msg->primary;
          sim_backup = msg->secondary;
          next = lookup_history_index(nxt_head);
          memcpy(next.register_file, previous.register_file, sizeof(RegisterFile));

          Model pm = {
            .dimensions = previous.dimensions,
            .register_file = previous.register_file,
            .memory = previous.memory,
          };

          Model nm = {
            .dimensions = next.dimensions,
            .register_file = next.register_file,
            .memory = next.memory,
          };

          for (Index y = 0; y < MIN(previous.dimensions.y, next.dimensions.y); y++) {
            for (Index x = 0; x < MIN(previous.dimensions.x, next.dimensions.x); x++) {
              MODEL_INDEX(&nm, x, y) = MODEL_INDEX(&pm, x, y);
            }
          }
        } break;

      case CONTROL_MESSAGE_CLEAR:
        {
          Model model = {
            .dimensions = sim_history.dimensions,
            .register_file = next.register_file,
            .memory = next.memory,
          };
          model_init(&model);
        } break;

      case CONTROL_MESSAGE_PAUSE:
        {
          sim_pause = ! sim_pause;
        } break;

      default: { }

    }

  }

  // compute the audio for this period
  const Index period = bpm_to_period(sim_tempo);
  Index elapsed = 0;
  while (elapsed < frames) {
    const Index residue = sim_frame % period;
    const Index delta = MIN(period - residue, frames - elapsed);
    if (sim_pause == false && residue == 0) {
      Model model = {
        .dimensions = sim_history.dimensions,
        .register_file = next.register_file,
        .memory = next.memory,
      };
      sim_step_model(&model, next.graph);
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

  // write dsp visualization data
  dsp_state->tempo = sim_tempo;
  for (Index i = 0; i < SIM_VOICES; i++) {
    const SamplerVoice* const voice = &sim_sampler_voices[i];
    if (voice->sound != INDEX_NONE) {
      const Index length = sim_palette[voice->sound].frames;
      dsp_state->voices[i].active = true;
      dsp_state->voices[i].sound = voice->sound;
      dsp_state->voices[i].frame = sim_sampler_voice_frame(voice, length);
      dsp_state->voices[i].length = length;
    } else {
      dsp_state->voices[i].active = false;
      dsp_state->voices[i].frame = 0;
      dsp_state->voices[i].length = 0;
      dsp_state->voices[i].sound = INDEX_NONE;
    }
  }

  // update shared pointer
  if (nxt_head >= 0) {
    ATOMIC_QUEUE_ENQUEUE(Index)(&allocation_queue, nxt_head);
  }
  sim_head = nxt_head;
}

#if 0
Void sim_step(F32* audio_out, Index frames)
{
  // clear the output buffer
  memset(audio_out, 0, STEREO * frames * sizeof(F32));

  if (ATOMIC_QUEUE_LENGTH(Index)(&free_queue) > 0) {

    // pull the next slot off the queue
    const Index sentinel = -1;
    const Index slot = ATOMIC_QUEUE_DEQUEUE(Index)(&free_queue, sentinel);
    ASSERT(slot != sentinel);

    const Index area = sim_history.dimensions.x * sim_history.dimensions.y;

    // copy memory
    Value* const memory_dst = &sim_history.memory[slot * area];
    Value* const memory_src = &sim_history.memory[sim_head * area];
    memcpy(memory_dst, memory_src, area * sizeof(Value));

    // copy register file
    memcpy(&sim_history.register_file[slot], &sim_history.register_file[sim_head], sizeof(RegisterFile));

    // copy graph
    GraphEdge* const graph_dst = &sim_history.graph[slot * GRAPH_FACTOR * area];
    GraphEdge* const graph_src = &sim_history.graph[sim_head * GRAPH_FACTOR * area];
    memcpy(graph_dst, graph_src, GRAPH_FACTOR * area * sizeof(GraphEdge));

    // update index
    sim_head = slot;

    // the current model
    Model model = {
      .dimensions = sim_history.dimensions,
      .register_file = &sim_history.register_file[sim_head],
      .memory = memory_dst,
    };
    Model* const m = &model;

    // the current dsp state
    DSPState* const dsp_state = &dsp_history[slot];

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
            Value* const value = &MODEL_INDEX(m, c.x, c.y);
            if (is_operator(*value)) {
              value->powered = ! value->powered;
            }
          } break;

        case CONTROL_MESSAGE_SOUND:
          {
            // @rdk: We should send a message back to the render thread to free
            // unused audio data.
            ASSERT(message.sound.slot >= 0);
            ASSERT(message.sound.sound.frames > 0);
            ASSERT(message.sound.sound.samples);
            sim_palette[message.sound.slot] = message.sound.sound;
          } break;

        case CONTROL_MESSAGE_TEMPO:
          {
            ASSERT(message.tempo > 0);
            sim_tempo = message.tempo;
          } break;

        case CONTROL_MESSAGE_MEMORY_RESIZE:
          {
            // ASSERT(message.resize.dimensions.x > 0);
            // ASSERT(message.resize.dimensions.y > 0);
            // SDL_Log("resize: %dx%d", message.resize.dimensions.x, message.resize.dimensions.y);
          } break;

        default: { }

      }

    }

    // write dsp visualization data
    dsp_state->tempo = sim_tempo;
    for (Index i = 0; i < SIM_VOICES; i++) {
      const SamplerVoice* const voice = &sim_sampler_voices[i];
      if (voice->sound != INDEX_NONE) {
        const Index length = sim_palette[voice->sound].frames;
        dsp_state->voices[i].active = true;
        dsp_state->voices[i].sound = voice->sound;
        dsp_state->voices[i].frame = sim_sampler_voice_frame(voice, length);
        dsp_state->voices[i].length = length;
      } else {
        dsp_state->voices[i].active = false;
        dsp_state->voices[i].frame = 0;
        dsp_state->voices[i].length = 0;
        dsp_state->voices[i].sound = INDEX_NONE;
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

  }
}
#endif

Void sim_init(ProgramHistory primary, ProgramHistory secondary)
{
  sim_history = primary;
  sim_backup = secondary;

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

