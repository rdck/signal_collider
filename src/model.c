#include <string.h>
#include "model.h"

_Static_assert(sizeof(MODEL_SIGNATURE) == MODEL_SIGNATURE_BYTES + 1);

static V2S unit_vector_table[DIRECTION_CARDINAL] = {
  [ DIRECTION_NORTH       ] = {  0, -1 },
  [ DIRECTION_EAST        ] = {  1,  0 },
  [ DIRECTION_SOUTH       ] = {  0,  1 },
  [ DIRECTION_WEST        ] = { -1,  0 },
};

const Value value_none      = { 0 };
const Value value_bang      = { .tag = VALUE_BANG };
const Value value_add       = { .tag = VALUE_ADD };
const Value value_sub       = { .tag = VALUE_SUB };
const Value value_mul       = { .tag = VALUE_MUL };
const Value value_div       = { .tag = VALUE_DIV };
const Value value_equal     = { .tag = VALUE_EQUAL };
const Value value_greater   = { .tag = VALUE_GREATER };
const Value value_lesser    = { .tag = VALUE_LESSER };
const Value value_and       = { .tag = VALUE_AND };
const Value value_or        = { .tag = VALUE_OR };
const Value value_alter     = { .tag = VALUE_ALTER };
const Value value_bottom    = { .tag = VALUE_BOTTOM };
const Value value_clock     = { .tag = VALUE_CLOCK };
const Value value_delay     = { .tag = VALUE_DELAY };
const Value value_hop       = { .tag = VALUE_HOP };
const Value value_interfere = { .tag = VALUE_INTERFERE };
const Value value_jump      = { .tag = VALUE_JUMP };
const Value value_load      = { .tag = VALUE_LOAD };
const Value value_multiplex = { .tag = VALUE_MULTIPLEX };
const Value value_note      = { .tag = VALUE_NOTE };
const Value value_oddment   = { .tag = VALUE_ODDMENT };
const Value value_quote     = { .tag = VALUE_QUOTE };
const Value value_random    = { .tag = VALUE_RANDOM };
const Value value_store     = { .tag = VALUE_STORE };
const Value value_top       = { .tag = VALUE_TOP };
const Value value_synth     = { .tag = VALUE_SYNTH };
const Value value_sampler   = { .tag = VALUE_SAMPLER };

// We disable quotation of operators that haven't been implemented yet.
static const Bool quotation_table[VALUE_CARDINAL] = {
  [ VALUE_BANG      ] = true,
  [ VALUE_ADD       ] = true,
  [ VALUE_SUB       ] = true,
  [ VALUE_MUL       ] = true,
  [ VALUE_DIV       ] = true,
  [ VALUE_EQUAL     ] = true,
  [ VALUE_GREATER   ] = true,
  [ VALUE_LESSER    ] = true,
  [ VALUE_AND       ] = true,
  [ VALUE_OR        ] = true,
  [ VALUE_ALTER     ] = true,
  [ VALUE_BOTTOM    ] = true,
  [ VALUE_CLOCK     ] = true,
  [ VALUE_DELAY     ] = true,
  [ VALUE_E         ] = false,
  [ VALUE_F         ] = false,
  [ VALUE_G         ] = false,
  [ VALUE_HOP       ] = true,
  [ VALUE_INTERFERE ] = true,
  [ VALUE_JUMP      ] = true,
  [ VALUE_K         ] = false,
  [ VALUE_LOAD      ] = true,
  [ VALUE_MULTIPLEX ] = true,
  [ VALUE_NOTE      ] = true,
  [ VALUE_ODDMENT   ] = true,
  [ VALUE_P         ] = false,
  [ VALUE_QUOTE     ] = true,
  [ VALUE_RANDOM    ] = true,
  [ VALUE_STORE     ] = true,
  [ VALUE_TOP       ] = true,
  [ VALUE_U         ] = false,
  [ VALUE_V         ] = false,
  [ VALUE_W         ] = false,
  [ VALUE_SAMPLER   ] = true,
  [ VALUE_SYNTH     ] = true,
  [ VALUE_MIDI      ] = true,
};

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

static Void graph_set(ModelGraph* graph, V2S c, Bool value)
{
  if (valid_point(c)) {
    graph->map[c.y][c.x] = value;
  }
}

