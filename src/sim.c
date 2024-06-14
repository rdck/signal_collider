#include <intrin.h>
#include <math.h>
#include "sim.h"
#include "config.h"

#define View_INDEX_NONE (-1)
#define View_PERIOD 6000
#define View_HISTORY 0x100
#define View_VOICES 0x100
#define View_VOICE_DURATION 12000
#define View_REFERENCE_TONE 440
#define View_REFERENCE_ROOT 33

#define View_PI                 3.141592653589793238f
#define View_TWELFTH_ROOT_TWO   1.059463094359295264f

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
V2S sim_cursor = {0};
Model_T* sim_model = NULL;

// FIFO of messages from render thread to audio thread
static Message_Queue View_queue;

// static audio thread data
static Index View_tick = 0;
static Index View_head = 0;
static Index View_frame = 0;
static Model_T View_history[View_HISTORY] = {0};
static Voice View_voices[View_VOICES] = {0};
static Index View_voice_indices[View_VOICES] = {0};
static Index View_voice_head = 0;

static Model_Value View_input_value_table[0xFF] = {
  [ '=' ]       = { .tag = Model_VALUE_IF         },
  [ '~' ]       = { .tag = Model_VALUE_SYNTH      },
  [ 'c' ]       = { .tag = Model_VALUE_CLOCK      },
  [ 'd' ]       = { .tag = Model_VALUE_DELAY      },
  [ 'r' ]       = { .tag = Model_VALUE_RANDOM     },
  [ '!' ]       = { .tag = Model_VALUE_GENERATE   },
  [ '#' ]       = { .tag = Model_VALUE_SCALE      },
  [ '+' ]       = { .tag = Model_VALUE_ADD        },
  [ '-' ]       = { .tag = Model_VALUE_SUB        },
  [ '*' ]       = { .tag = Model_VALUE_MUL        },
};

static Void sim_snapshot(Model_T* m)
{
  _mm_sfence();
  sim_model = m;
}

static F32 View_to_hz(F32 pitch)
{
  const F32 power = pitch - View_REFERENCE_ROOT;
  return View_REFERENCE_TONE * powf(View_TWELFTH_ROOT_TWO, power);
}

static Index View_pop_voice()
{
  if (View_voice_head > 0) {
    View_voice_head -= 1;
    const Index index = View_voice_indices[View_voice_head];
    View_voice_indices[View_voice_head] = View_INDEX_NONE;
    return index;
  } else {
    return View_INDEX_NONE;
  }
}

static Void View_push_voice(Index index)
{
  if (View_voice_head < View_VOICES) {
    View_voice_indices[View_voice_head] = index;
    View_voice_head += 1;
  }
}

static Void View_clear_voice(Index index)
{
  if (index >= 0) {
    View_voices[index] = (Voice) {0};
    View_push_voice(index);
  }
}

static Void View_step_model()
{

  // step model
  Model_T* const m = &View_history[View_head];
  Model_step(m);

  // process synth events
  for (Index y = 0; y < Model_Y; y++) {
    for (Index x = 0; x < Model_X; x++) {
      const V2S origin = { (S32) x, (S32) y };
      const Model_Value value = m->map[y][x];
      if (value.tag == Model_VALUE_SYNTH) {
        Bool bang = false;
        for (Model_Direction d = 0; d < Model_DIRECTION_CARDINAL; d++) {
          const V2S c = Model_translate_unit(origin, d);
          const Model_Value adj = Model_get(m, c);
          if (adj.tag == Model_VALUE_BANG) {
            bang = true;
          }
        }
        if (bang) {
          const Index vi = View_pop_voice();
          if (vi != View_INDEX_NONE) {

            const V2S uv = Model_unit_vector(Model_DIRECTION_EAST);

            // parameter positions
            const V2S p_octave    = v2s_add(origin, v2s_scale(uv, 1));
            const V2S p_pitch     = v2s_add(origin, v2s_scale(uv, 2));
            const V2S p_duration  = v2s_add(origin, v2s_scale(uv, 3));
            const V2S p_velocity  = v2s_add(origin, v2s_scale(uv, 4));

            // parameter values
            const Model_Value v_octave    = Model_get(m, p_octave);
            const Model_Value v_pitch     = Model_get(m, p_pitch);
            const Model_Value v_duration  = Model_get(m, p_duration);
            const Model_Value v_velocity  = Model_get(m, p_velocity);

            const S32 pitch   = Model_read_literal(v_pitch, 0);
            const S32 octave  = Model_read_literal(v_octave, 0);

            Voice* const voice = &View_voices[vi];
            voice->active = true;
            voice->frame = 0;
            voice->pitch = 12 * octave + pitch;
            voice->duration = Model_read_literal(v_duration, 0);
            // @rdk: use proper maximum value once we've sorted out literals
            voice->volume = Model_read_literal(v_velocity, 8) / 8.f;
          }
        }
      }
    }
  }

}

