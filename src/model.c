#include <string.h>
#include <SDL3/SDL_log.h>
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

static GraphEdge graph_edge(GraphEdgeTag tag, V2S origin, V2S target, ValueTag cause, const Char* attribute)
{
  GraphEdge edge;
  edge.tag = tag;
  edge.origin = origin;
  edge.target = target;
  edge.cause = cause;
  edge.attribute = attribute;
  return edge;
}

static Void record_graph_edge(Graph* graph, GraphEdge edge)
{
  if (graph->head < GRAPH_EDGES) {
    graph->edges[graph->head] = edge;
    graph->head += 1;
  } else {
    SDL_Log("at graph capacity");
  }
}

static Value record_read(const Model* m, Graph* g, V2S origin, V2S offset, ValueTag cause, const Char* attribute)
{
  const V2S target = v2s_add(origin, offset);
  const Value input = model_get(m, target);
  const GraphEdge edge = graph_edge(
      GRAPH_EDGE_INPUT,
      origin,
      target,
      cause,
      attribute);
  record_graph_edge(g, edge);
  return input;
}

static Void record_write(Model* m, Graph* g, V2S origin, V2S offset, ValueTag cause, const Char* attribute, Value v)
{
  const V2S target = v2s_add(origin, offset);
  model_set(m, target, v);
  const GraphEdge edge = graph_edge(
      GRAPH_EDGE_OUTPUT,
      origin,
      target,
      cause,
      attribute);
  record_graph_edge(g, edge);
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
  if (valid_point(point)) {
    return m->map[point.y][point.x];
  } else {
    return value_none;
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

Void model_step(Model* m, Graph* g)
{
  // clear graph
  memset(g, 0, sizeof(*g));

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
              const Value augend = record_read(m, g, origin, v2s(-1, 0), value.tag, "ADDEND");
              const Value addend = record_read(m, g, origin, v2s( 1, 0), value.tag, "ADDEND");
              const S32 output = (read_literal(augend, 0) + read_literal(addend, 0)) % MODEL_RADIX;
              record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_literal(output));
            } break;

          case VALUE_SUB:
            {
              const Value minuend    = record_read(m, g, origin, v2s(-1, 0), value.tag, "MINUEND");
              const Value subtrahend = record_read(m, g, origin, v2s( 1, 0), value.tag, "SUBTRAHEND");
              const S32 difference = read_literal(minuend, 0) - read_literal(subtrahend, 0);
              const S32 output = difference < 0 ? difference + MODEL_RADIX : difference;
              record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_literal(output));
            } break;

          case VALUE_MUL:
            {
              const Value multiplier   = record_read(m, g, origin, v2s(-1, 0), value.tag, "MULTIPLIER");
              const Value multiplicand = record_read(m, g, origin, v2s( 1, 0), value.tag, "MULTIPLICAND");
              const S32 output = (read_literal(multiplier, 0) * read_literal(multiplicand, 0)) % MODEL_RADIX;
              record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_literal(output));
            } break;

          case VALUE_DIV:
            {
              const Value dividend = record_read(m, g, origin, v2s(-1, 0), value.tag, "DIVIDEND");
              const Value divisor  = record_read(m, g, origin, v2s( 1, 0), value.tag, "DIVISOR");
              const S32 divisor_literal = read_literal(divisor, 0);
              if (divisor_literal != 0) {
                const S32 quotient = read_literal(dividend, 0) / divisor_literal;
                record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_literal(quotient));
              }
            } break;

          case VALUE_EQUAL:
            {
              // How should equality (and inequality) behave when comparing operators?
              const Value lhs = record_read(m, g, origin, v2s(-1, 0), value.tag, "LEFT HAND SIDE");
              const Value rhs = record_read(m, g, origin, v2s( 1, 0), value.tag, "RIGHT HAND SIDE");
              if (lhs.tag == VALUE_LITERAL && rhs.tag == VALUE_LITERAL) {
                if (ve.literal == vw.literal) {
                  record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_bang);
                }
              }
            } break;

          case VALUE_GREATER:
            {
              const Value lhs = record_read(m, g, origin, v2s(-1, 0), value.tag, "LEFT HAND SIDE");
              const Value rhs = record_read(m, g, origin, v2s( 1, 0), value.tag, "RIGHT HAND SIDE");
              if (lhs.tag == VALUE_LITERAL && rhs.tag == VALUE_LITERAL) {
                if (lhs.literal > rhs.literal) {
                  record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_bang);
                }
              }
            } break;

          case VALUE_LESSER:
            {
              const Value lhs = record_read(m, g, origin, v2s(-1, 0), value.tag, "LEFT HAND SIDE");
              const Value rhs = record_read(m, g, origin, v2s( 1, 0), value.tag, "RIGHT HAND SIDE");
              if (lhs.tag == VALUE_LITERAL && rhs.tag == VALUE_LITERAL) {
                if (lhs.literal < rhs.literal) {
                  record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_bang);
                }
              }
            } break;

          case VALUE_AND:
            {
              const Value lhs = record_read(m, g, origin, v2s(-1, 0), value.tag, "LEFT CONJUNCT");
              const Value rhs = record_read(m, g, origin, v2s( 1, 0), value.tag, "RIGHT CONJUNCT");
              if (lhs.tag == VALUE_LITERAL && rhs.tag == VALUE_LITERAL) {
                record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_literal(lhs.literal & rhs.literal));
              } else if (ve.tag != VALUE_NONE && vw.tag != VALUE_NONE) {
                record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_bang);
              }
            } break;

          case VALUE_OR:
            {
              const Value lhs = record_read(m, g, origin, v2s(-1, 0), value.tag, "LEFT DISJUNCT");
              const Value rhs = record_read(m, g, origin, v2s( 1, 0), value.tag, "RIGHT DISJUNCT");
              if (lhs.tag == VALUE_LITERAL && rhs.tag == VALUE_LITERAL) {
                record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_literal(lhs.literal | rhs.literal));
              } else if (ve.tag != VALUE_NONE || vw.tag != VALUE_NONE) {
                record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_bang);
              }
            } break;

          case VALUE_ALTER:
            {
              const Value lhs = record_read(m, g, origin, v2s( 1, 0), value.tag, "MINIMUM");
              const Value rhs = record_read(m, g, origin, v2s( 2, 0), value.tag, "MAXIMUM");
              const Value t   = record_read(m, g, origin, v2s(-1, 0), value.tag, "TIME");
              const S32 lhsv = read_literal(lhs, 0);
              const S32 rhsv = read_literal(rhs, 0);
              const S32 tv   = read_literal(t, 0);
              const S32 scale = MODEL_RADIX - 1;
              const S32 output = ((scale - tv) * lhsv + tv * rhsv) / scale;
              record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_literal(output));
            } break;

          case VALUE_BOTTOM:
            {
              const Value lhs = record_read(m, g, origin, v2s(-1, 0), value.tag, "LEFT");
              const Value rhs = record_read(m, g, origin, v2s( 1, 0), value.tag, "RIGHT");
              if (lhs.tag == VALUE_LITERAL && rhs.tag == VALUE_LITERAL) {
                const Value output = value_literal(MIN(lhs.literal, rhs.literal));
                record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", output);
              }
            } break;

          case VALUE_CLOCK:
            {
              const Value rate_value = record_read(m, g, origin, v2s(-1, 0), value.tag, "RATE");
              const Value mod_value  = record_read(m, g, origin, v2s( 1, 0), value.tag, "MODULUS");
              const S32 rate = read_literal(rate_value, 0) + 1;
              if (m->frame % rate == 0) {
                const S32 mod = map_zero(mod_value, MODEL_RADIX);
                const Value output = value_literal((m->frame / rate) % mod);
                record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", output);
              }
            } break;

          case VALUE_DELAY:
            {
              const Value rate_value = record_read(m, g, origin, v2s(-1, 0), value.tag, "RATE");
              const Value mod_value  = record_read(m, g, origin, v2s( 1, 0), value.tag, "MODULUS");
              const S32 rate = read_literal(rate_value, 0) + 1;
              const S32 mod = map_zero(mod_value, MODEL_RADIX);
              const S32 output = (m->frame / rate) % mod;
              if (output == 0) {
                record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_bang);
              }
            } break;

          case VALUE_HOP:
            {
              // Whether we should apply the hop to nil values is unclear to me.
              const Value input = record_read(m, g, origin, v2s(-1, 0), value.tag, "INPUT");
              record_write(m, g, origin, v2s(1, 0), value.tag, "OUTPUT", input);
            } break;

          case VALUE_INTERFERE:
            {
              // Again, what to do in the nil input case is unclear to me.
              const Value xv = record_read(m, g, origin, v2s(-2, 0), value.tag, "X COORDINATE");
              const Value yv = record_read(m, g, origin, v2s(-1, 0), value.tag, "Y COORDINATE");
              const Value iv = record_read(m, g, origin, v2s( 1, 0), value.tag, "VALUE");
              const V2S delta = { read_literal(xv, 0), read_literal(yv, 0) + 1 };
              record_write(m, g, origin, delta, value.tag, "OUTPUT", iv);
            } break;

          case VALUE_JUMP:
            {
              const Value input = record_read(m, g, origin, v2s(0, -1), value.tag, "INPUT");
              record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", input);
            } break;

          case VALUE_LOAD:
            {
              const Value reg = record_read(m, g, origin, v2s(-1, 0), value.tag, "REGISTER");
              if (reg.tag == VALUE_LITERAL) {
                const Value v = m->registers[reg.literal];
                record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", v);
              }
            } break;

          case VALUE_MULTIPLEX:
            {
              const Value xv = record_read(m, g, origin, v2s(1, 0), value.tag, "X COORDINATE");
              const Value yv = record_read(m, g, origin, v2s(2, 0), value.tag, "Y COORDINATE");
              const V2S delta = { - (read_literal(xv, 0) + 1), - read_literal(yv, 0) };
              const Value iv = record_read(m, g, origin, delta, value.tag, "VALUE");
              record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", iv);
            } break;

          case VALUE_NOTE:
            {
              const Value index = record_read(m, g, origin, v2s(-1, 0), value.tag, "NOTE INDEX");
              if (index.tag == VALUE_LITERAL) {
                const S32 octave  = index.literal / SCALE_CARDINAL;
                const S32 note    = index.literal % SCALE_CARDINAL;
                const S32 pitch   = (OCTAVE * octave + scale_table[note]) % MODEL_RADIX;
                record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_literal(pitch));
              } else {
                record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_none);
              }
            } break;

          case VALUE_ODDMENT:
            {
              const Value dividend = record_read(m, g, origin, v2s(-1, 0), value.tag, "DIVIDEND");
              const Value divisor  = record_read(m, g, origin, v2s( 1, 0), value.tag, "DIVISOR");
              if (dividend.tag == VALUE_LITERAL && divisor.tag == VALUE_LITERAL) {
                const S32 d = divisor.literal == 0 ? MODEL_RADIX : divisor.literal;
                const Value residue = value_literal(dividend.literal % d);
                record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", residue);
              } else {
                record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_none);
              }
            } break;

          case VALUE_QUOTE:
            {
              const Value index = record_read(m, g, origin, v2s(-1, 0), value.tag, "INDEX");
              if (index.tag == VALUE_LITERAL) {
                const Value output = {
                  .tag = VALUE_BANG + index.literal,
                  .powered = true,
                };
                if (quotation_table[output.tag]) {
                  record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", output);
                } else {
                record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_none);
                }
              } else {
                record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_none);
              }
            } break;

          case VALUE_RANDOM:
            {
              const Value rate = record_read(m, g, origin, v2s(-1, 0), value.tag, "RATE");
              const Value mod  = record_read(m, g, origin, v2s( 1, 0), value.tag, "MODULUS");
              if (rate.tag == VALUE_LITERAL && mod.tag == VALUE_LITERAL) {
                const S32 r = rate.literal == 0 ? MODEL_RADIX : rate.literal;
                if (m->frame % r == 0) {
                  const S32 d = mod.literal == 0 ? MODEL_RADIX : mod.literal;
                  const Value output = value_literal(rnd_pcg_next(&m->rnd) % d);
                  record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", output);
                }
              } else {
                record_write(m, g, origin, v2s(0, 1), value.tag, "OUTPUT", value_none);
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

#define RND_IMPLEMENTATION
#include "rnd.h"
