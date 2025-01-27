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

#include "prelude.h"
#include "rnd.h"

// default grid size
#define MODEL_X 0x40
#define MODEL_Y 0x24
#define GRAPH_EDGES (MODEL_X * MODEL_Y * 4)

// the base of the numeral system
#define MODEL_RADIX 36

// byte length of the file signature
#define MODEL_SIGNATURE "brstmata"
#define MODEL_SIGNATURE_BYTES 8
#define MODEL_VERSION 1

#define ATTRIBUTE_LEFT_ADDEND "LEFT ADDEND"
#define ATTRIBUTE_RIGHT_ADDEND "RIGHT ADDEND"
#define ATTRIBUTE_MINUEND "MINUEND"
#define ATTRIBUTE_SUBTRAHEND "SUBTRAHEND"
#define ATTRIBUTE_MULTIPLIER "MULTIPLIER"
#define ATTRIBUTE_MULTIPLICAND "MULTIPLICAND"
#define ATTRIBUTE_DIVIDEND "DIVIDEND"
#define ATTRIBUTE_DIVISOR "DIVISOR"
#define ATTRIBUTE_LEFT_COMPARATE "LEFT COMPARATE"
#define ATTRIBUTE_RIGHT_COMPARATE "RIGHT COMPARATE"
#define ATTRIBUTE_LEFT_CONJUNCT "LEFT CONJUNCT"
#define ATTRIBUTE_RIGHT_CONJUNCT "RIGHT CONJUNCT"
#define ATTRIBUTE_LEFT_DISJUNCT "LEFT DISJUNCT"
#define ATTRIBUTE_RIGHT_DISJUNCT "RIGHT DISJUNCT"
#define ATTRIBUTE_MINIMUM "MINIMUM"
#define ATTRIBUTE_MAXIMUM "MAXIMUM"
#define ATTRIBUTE_RATE "RATE"
#define ATTRIBUTE_TIME "TIME"
#define ATTRIBUTE_X "X"
#define ATTRIBUTE_Y "Y"
#define ATTRIBUTE_INPUT "INPUT"
#define ATTRIBUTE_REGISTER "REGISTER"
#define ATTRIBUTE_INDEX "INDEX"
#define ATTRIBUTE_OUTPUT "OUTPUT"

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
  VALUE_DIV,
  VALUE_EQUAL,
  VALUE_GREATER,
  VALUE_LESSER,
  VALUE_AND,
  VALUE_OR,
  VALUE_ALTER,
  VALUE_BOTTOM,
  VALUE_CLOCK,
  VALUE_DELAY,
  VALUE_E,
  VALUE_F,
  VALUE_G,
  VALUE_HOP,
  VALUE_INTERFERE,
  VALUE_JUMP,
  VALUE_K,
  VALUE_LOAD,
  VALUE_MULTIPLEX,
  VALUE_NOTE,
  VALUE_ODDMENT,
  VALUE_P,
  VALUE_QUOTE,
  VALUE_RANDOM,
  VALUE_STORE,
  VALUE_TOP,
  VALUE_U,
  VALUE_V,
  VALUE_W,
  VALUE_SAMPLER,
  VALUE_SYNTH,
  VALUE_MIDI,
  VALUE_CARDINAL,
} ValueTag;

// literal values
typedef struct Value {
  ValueTag tag;
  Bool powered;
  Bool pulse; // flag for renderer
  S32 literal;
} Value;

// program state
typedef struct Model {
  Index frame;                            // beat counter
  rnd_pcg_t rnd;                          // random number generator
  Value registers[MODEL_RADIX];           // register set
  Value map[MODEL_Y][MODEL_X];            // program memory
} Model;

// state stored on disk
typedef struct ModelStorage {
  Byte signature[MODEL_SIGNATURE_BYTES];  // file signature
  S32 version;                            // file format version
  Value registers[MODEL_RADIX];           // register set
  Value map[MODEL_Y][MODEL_X];            // program memory
} ModelStorage;

typedef enum GraphEdgeTag {
  GRAPH_EDGE_NONE,
  GRAPH_EDGE_INPUT,
  GRAPH_EDGE_OUTPUT,
  GRAPH_EDGE_CARDINAL,
} GraphEdgeTag;

typedef struct GraphEdge {
  GraphEdgeTag tag;
  V2S origin;
  V2S target;
  ValueTag cause;
  const Char* attribute;
} GraphEdge;

typedef struct Graph {
  Index head;
  GraphEdge edges[GRAPH_EDGES];
} Graph;

typedef struct ModelGraph {
  Model model;
  Graph graph;
} ModelGraph;

// constant values
extern const Value value_none;
extern const Value value_bang;
extern const Value value_add;
extern const Value value_sub;
extern const Value value_mul;
extern const Value value_div;
extern const Value value_equal;
extern const Value value_greater;
extern const Value value_lesser;
extern const Value value_and;
extern const Value value_or;
extern const Value value_alter;
extern const Value value_bottom;
extern const Value value_clock;
extern const Value value_delay;
extern const Value value_hop;
extern const Value value_interfere;
extern const Value value_jump;
extern const Value value_load;
extern const Value value_multiplex;
extern const Value value_note;
extern const Value value_oddment;
extern const Value value_quote;
extern const Value value_random;
extern const Value value_store;
extern const Value value_top;
extern const Value value_sampler;
extern const Value value_synth;

// operator predicate
Bool is_operator(Value value);

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
Void model_step(Model* m, Graph* graph);
