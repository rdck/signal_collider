#pragma once
#include "linear_algebra.h"
#include "rnd.h"

#define Model_X 32
#define Model_Y 18
#define Model_BASE 36

typedef enum Model_Direction {
  Model_DIRECTION_NORTH,
  Model_DIRECTION_EAST,
  Model_DIRECTION_SOUTH,
  Model_DIRECTION_WEST,
  Model_DIRECTION_CARDINAL,
} Model_Direction;

typedef enum Model_ValueTag {
  Model_VALUE_NONE,
  Model_VALUE_BANG,
  Model_VALUE_LITERAL,
  Model_VALUE_IF,
  Model_VALUE_CLOCK,
  Model_VALUE_DELAY,
  Model_VALUE_RANDOM,
  Model_VALUE_ADD,
  Model_VALUE_SUB,
  Model_VALUE_MUL,
  Model_VALUE_GENERATE,
  Model_VALUE_SCALE,
  Model_VALUE_SYNTH,
  Model_VALUE_CARDINAL,
} Model_ValueTag;

typedef struct Model_Value {
  Model_ValueTag tag;
  S32 literal;
} Model_Value;

typedef struct Model_T {
  Index frame;
  V2S cursor;
  rnd_pcg_t rnd;
  Model_Value map[Model_Y][Model_X];
} Model_T;

// values
extern const Model_Value Model_value_none;
extern const Model_Value Model_value_bang;
extern const Model_Value Model_value_if;
extern const Model_Value Model_value_clock;
extern const Model_Value Model_value_delay;
extern const Model_Value Model_value_add;
extern const Model_Value Model_value_sub;
extern const Model_Value Model_value_synth;

Model_Value Model_value_literal(S32 literal);

// coordinate system
Bool Model_valid_coordinate(V2S c);
V2S Model_unit_vector(Model_Direction d);
V2S Model_translate_unit(V2S point, Model_Direction d);

Void Model_set(Model_T* m, V2S point, Model_Value value);
Model_Value Model_get(const Model_T* m, V2S point);

S32 Model_read_literal(Model_Value value, S32 none);

Void Model_init(Model_T* m);
Void Model_step(Model_T* m);