Bool is_operator(Value value)
{
  switch (value.tag) {
    case VALUE_NONE:
    case VALUE_LITERAL:
    case VALUE_BANG:
      return false;
    default:
      return true;
  }
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
  // clear bangs and pulses
  for (Index y = 0; y < MODEL_Y; y++) {
    for (Index x = 0; x < MODEL_X; x++) {
      m->map[y][x].pulse = false;
      if (m->map[y][x].tag == VALUE_BANG) {
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
      Bool bang = false;
      for (Direction d = 0; d < DIRECTION_CARDINAL; d++) {
        points[d] = add_unit_vector(origin, d);
        values[d] = model_get(m, points[d]);
        bang = bang || values[d].tag == VALUE_BANG;
      }

      // mark pulse
      if (value.powered == false && bang) {
        m->map[y][x].pulse = true;
      }
      
      const V2S east = unit_vector(DIRECTION_EAST);
      const V2S west = unit_vector(DIRECTION_WEST);

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
      
      if (value.powered || bang) {

        switch (value.tag) {

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
              const S32 lhs = read_literal(vw, 0);
              const S32 rhs = read_literal(ve, 0);
              const S32 product = (lhs * rhs) % MODEL_RADIX;
              model_set(m, ps, value_literal(product));
            } break;

          case VALUE_DIV:
            {
              const S32 dividend = read_literal(vw, 0);
              const S32 divisor = read_literal(ve, 1);
              const S32 quotient = divisor == 0 ? 0 : dividend / divisor;
              model_set(m, ps, value_literal(quotient));
            } break;

          case VALUE_EQUAL:
            {
              if (ve.tag == VALUE_LITERAL && vw.tag == VALUE_LITERAL) {
                if (ve.literal == vw.literal) {
                  model_set(m, ps, value_bang);
                }
              }
            } break;

          case VALUE_GREATER:
            {
              if (ve.tag == VALUE_LITERAL && vw.tag == VALUE_LITERAL) {
                if (vw.literal > ve.literal) {
                  model_set(m, ps, value_bang);
                }
              }
            } break;

          case VALUE_LESSER:
            {
              if (ve.tag == VALUE_LITERAL && vw.tag == VALUE_LITERAL) {
                if (vw.literal < ve.literal) {
                  model_set(m, ps, value_bang);
                }
              }
            } break;

          case VALUE_AND:
            {
              if (ve.tag == VALUE_LITERAL && vw.tag == VALUE_LITERAL) {
                model_set(m, ps, value_literal(vw.literal & ve.literal));
              } else if (ve.tag != VALUE_NONE && vw.tag != VALUE_NONE) {
                model_set(m, ps, value_bang);
              }
            } break;

          case VALUE_OR:
            {
              if (ve.tag == VALUE_LITERAL && vw.tag == VALUE_LITERAL) {
                model_set(m, ps, value_literal(vw.literal | ve.literal));
              } else if (ve.tag != VALUE_NONE || vw.tag != VALUE_NONE) {
                model_set(m, ps, value_bang);
              }
            } break;

          case VALUE_ALTER:
            {
              const S32 lhs = read_literal(model_get(m, v2s_add(origin, v2s_scale(east, 1))), 0);
              const S32 rhs = read_literal(model_get(m, v2s_add(origin, v2s_scale(east, 2))), 0);
              const S32 t = read_literal(model_get(m, pw), 0);
              const S32 scale = MODEL_RADIX - 1;
              const S32 output = ((scale - t) * lhs + t * rhs) / scale;
              model_set(m, ps, value_literal(output));
            } break;

          case VALUE_BOTTOM:
            {
              const S32 lhs = read_literal(vw, 0);
              const S32 rhs = read_literal(ve, 0);
              model_set(m, ps, value_literal(MIN(lhs, rhs)));
            } break;

          case VALUE_CLOCK:
            {
              const S32 rate = read_literal(vw, 0) + 1;
              if (m->frame % rate == 0) {
                const S32 mod = read_literal(ve, MODEL_RADIX - 1) + 1;
                const S32 output = (m->frame / rate) % mod;
                model_set(m, ps, value_literal(output));
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

          case VALUE_HOP:
            {
              model_set(m, pe, vw);
            } break;

          case VALUE_INTERFERE:
            {
              const V2S px = v2s_add(origin, v2s_scale(west, 2)); // coordinate for X value
              const V2S py = v2s_add(origin, v2s_scale(west, 1)); // coordinate for Y value
              const S32 dx = read_literal(model_get(m, px), 0);
              const S32 dy = read_literal(model_get(m, py), 0) + 1;
              const V2S dest = v2s_add(origin, v2s(dx, dy));
              model_set(m, dest, ve);
            } break;

          case VALUE_JUMP:
            {
              model_set(m, ps, vn);
            } break;

          case VALUE_LOAD:
            {
              if (vw.tag == VALUE_LITERAL) {
                const Value v = m->registers[vw.literal];
                model_set(m, ps, v);
              }
            } break;

          case VALUE_MULTIPLEX:
            {
              const S32 dx = read_literal(model_get(m, v2s_add(origin, v2s_scale(east, 1))), 0);
              const S32 dy = read_literal(model_get(m, v2s_add(origin, v2s_scale(east, 2))), 0);
              const V2S source = v2s_sub(origin, v2s(dx + 1, dy));
              const Value output = model_get(m, source);
              model_set(m, ps, output);
            } break;

          case VALUE_NOTE:
            {
              const S32 index   = read_literal(vw, 0);
              const S32 octave  = index / SCALE_CARDINAL;
              const S32 note    = index % SCALE_CARDINAL;
              const S32 pitch   = (OCTAVE * octave + scale_table[note]) % MODEL_RADIX;
              model_set(m, ps, value_literal(pitch));
            } break;

          case VALUE_ODDMENT:
            {
              const S32 dividend = read_literal(vw, 0);
              const S32 divisor = read_literal(ve, 0);
              const S32 residue = divisor == 0 ? 0 : dividend % divisor;
              model_set(m, ps, value_literal(residue));
            } break;

          case VALUE_QUOTE:
            {
              const S32 index = read_literal(vw, 0);
              const Value output = { .tag = VALUE_BANG + index };
              if (quotation_table[output.tag]) {
                model_set(m, ps, output);
              }
            } break;

          case VALUE_RANDOM:
            {
              const S32 rate = read_literal(vw, 0) + 1;
              if (m->frame % rate == 0) {
                const S32 mod = map_zero(ve, 8);
                const S32 output = rnd_pcg_next(&m->rnd) % mod;
                model_set(m, ps, value_literal(output));
              }
            } break;

          case VALUE_STORE:
            {
              if (vw.tag == VALUE_LITERAL) {
                m->registers[vw.literal] = ve;
              }
            } break;

          case VALUE_TOP:
            {
              const S32 lhs = read_literal(vw, 0);
              const S32 rhs = read_literal(ve, 0);
              model_set(m, ps, value_literal(MAX(lhs, rhs)));
            } break;
        }
      }
    }
  }

  m->frame += 1;
}

// I would like if we could come up with a data structure that describes the
// memory used by each operator, but without reflection this seems difficult.
// Short of that, we have to keep this in sync with the logic in model_step.
Void model_graph(ModelGraph* graph, const Model* m)
{
  // clear graph to zero
  memset(graph, 0, sizeof(*graph));

  for (S32 y = 0; y < MODEL_Y; y++) {
    for (S32 x = 0; x < MODEL_X; x++) {

      // the occupying value
      const V2S origin = { x, y };
      const Value value = model_get(m, origin);

      // common vectors
      const V2S north = unit_vector(DIRECTION_NORTH);
      const V2S east  = unit_vector(DIRECTION_EAST);
      const V2S west  = unit_vector(DIRECTION_WEST);

      // check for adjacent bang
      Bool bang = false;
      for (Direction d = 0; d < DIRECTION_CARDINAL; d++) {
        const Value v = model_get(m, v2s_add(origin, unit_vector(d)));
        bang = bang || v.tag == VALUE_BANG;
      }

      if (value.powered || bang) {

        switch (value.tag) {

          case VALUE_ADD:
          case VALUE_SUB:
          case VALUE_MUL:
          case VALUE_DIV:
          case VALUE_EQUAL:
          case VALUE_GREATER:
          case VALUE_LESSER:
          case VALUE_AND:
          case VALUE_OR:
          case VALUE_BOTTOM:
          case VALUE_CLOCK:
          case VALUE_DELAY:
          case VALUE_STORE:
          case VALUE_ODDMENT:
          case VALUE_RANDOM:
          case VALUE_TOP:
            {
              graph_set(graph, v2s_add(origin, v2s_scale(west, 1)), true);
              graph_set(graph, v2s_add(origin, v2s_scale(east, 1)), true);
            } break;
          case VALUE_ALTER:
            {
              graph_set(graph, v2s_add(origin, v2s_scale(west, 1)), true);
              graph_set(graph, v2s_add(origin, v2s_scale(east, 1)), true);
              graph_set(graph, v2s_add(origin, v2s_scale(east, 2)), true);
            } break;
          case VALUE_HOP:
          case VALUE_LOAD:
          case VALUE_NOTE:
          case VALUE_QUOTE:
            {
              graph_set(graph, v2s_add(origin, v2s_scale(west, 1)), true);
            } break;
          case VALUE_INTERFERE:
            {
              graph_set(graph, v2s_add(origin, v2s_scale(west, 1)), true);
              graph_set(graph, v2s_add(origin, v2s_scale(west, 2)), true);
              graph_set(graph, v2s_add(origin, v2s_scale(east, 1)), true);
            } break;
          case VALUE_JUMP:
            {
              graph_set(graph, v2s_add(origin, v2s_scale(north, 1)), true);
            } break;
          case VALUE_MULTIPLEX:
            {
              const S32 dx = read_literal(model_get(m, v2s_add(origin, v2s_scale(east, 1))), 0);
              const S32 dy = read_literal(model_get(m, v2s_add(origin, v2s_scale(east, 2))), 0);
              const V2S source = v2s_sub(origin, v2s(dx + 1, dy));
              graph_set(graph, source, true);
            } break;
        }
      }
    }
  }
}

#define RND_IMPLEMENTATION
#include "rnd.h"
