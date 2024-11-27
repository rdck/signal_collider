#include "model.h"
#include <string.h>

// @rdk: Should this be a shared definition?
#define OCTAVE 12

static V2S unit_vector_table[DIRECTION_CARDINAL] = {
  [ DIRECTION_NORTH       ] = {  0, -1 },
  [ DIRECTION_EAST        ] = {  1,  0 },
  [ DIRECTION_SOUTH       ] = {  0,  1 },
  [ DIRECTION_WEST        ] = { -1,  0 },
};

const Value value_none      = { 0 };
const Value value_bang      = { .tag = VALUE_BANG };
const Value value_if        = { .tag = VALUE_IF };
const Value value_clock     = { .tag = VALUE_CLOCK };
const Value value_delay     = { .tag = VALUE_DELAY };
const Value value_add       = { .tag = VALUE_ADD };
const Value value_sub       = { .tag = VALUE_SUB };
const Value value_mul       = { .tag = VALUE_MUL };
const Value value_synth     = { .tag = VALUE_SYNTH };
const Value value_sampler   = { .tag = VALUE_SAMPLER };

// semitone intervals of the major scale
#define SCALE_CARDINAL 7
static S32 scale_table[SCALE_CARDINAL] = {
  0, 2, 4, 5, 7, 9, 11,
};

// revert zero to default value
static S32 map_zero(Value value, S32 revert)
{
  const S32 literal = read_literal(value, revert);
  return literal == 0 ? revert : literal;
}

Value value_literal(S32 literal)
{
  const Value out = {
    .tag = VALUE_LITERAL,
    .literal = literal,
  };
  return out;
}

Bool valid_point(V2S c)
{
  const Bool x = c.x >= 0 && c.x < MODEL_X;
  const Bool y = c.y >= 0 && c.y < MODEL_Y;
  return x && y;
}

V2S unit_vector(Direction d)
{
  return unit_vector_table[d];
}

V2S add_unit_vector(V2S point, Direction d)
{
  const V2S uv = unit_vector(d);
  return v2s_add(point, uv);
}

Void model_init(Model* m)
{
  memset(m, 0, sizeof(*m));
  rnd_pcg_seed(&m->rnd, 0u);
}

Value model_get(const Model* m, V2S point)
{
  const Value none = {0};
  if (valid_point(point)) {
    return m->map[point.y][point.x];
  } else {
    return none;
  }
}

Void model_set(Model* m, V2S point, Value value)
{
  if (valid_point(point)) {
    m->map[point.y][point.x] = value;
  }
}

S32 read_literal(Value v, S32 none)
{
  return v.tag == VALUE_LITERAL ? v.literal : none;
}

Void model_step(Model* m)
{
  // clear bangs
  for (Index y = 0; y < MODEL_Y; y++) {
    for (Index x = 0; x < MODEL_X; x++) {
      const Value value = m->map[y][x];
      if (value.tag == VALUE_BANG) {
        m->map[y][x] = value_none;
      }
    }
  }

  // iterate in English reading order
  for (Index y = 0; y < MODEL_Y; y++) {
    for (Index x = 0; x < MODEL_X; x++) {

      const V2S origin = { (S32) x, (S32) y };
      const Value value = m->map[y][x];

      // cache adjacent coordinates and values
      V2S points[DIRECTION_CARDINAL];
      Value values[DIRECTION_CARDINAL];
      for (Direction d = 0; d < DIRECTION_CARDINAL; d++) {
        points[d] = add_unit_vector(origin, d);
        values[d] = model_get(m, points[d]);
      }

      // point abbreviations
      const V2S pn = points[DIRECTION_NORTH];
      const V2S pe = points[DIRECTION_EAST ];
      const V2S pw = points[DIRECTION_WEST ];
      const V2S ps = points[DIRECTION_SOUTH];

      // value abbreviations
      const Value vn = values[DIRECTION_NORTH];
      const Value ve = values[DIRECTION_EAST ];
      const Value vw = values[DIRECTION_WEST ];
      const Value vs = values[DIRECTION_SOUTH];

      // We won't use some of these values until additional language constructs
      // are added.
      UNUSED_PARAMETER(vn);
      UNUSED_PARAMETER(vs);
      UNUSED_PARAMETER(pe);
      UNUSED_PARAMETER(pw);
      UNUSED_PARAMETER(pn);

      switch (value.tag) {

        case VALUE_CLOCK:
          {
            const S32 rate = read_literal(vw, 0) + 1;
            const S32 mod = map_zero(ve, 8);
            const S32 output = (m->frame / rate) % mod;
            model_set(m, ps, value_literal(output));
          } break;

        case VALUE_IF:
          {
            if (ve.tag == VALUE_LITERAL && vw.tag == VALUE_LITERAL) {
              if (ve.literal == vw.literal) {
                model_set(m, ps, value_bang);
              }
            }
          } break;

        case VALUE_DELAY:
          {
            const S32 rate = read_literal(vw, 0) + 1;
            const S32 mod = map_zero(ve, 8);
            const S32 output = (m->frame / rate) % mod;
            if (output == 0) {
              model_set(m, ps, value_bang);
            }
          } break;

        case VALUE_RANDOM:
          {
            const S32 mod = map_zero(ve, 8);
            const S32 output = rnd_pcg_next(&m->rnd) % mod;
            model_set(m, ps, value_literal(output));
          } break;

        case VALUE_ADD:
          {
            const S32 l = read_literal(vw, 0);
            const S32 r = read_literal(ve, 0);
            const S32 e = (l + r) % MODEL_RADIX;
            model_set(m, ps, value_literal(e));
          } break;

        case VALUE_SUB:
          {
            const S32 l = read_literal(vw, 0);
            const S32 r = read_literal(ve, 0);
            const S32 e = l - r;
            const S32 rem = e < 0 ? e + MODEL_RADIX : e;
            model_set(m, ps, value_literal(rem));
          } break;

        case VALUE_MUL:
          {
            const S32 l = read_literal(vw, 0);
            const S32 r = read_literal(ve, 0);
            const S32 e = (l * r) % MODEL_RADIX;
            model_set(m, ps, value_literal(e));
          } break;

        case VALUE_GENERATE:
          {

            const V2S uv = unit_vector(DIRECTION_WEST);
            const V2S px = v2s_add(origin, v2s_scale(uv, 2)); // coordinate for X value
            const V2S py = v2s_add(origin, v2s_scale(uv, 1)); // coordinate for Y value
            const S32 dx = read_literal(model_get(m, px), 0);
            const S32 dy = read_literal(model_get(m, py), 0);
            const V2S dest = v2s_add(origin, v2s(dx, dy));

            // We only generate a value when the relative coordinate is
            // nonzero, so as not to overwrite the generator operator.
            if (v2s_equal(dest, origin) == false) {
              if (ve.tag != VALUE_NONE) {
                model_set(m, dest, ve);
              }
            }
          } break;

        case VALUE_SCALE:
          {
            const S32 index   = read_literal(ve, 0);
            const S32 octave  = index / SCALE_CARDINAL;
            const S32 note    = index % SCALE_CARDINAL;
            const S32 pitch   = (OCTAVE * octave + scale_table[note]) % MODEL_RADIX;
            model_set(m, ps, value_literal(pitch));
          } break;
      }
    }
  }

  m->frame += 1;
}

#define RND_IMPLEMENTATION
#include "rnd.h"
