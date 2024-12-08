/*******************************************************************************
 * model.h - language interpreter
 *
 * This module constitutes a definition of the language syntax and semantics.
 * The `model_step` function is a "small-step" evaluator. It will be called
 * once per beat by the realtime driver.
 *
 * The syntax of a program is structured as a 2D array, rather than as a tree.
 * Evaluation proceeds in English reading-order.
 ******************************************************************************/

#pragma once

#include "linear_algebra.h"
#include "rnd.h"

// default grid size
#define MODEL_X 32
#define MODEL_Y 18

// the base of the numeral system
#define MODEL_RADIX 36

// byte length of the file signature
#define MODEL_SIGNATURE "brstmata"
#define MODEL_SIGNATURE_BYTES 8
#define MODEL_VERSION 1

// cardinal directions
#define DIRECTION_NONE (-1)
typedef enum Direction {
  DIRECTION_NORTH,
  DIRECTION_EAST,
  DIRECTION_SOUTH,
  DIRECTION_WEST,
  DIRECTION_CARDINAL,
} Direction;

// syntactic constructs
typedef enum ValueTag {
  VALUE_NONE,
  VALUE_LITERAL,
  VALUE_BANG,
  VALUE_ADD,
  VALUE_SUB,
  VALUE_MUL,
  VALUE_EQUAL,
  VALUE_CLOCK,
  VALUE_DELAY,
  VALUE_RANDOM,
  VALUE_GENERATE,
  VALUE_SCALE,
  VALUE_SYNTH,
  VALUE_SAMPLER,
  VALUE_CARDINAL,
} ValueTag;

// literal values
typedef struct Value {
  ValueTag tag;
  S32 literal;
} Value;

// program state
typedef struct Model {
  Index frame;                            // beat counter
  rnd_pcg_t rnd;                          // random number generator
  Value map[MODEL_Y][MODEL_X];            // value grid
} Model;

// state stored on disk
typedef struct ModelStorage {
  Byte signature[MODEL_SIGNATURE_BYTES];  // file signature
  S32 version;                            // file format version
  Value map[MODEL_Y][MODEL_X];            // value grid
} ModelStorage;

// constant values
extern const Value value_none;
extern const Value value_bang;
extern const Value value_add;
extern const Value value_sub;
extern const Value value_mul;
extern const Value value_equal;
extern const Value value_clock;
extern const Value value_delay;
extern const Value value_random;
extern const Value value_generate;
extern const Value value_scale;
extern const Value value_synth;
extern const Value value_sampler;

// build a literal value
Value value_literal(S32 literal);

// validate a coordinate
Bool valid_point(V2S c);

// construct a unit vector for a cardinal direction
V2S unit_vector(Direction d);

// addition for points and directions, in the natural way
V2S add_unit_vector(V2S point, Direction d);

// fold over optional value
S32 read_literal(Value value, S32 none);

// set a value
Void model_set(Model* m, V2S point, Value value);

// read a value
Value model_get(const Model* m, V2S point);

// evaluator
Void model_init(Model* m);
Void model_step(Model* m);
