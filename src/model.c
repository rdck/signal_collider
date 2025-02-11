#include <string.h>
#include <SDL3/SDL_log.h>
#include "model.h"

_Static_assert(
    sizeof(MODEL_SIGNATURE) == MODEL_SIGNATURE_BYTES + 1,
    "invalid signature size"
    );

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

// map zero to default value
static inline S32 map_zero(S32 value, S32 revert)
{
  return value == 0 ? revert : value;
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
  const V2S target = v2s_sub(origin, offset);
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

Bool valid_point(V2S d, V2S c)
{
  const Bool x = c.x >= 0 && c.x < d.x;
  const Bool y = c.y >= 0 && c.y < d.y;
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
  memset(m->memory, 0, m->dimensions.x * m->dimensions.y * sizeof(Value));
  memset(m->register_file, 0, sizeof(RegisterFile));
  rnd_pcg_seed(&m->register_file->rnd, 0u);
}

Value model_get(const Model* m, V2S point)
{
  if (valid_point(m->dimensions, point)) {
    return MODEL_INDEX(m, point.x, point.y);
  } else {
    return value_none;
  }
}

Void model_set(Model* m, V2S point, Value value)
{
  if (valid_point(m->dimensions, point)) {
    MODEL_INDEX(m, point.x, point.y) = value;
  }
}

S32 read_literal(Value v, S32 none)
{
  return v.tag == VALUE_LITERAL ? v.literal : none;
}

Void model_step(Model* m, Graph* g)
{
  // clear graph
  // @rdk: remember to change this when making graph size dynamic
  memset(g, 0, sizeof(*g));

  // shorthand
  RegisterFile* const rf = m->register_file;

  // clear bangs and pulses
  for (Index y = 0; y < MODEL_Y; y++) {
    for (Index x = 0; x < MODEL_X; x++) {
      MODEL_INDEX(m, x, y).pulse = false;
      if (MODEL_INDEX(m, x, y).tag == VALUE_BANG) {
        MODEL_INDEX(m, x, y) = value_none;
      }
    }
  }

  // iterate in English reading order
  for (Index y = 0; y < MODEL_Y; y++) {
    for (Index x = 0; x < MODEL_X; x++) {

      const V2S origin = { (S32) x, (S32) y };
      const Value value = MODEL_INDEX(m, x, y);

      // cache adjacent coordinates and values
      Bool bang = false;
      for (Direction d = 0; d < DIRECTION_CARDINAL; d++) {
        const Value adjacent = model_get(m, add_unit_vector(origin, d));
        bang = bang || adjacent.tag == VALUE_BANG;
      }

      // mark pulse
      if (value.powered == false && bang) {
        MODEL_INDEX(m, x, y).pulse = true;
      }
      
      if (value.powered || bang) {

        switch (value.tag) {

          case VALUE_ADD:
            {
              const Value augend = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_LEFT_ADDEND);
              const Value addend = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_RIGHT_ADDEND);
              const S32 output = (read_literal(augend, 0) + read_literal(addend, 0)) % MODEL_RADIX;
              record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, value_literal(output));
            } break;

          case VALUE_SUB:
            {
              const Value minuend    = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_MINUEND);
              const Value subtrahend = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_SUBTRAHEND);
              const S32 difference = read_literal(minuend, 0) - read_literal(subtrahend, 0);
              const S32 output = difference < 0 ? difference + MODEL_RADIX : difference;
              record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, value_literal(output));
            } break;

          case VALUE_MUL:
            {
              const Value multiplier   = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_MULTIPLIER);
              const Value multiplicand = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_MULTIPLICAND);
              const S32 output = (read_literal(multiplier, 0) * read_literal(multiplicand, 0)) % MODEL_RADIX;
              record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, value_literal(output));
            } break;

          case VALUE_DIV:
            {
              const Value dividend = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_DIVIDEND);
              const Value divisor  = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_DIVISOR);
              const S32 divisor_literal = read_literal(divisor, 0);
              if (divisor_literal != 0) {
                const S32 quotient = read_literal(dividend, 0) / divisor_literal;
                record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, value_literal(quotient));
              } else {
                record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, value_none);
              }
            } break;

          case VALUE_EQUAL:
            {
              // How should equality (and inequality) behave when comparing operators?
              const Value lhs = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_LEFT_COMPARATE);
              const Value rhs = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_RIGHT_COMPARATE);
              if (lhs.tag == VALUE_LITERAL && rhs.tag == VALUE_LITERAL) {
                if (lhs.literal == rhs.literal) {
                  record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, value_bang);
                }
              }
            } break;

          case VALUE_GREATER:
            {
              const Value lhs = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_LEFT_COMPARATE);
              const Value rhs = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_RIGHT_COMPARATE);
              if (lhs.tag == VALUE_LITERAL && rhs.tag == VALUE_LITERAL) {
                if (lhs.literal > rhs.literal) {
                  record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, value_bang);
                }
              }
            } break;

          case VALUE_LESSER:
            {
              const Value lhs = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_LEFT_COMPARATE);
              const Value rhs = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_RIGHT_COMPARATE);
              if (lhs.tag == VALUE_LITERAL && rhs.tag == VALUE_LITERAL) {
                if (lhs.literal < rhs.literal) {
                  record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, value_bang);
                }
              }
            } break;

          case VALUE_AND:
            {
              const Value lhs = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_LEFT_CONJUNCT);
              const Value rhs = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_RIGHT_CONJUNCT);
              if (lhs.tag == VALUE_LITERAL && rhs.tag == VALUE_LITERAL) {
                const Value output = value_literal(lhs.literal & rhs.literal);
                record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, output);
              } else if (lhs.tag != VALUE_NONE && rhs.tag != VALUE_NONE) {
                record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, value_bang);
              }
            } break;

          case VALUE_OR:
            {
              const Value lhs = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_LEFT_DISJUNCT);
              const Value rhs = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_RIGHT_DISJUNCT);
              if (lhs.tag == VALUE_LITERAL && rhs.tag == VALUE_LITERAL) {
                const Value output = value_literal(lhs.literal | rhs.literal);
                record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, output);
              } else if (lhs.tag != VALUE_NONE || rhs.tag != VALUE_NONE) {
                record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, value_bang);
              }
            } break;

          case VALUE_ALTER:
            {
              const Value t   = record_read(m, g, origin, v2s(3, 0), value.tag, ATTRIBUTE_TIME);
              const Value lhs = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_MINIMUM);
              const Value rhs = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_MAXIMUM);
              const S32 lhsv = read_literal(lhs, 0);
              const S32 rhsv = read_literal(rhs, 0);
              const S32 tv   = read_literal(t, 0);
              const S32 scale = MODEL_RADIX - 1;
              const S32 output = ((scale - tv) * lhsv + tv * rhsv) / scale;
              record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, value_literal(output));
            } break;

          case VALUE_BOTTOM:
            {
              const Value input_lhs = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_LEFT_COMPARATE);
              const Value input_rhs = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_RIGHT_COMPARATE);
              const S32 lhs = read_literal(input_lhs, MODEL_RADIX - 1);
              const S32 rhs = read_literal(input_rhs, MODEL_RADIX - 1);
              const Value output = value_literal(MIN(lhs, rhs));
              record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, output);
            } break;

          case VALUE_CLOCK:
            {
              const Value input_rate = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_RATE);
              const Value input_mod  = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_DIVISOR);
              const S32 rate = read_literal(input_rate, 0) + 1;
              if (rf->frame % rate == 0) {
                const S32 mod = map_zero(read_literal(input_mod, 0), MODEL_RADIX);
                const Value output = value_literal((rf->frame / rate) % mod);
                record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, output);
              }
            } break;

          case VALUE_DELAY:
            {
              const Value input_rate = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_RATE);
              const Value input_mod  = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_DIVISOR);
              const S32 rate = read_literal(input_rate, 0) + 1;
              const S32 mod = map_zero(read_literal(input_mod, 1), MODEL_RADIX);
              const S32 output = (rf->frame / rate) % mod;
              if (output == 0) {
                record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, value_bang);
              }
            } break;

          case VALUE_HOP:
            {
              // Whether we should apply the hop to nil values is unclear to me.
              const Value input = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_INPUT);
              record_write(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_OUTPUT, input);
            } break;

          case VALUE_INTERFERE:
            {
              // Again, what to do in the nil input case is unclear to me.
              const Value iv = record_read(m, g, origin, v2s(3, 0), value.tag, ATTRIBUTE_INPUT);
              const Value xv = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_X);
              const Value yv = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_Y);
              const V2S delta = { read_literal(xv, 0), read_literal(yv, 0) + 1 };
              record_write(m, g, origin, delta, value.tag, ATTRIBUTE_OUTPUT, iv);
            } break;

          case VALUE_JUMP:
            {
              const Value input = record_read(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_INPUT);
              record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, input);
            } break;

          case VALUE_LOAD:
            {
              const Value reg = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_REGISTER);
              if (reg.tag == VALUE_LITERAL) {
                const Value v = rf->registers[reg.literal];
                record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, v);
              } else {
                record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, value_none);
              }
            } break;

          case VALUE_MULTIPLEX:
            {
              const Value xv = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_X);
              const Value yv = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_Y);
              const V2S delta = { - (read_literal(xv, 0) + 1), read_literal(yv, 0) };
              const Value iv = record_read(m, g, origin, delta, value.tag, ATTRIBUTE_INPUT);
              record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, iv);
            } break;

          case VALUE_NOTE:
            {
              const Value input_index = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_INDEX);
              const S32 index = read_literal(input_index, 0);
              const S32 octave  = index / SCALE_CARDINAL;
              const S32 note    = index % SCALE_CARDINAL;
              const S32 pitch   = (OCTAVE * octave + scale_table[note]) % MODEL_RADIX;
              record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, value_literal(pitch));
            } break;

          case VALUE_ODDMENT:
            {
              const Value input_dividend = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_DIVIDEND);
              const Value input_divisor  = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_DIVISOR);
              const S32 dividend = read_literal(input_dividend, 0);
              const S32 divisor = map_zero(read_literal(input_divisor, 0), MODEL_RADIX);
              const Value residue = value_literal(dividend % divisor);
              record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, residue);
            } break;

          case VALUE_QUOTE:
            {
              const Value index = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_INDEX);
              if (index.tag == VALUE_LITERAL) {
                const Value output = {
                  .tag = VALUE_BANG + index.literal,
                  .powered = true,
                };
                if (quotation_table[output.tag]) {
                  record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, output);
                } else {
                record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, value_none);
                }
              } else {
                record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, value_none);
              }
            } break;

          case VALUE_RANDOM:
            {
              const Value input_rate = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_RATE);
              const Value input_mod = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_DIVISOR);
              const S32 rate = read_literal(input_rate, 0) + 1;
              if (rf->frame % rate == 0) {
                const S32 mod = map_zero(read_literal(input_mod, 0), MODEL_RADIX);
                const Value output = value_literal(rnd_pcg_next(&rf->rnd) % mod);
                record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, output);
              }
            } break;

          case VALUE_STORE:
            {
              const Value set = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_INPUT);
              const Value reg = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_REGISTER);
              if (reg.tag == VALUE_LITERAL) {
                rf->registers[reg.literal] = set;
              }
            } break;

          case VALUE_TOP:
            {
              const Value input_lhs = record_read(m, g, origin, v2s(2, 0), value.tag, ATTRIBUTE_LEFT_COMPARATE);
              const Value input_rhs = record_read(m, g, origin, v2s(1, 0), value.tag, ATTRIBUTE_RIGHT_COMPARATE);
              const S32 lhs = read_literal(input_lhs, 0);
              const S32 rhs = read_literal(input_rhs, 0);
              const Value output = value_literal(MAX(lhs, rhs));
              record_write(m, g, origin, v2s(0, 1), value.tag, ATTRIBUTE_OUTPUT, output);
            } break;

          case VALUE_SYNTH:
            {
              // These coordinates have to be kept in sync with the logic in
              // the simulation module.
              record_read(m, g, origin, v2s(6, 0), value.tag, "OCTAVE");
              record_read(m, g, origin, v2s(5, 0), value.tag, "PITCH");
              record_read(m, g, origin, v2s(4, 0), value.tag, "VOLUME");
              record_read(m, g, origin, v2s(3, 0), value.tag, "ATTACK");
              record_read(m, g, origin, v2s(2, 0), value.tag, "HOLD");
              record_read(m, g, origin, v2s(1, 0), value.tag, "RELEASE");
            } break;

          case VALUE_SAMPLER:
            {
              record_read(m, g, origin, v2s(7, 0), value.tag, "SOUND INDEX");
              record_read(m, g, origin, v2s(6, 0), value.tag, "START TIME");
              record_read(m, g, origin, v2s(5, 0), value.tag, "VOLUME");
              record_read(m, g, origin, v2s(4, 0), value.tag, "ATTACK");
              record_read(m, g, origin, v2s(3, 0), value.tag, "HOLD");
              record_read(m, g, origin, v2s(2, 0), value.tag, "RELEASE");
              record_read(m, g, origin, v2s(1, 0), value.tag, "PITCH");
            } break;

          default: { }
        }
      }
    }
  }

  rf->frame += 1;
}

#define RND_IMPLEMENTATION
#include "rnd.h"
