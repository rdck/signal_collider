/*******************************************************************************
 * message.h - message passing between audio and render thread
 ******************************************************************************/

#pragma once

#include "model.h"
#include "palette.h"

#define MESSAGE_QUEUE_CAPACITY 0x100

typedef enum MessageTag {

  // null value
  MESSAGE_NONE,

  // input queue
  MESSAGE_WRITE,
  MESSAGE_CLEAR,

  // allocation queues
  MESSAGE_ALLOCATE,

  // load queue
  MESSAGE_LOAD,

  // control queue
  MESSAGE_TEMPO,
  MESSAGE_PALETTE,
  MESSAGE_GLOBAL_VOLUME,
  MESSAGE_ENVELOPE_COEFFICIENT,
  MESSAGE_ENVELOPE_EXPONENT,
  MESSAGE_REVERB_STATUS,
  MESSAGE_REVERB_SIZE,
  MESSAGE_REVERB_CUTOFF,
  MESSAGE_REVERB_MIX,

  // sentinel
  MESSAGE_CARDINAL,

} MessageTag;

typedef struct Write {
  V2S point;
  Value value;
} Write;

typedef struct Allocate {
  Index index;
} Allocate;

// We don't really need this to be a tagged union. The queue should be
// specialized to each type, eventually. This would also mean we don't need a
// generic void pointer message.
typedef struct Message {
  MessageTag tag;
  union {
    Write write;
    Allocate alloc;
    S32 tempo;
    Palette* palette;
    ModelStorage* storage;
    Bool flag;
    F32 parameter;
  };
} Message;

typedef struct MessageQueue {
  Index producer;
  Index consumer;
  _Atomic Index length;
  Message buffer[MESSAGE_QUEUE_CAPACITY];
} MessageQueue;

// message builders
Message message_write(V2S point, Value value);
Message message_clear();
Message message_alloc(Index index);
Message message_load(ModelStorage* storage);
Message message_tempo(S32 tempo);
Message message_palette(Palette* pointer);
Message message_global_volume(F32 volume);
Message message_envelope_coefficient(F32 coefficient);
Message message_envelope_exponent(F32 exponent);
Message message_reverb_status(Bool status);
Message message_reverb_size(F32 size);
Message message_reverb_cutoff(F32 cutoff);
Message message_reverb_mix(F32 mix);

// no effect when queue is full
Void message_enqueue(MessageQueue* queue, Message message);

// no effect when queue is empty
Void message_dequeue(MessageQueue* queue, Message* out);

// query length
Index message_queue_length(const MessageQueue* queue);
