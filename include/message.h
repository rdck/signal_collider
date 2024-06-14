#pragma once
#include "model.h"

#define Message_QUEUE_CAPACITY 0x40

// @rdk: change prefix
typedef enum Message_MessageTag {
  Model_MESSAGE_NONE,
  Model_MESSAGE_SET,
  Model_MESSAGE_CARDINAL,
} Message_MessageTag;

typedef struct Message_Message {
  Message_MessageTag tag;
  V2S point;
  Model_Value value;
} Message_Message;

typedef struct Message_Queue {
  Index producer;
  Index consumer;
  Index length;
  Message_Message buffer[Message_QUEUE_CAPACITY];
} Message_Queue;

Void Message_enqueue(Message_Queue* queue, Message_Message message);
Void Message_dequeue(Message_Queue* queue, Message_Message* out);
Index Message_queue_length(const Message_Queue* queue);
