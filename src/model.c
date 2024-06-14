#include "model.h"

#define Model_TMP_CLOCK 4
#define Model_SCALE_CARDINAL 7

static V2S Model_unit_vector_table[Model_DIRECTION_CARDINAL] = {
  [ Model_DIRECTION_NORTH       ] = {  0, -1 },
  [ Model_DIRECTION_EAST        ] = {  1,  0 },
  [ Model_DIRECTION_SOUTH       ] = {  0,  1 },
  [ Model_DIRECTION_WEST        ] = { -1,  0 },
};

static S32 Model_scale_table[Model_SCALE_CARDINAL] = {
  0, 2, 4, 5, 7, 9, 11,
};

const Model_Value Model_value_none      = { 0 };
const Model_Value Model_value_bang      = { .tag = Model_VALUE_BANG };
const Model_Value Model_value_if        = { .tag = Model_VALUE_IF };
const Model_Value Model_value_clock     = { .tag = Model_VALUE_CLOCK };
const Model_Value Model_value_delay     = { .tag = Model_VALUE_DELAY };
const Model_Value Model_value_add       = { .tag = Model_VALUE_ADD };
const Model_Value Model_value_sub       = { .tag = Model_VALUE_SUB };
const Model_Value Model_value_mul       = { .tag = Model_VALUE_MUL };
const Model_Value Model_value_synth     = { .tag = Model_VALUE_SYNTH };

Model_Value Model_value_literal(S32 literal)
{
  const Model_Value out = {
    .tag = Model_VALUE_LITERAL,
    .literal = literal,
  };
  return out;
}

Bool Model_valid_coordinate(V2S c)
{
  const Bool x = c.x >= 0 && c.x < Model_X;
  const Bool y = c.y >= 0 && c.y < Model_Y;
  return x && y;
}

V2S Model_unit_vector(Model_Direction d)
{
  return Model_unit_vector_table[d];
}

V2S Model_translate_unit(V2S point, Model_Direction d)
{
  const V2S uv = Model_unit_vector(d);
  return v2s_add(point, uv);
}

Void Model_init(Model_T* m)
{

  memset(m, 0, sizeof(*m));
  rnd_pcg_seed(&m->rnd, 0u);

}

Model_Value Model_get(const Model_T* m, V2S point)
{
  const Model_Value none = {0};
  if (Model_valid_coordinate(point)) {
    return m->map[point.y][point.x];
  } else {
    return none;
  }
}

Void Model_set(Model_T* m, V2S point, Model_Value value)
{
  if (Model_valid_coordinate(point)) {
    m->map[point.y][point.x] = value;
  }
}

S32 Model_read_literal(Model_Value v, S32 none)
{
  return v.tag == Model_VALUE_LITERAL ? v.literal : none;
}

Void Model_step(Model_T* m)
{

  // clear bangs
  for (Index y = 0; y < Model_Y; y++) {
    for (Index x = 0; x < Model_X; x++) {
      const Model_Value value = m->map[y][x];
      if (value.tag == Model_VALUE_BANG) {
        m->map[y][x] = Model_value_none;
      }
    }
  }

  for (Index y = 0; y < Model_Y; y++) {
    for (Index x = 0; x < Model_X; x++) {

      const V2S origin = { (S32) x, (S32) y };
      const Model_Value value = m->map[y][x];

      V2S points[Model_DIRECTION_CARDINAL];
      Model_Value values[Model_DIRECTION_CARDINAL];
      for (Model_Direction d = 0; d < Model_DIRECTION_CARDINAL; d++) {
        points[d] = Model_translate_unit(origin, d);
        values[d] = Model_get(m, points[d]);
      }

      const V2S pn = points[Model_DIRECTION_NORTH];
      const V2S pe = points[Model_DIRECTION_EAST ];
      const V2S pw = points[Model_DIRECTION_WEST ];
      const V2S ps = points[Model_DIRECTION_SOUTH];

      const Model_Value vn = values[Model_DIRECTION_NORTH];
      const Model_Value ve = values[Model_DIRECTION_EAST ];
      const Model_Value vw = values[Model_DIRECTION_WEST ];
      const Model_Value vs = values[Model_DIRECTION_SOUTH];

      switch (value.tag) {

        case Model_VALUE_CLOCK:
          {

            const S32 rate = Model_read_literal(vw, 0) + 1;
            S32 mod = Model_read_literal(ve, 8);
            mod = mod == 0 ? 8 : mod;
            const S32 output = (m->frame / rate) % mod;
            Model_set(m, ps, Model_value_literal(output));

          } break;

        case Model_VALUE_IF:
          {
            if (ve.tag == Model_VALUE_LITERAL && vw.tag == Model_VALUE_LITERAL) {
              if (ve.literal == vw.literal) {
                Model_set(m, ps, Model_value_bang);
              }
            }
          } break;

        case Model_VALUE_DELAY:
          {
            const S32 rate = Model_read_literal(vw, 0) + 1;
            S32 mod = Model_read_literal(ve, 8);
            mod = mod == 0 ? 8 : mod;
            const S32 output = (m->frame / rate) % mod;
            if (output == 0) {
              Model_set(m, ps, Model_value_bang);
            }
          } break;

        case Model_VALUE_RANDOM:
          {
            // @rdk: abstract modulo calculation
            S32 mod = Model_read_literal(ve, 8);
            mod = mod == 0 ? 8 : mod;
            const S32 output = rnd_pcg_next(&m->rnd) % mod;
            Model_set(m, ps, Model_value_literal(output));
          } break;

        case Model_VALUE_ADD:
          {
            const S32 l = Model_read_literal(vw, 0);
            const S32 r = Model_read_literal(ve, 0);
            const S32 e = (l + r) % Model_BASE;
            Model_set(m, ps, Model_value_literal(e));
          } break;

        case Model_VALUE_SUB:
          {
            const S32 l = Model_read_literal(vw, 0);
            const S32 r = Model_read_literal(ve, 0);
            const S32 e = l - r;
            const S32 rem = e < 0 ? e + Model_BASE : e;
            Model_set(m, ps, Model_value_literal(rem));
          } break;

        case Model_VALUE_MUL:
          {
            const S32 l = Model_read_literal(vw, 0);
            const S32 r = Model_read_literal(ve, 0);
            const S32 e = (l * r) % Model_BASE;
            Model_set(m, ps, Model_value_literal(e));
          } break;

        case Model_VALUE_GENERATE:
          {
            const V2S wuv = Model_unit_vector(Model_DIRECTION_WEST);
            const V2S p_x = v2s_add(origin, v2s_scale(wuv, 2));
            const V2S p_y = v2s_add(origin, v2s_scale(wuv, 1));
            const Model_Value xval = Model_get(m, p_x);
            const Model_Value yval = Model_get(m, p_y);
            const S32 xv = Model_read_literal(xval, 0);
            const S32 yv = Model_read_literal(yval, 0);
            const V2S dest = v2s_add(origin, v2s(xv, yv));
            if (v2s_equal(dest, origin) == false) {
              if (ve.tag != Model_VALUE_NONE) {
                Model_set(m, dest, ve);
              }
            }
          } break;

        case Model_VALUE_SCALE:
          {
            const S32 index   = Model_read_literal(ve, 0);
            const S32 octave  = index / Model_SCALE_CARDINAL;
            const S32 note    = index % Model_SCALE_CARDINAL;
            const S32 pitch   = (12 * octave + Model_scale_table[note]) % Model_BASE;
            Model_set(m, ps, Model_value_literal(pitch));
          } break;

      }

    }
  }

  m->frame += 1;

}

#define RND_IMPLEMENTATION
#include "rnd.h"
