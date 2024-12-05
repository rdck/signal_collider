/*******************************************************************************
 * message.h - message passing between audio and render thread
 ******************************************************************************/

#pragma once

#include "model.h"

#define MESSAGE_QUEUE_CAPACITY 0x100

typedef enum MessageTag {
  MESSAGE_NONE,
  MESSAGE_WRITE,
  MESSAGE_ALLOCATE,
  MESSAGE_POINTER,
  MESSAGE_REVERB,
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
    Void* pointer;
    Bool flag;
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
Message message_alloc(Index index);
Message message_pointer(Void* pointer);
Message message_reverb(Bool status);

// no effect when queue is full
Void message_enqueue(MessageQueue* queue, Message message);

// no effect when queue is empty
Void message_dequeue(MessageQueue* queue, Message* out);

// query length
Index message_queue_length(const MessageQueue* queue);
