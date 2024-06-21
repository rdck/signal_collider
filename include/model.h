/*******************************************************************************
 * model.h - big step semantics
 ******************************************************************************/

#pragma once

#include "linear_algebra.h"
#include "rnd.h"

#define MODEL_X 32
#define MODEL_Y 18
#define MODEL_RADIX 36
#define DIRECTION_NONE (-1)

typedef enum Direction {
  DIRECTION_NORTH,
  DIRECTION_EAST,
  DIRECTION_SOUTH,
  DIRECTION_WEST,
  DIRECTION_CARDINAL,
} Direction;

typedef enum ValueTag {
  VALUE_NONE,
  VALUE_BANG,
  VALUE_LITERAL,
  VALUE_IF,
  VALUE_CLOCK,
  VALUE_DELAY,
  VALUE_RANDOM,
  VALUE_ADD,
  VALUE_SUB,
  VALUE_MUL,
  VALUE_GENERATE,
  VALUE_SCALE,
  VALUE_SYNTH,
  VALUE_CARDINAL,
} ValueTag;

typedef struct Value {
  ValueTag tag;
  S32 literal;
} Value;

typedef struct Model {
  Index frame;
  V2S cursor;
  rnd_pcg_t rnd;
  Value map[MODEL_Y][MODEL_X];
} Model;

// values
extern const Value value_none;
extern const Value value_bang;
extern const Value value_if;
extern const Value value_clock;
extern const Value value_delay;
extern const Value value_add;
extern const Value value_sub;
extern const Value value_synth;

// value builders
Value value_literal(S32 literal);

// coordinate system
Bool valid_point(V2S c);
V2S unit_vector(Direction d);
V2S add_unit_vector(V2S point, Direction d);

// fold over optional value
S32 read_literal(Value value, S32 none);

Void model_set(Model* m, V2S point, Value value);
Value model_get(const Model* m, V2S point);

Void model_init(Model* m);
Void model_step(Model* m);