static Void View_step_voice(Index vi, F32* out, Index frames)
{
  ASSERT(vi != View_INDEX_NONE);
  Voice* const voice = &View_voices[vi];
  const Index duration = (voice->duration + 1) * View_VOICE_DURATION;
  const F32 pitch = (F32) voice->pitch;
  const F32 hz = View_to_hz(pitch);

  const Index remaining = duration - voice->frame;
  const Index delta = MIN(frames, remaining);

  for (Index i = 0; i < delta; i++) {
    const Index iframe = voice->frame + i;
    const F32 sin_value = sinf(hz * View_PI * iframe / Config_AUDIO_SAMPLE_RATE);
    const F32 volume = (duration - iframe) / (F32) duration;
    const F32 sample = sin_value * volume * voice->volume;
    out[i * 2 + 0] += sample;
    out[i * 2 + 1] += sample;
  }
  voice->frame += delta;
  if (delta == remaining) {
    View_clear_voice(vi);
  }
}

static Void View_partial_step(F32* audio_out, Index frames)
{

  for (Index i = 0; i < frames; i++) {
    audio_out[2 * i + 0] = 0.f;
    audio_out[2 * i + 1] = 0.f;
  }

  // update voices
  for (Index i = 0; i < View_VOICES; i++) {
    Voice* const voice = &View_voices[i];
    if (voice->active) {
      View_step_voice(i, audio_out, frames);
    }
  }

  View_frame += frames;

}

static Void View_process_message(Message_Message msg)
{

  Model_T* const m = &View_history[View_head];

  if (msg.tag == Model_MESSAGE_SET) {
    Model_set(m, msg.point, msg.value);
  }

}

Void View_step(F32* audio_out, Index frames)
{

  const Index next_tick = View_tick + View_PERIOD;
  const Index delta = MIN(frames, next_tick - View_frame);

  // copy model
  const Index next_head = (View_head + 1) % View_HISTORY;
  memcpy(&View_history[next_head], &View_history[View_head], sizeof(Model_T));
  View_head = next_head;

  while (Message_queue_length(&View_queue) > 0) {
    Message_Message msg = {0};
    Message_dequeue(&View_queue, &msg);
    View_process_message(msg);
  }

  if (delta < frames) {
    const Index remaining = frames - delta;
    View_partial_step(audio_out, delta);
    View_step_model();
    View_tick += View_PERIOD;
    View_partial_step(audio_out + 2 * delta, remaining);
  } else {
    View_partial_step(audio_out, frames);
  }

  // update shared pointer
  sim_snapshot(&View_history[next_head]);

}

Void View_init()
{

  // fill voice indices
  for (Index i = 0; i < View_VOICES; i++) {
    View_push_voice(i);
  }

  // initialize model
  Model_T* const m = &View_history[0];
  Model_init(m);

  // update shared pointer
  sim_snapshot(m);

}

static Void View_update_cursor(Model_Direction d)
{
  const V2S cursor_next = Model_translate_unit(sim_cursor, d);
  if (Model_valid_coordinate(cursor_next)) {
    sim_cursor = cursor_next;
  }
}

static S32 View_literal_of_char(Char c)
{
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'A' && c <= 'Z') {
    return c - 'A' + 10;
  } else {
    return -1;
  }
}

Void View_update(const Input_Frame* input)
{

  for (Index i = 0; i < Input_MAX_EVENTS; i++) {
    const Input_KeyEvent* const event = &input->events[i];
    if (event->state == Input_KEYSTATE_DOWN) {
      switch (event->code) {
        case Input_KEYCODE_ARROW_UP:
          {
            View_update_cursor(Model_DIRECTION_NORTH);
          } break;
        case Input_KEYCODE_ARROW_RIGHT:
          {
            View_update_cursor(Model_DIRECTION_EAST);
          } break;
        case Input_KEYCODE_ARROW_DOWN:
          {
            View_update_cursor(Model_DIRECTION_SOUTH);
          } break;
        case Input_KEYCODE_ARROW_LEFT:
          {
            View_update_cursor(Model_DIRECTION_WEST);
          } break;
      }
    }
  }

  for (Index i = 0; i < Input_MAX_EVENTS; i++) {
    const Char c = input->chars[i];
    const Model_Value input_value = View_input_value_table[c];
    if (input_value.tag != Model_VALUE_NONE) {
      Message_Message msg;
      msg.tag = Model_MESSAGE_SET;
      msg.point = sim_cursor;
      msg.value = input_value;
      Message_enqueue(&View_queue, msg);
    }
    if (c == 0x08) {
      Message_Message msg;
      msg.tag = Model_MESSAGE_SET;
      msg.point = sim_cursor;
      msg.value = Model_value_none;
      Message_enqueue(&View_queue, msg);
    }
    const S32 lvalue = View_literal_of_char(c);
    if (lvalue >= 0) {
      const S32 value = View_literal_of_char(c);
      Message_Message msg;
      msg.tag = Model_MESSAGE_SET;
      msg.point = sim_cursor;
      msg.value = Model_value_literal(value);
      Message_enqueue(&View_queue, msg);
    }
  }

}
