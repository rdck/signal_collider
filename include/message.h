/*******************************************************************************
 * message.h - message passing between audio and render thread
 ******************************************************************************/

#pragma once

#include "model.h"

#define MESSAGE_QUEUE_CAPACITY 0x400

typedef enum MessageTag {
  MESSAGE_NONE,
  MESSAGE_WRITE,
  MESSAGE_ALLOCATE,
  MESSAGE_CARDINAL,
} MessageTag;

typedef struct Write {
  V2S point;
  Value value;
} Write;

typedef struct Allocate {
  Index index;
} Allocate;

typedef struct Message {
  MessageTag tag;
  union {
    Write write;
    Allocate alloc;
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

// no effect when queue is full
Void message_enqueue(MessageQueue* queue, Message message);

// no effect when queue is empty
Void message_dequeue(MessageQueue* queue, Message* out);

// query length
Index message_queue_length(const MessageQueue* queue);
