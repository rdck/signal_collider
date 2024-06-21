/*******************************************************************************
 * message.h - message passing between audio and render thread
 ******************************************************************************/

#pragma once

#include "model.h"

// @rdk: support dynamic capacity for queues
#define MESSAGE_QUEUE_CAPACITY 0x100

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
  Index length;
  Message buffer[MESSAGE_QUEUE_CAPACITY];
} MessageQueue;

// message builders
Message message_write(V2S point, Value value);
Message message_alloc(Index index);

// queue mutation
Void message_enqueue(MessageQueue* queue, Message message);
Void message_dequeue(MessageQueue* queue, Message* out);

// queue queries
Index message_queue_length(const MessageQueue* queue);
