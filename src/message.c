#include <intrin.h>
#include "message.h"

Message message_write(V2S point, Value value)
{
  Message out;
  out.tag = MESSAGE_WRITE;
  out.write.point = point;
  out.write.value = value;
  return out;
}

Message message_alloc(Index index)
{
  Message out;
  out.tag = MESSAGE_ALLOCATE;
  out.alloc.index = index;
  return out;
}

Void message_enqueue(MessageQueue* queue, Message message)
{
  const Index length = queue->length;
  if (length < MESSAGE_QUEUE_CAPACITY) {
    queue->buffer[queue->producer] = message;
    queue->producer = (queue->producer + 1) % MESSAGE_QUEUE_CAPACITY;
    _InterlockedIncrement64(&queue->length);
  }
}

Void message_dequeue(MessageQueue* queue, Message* out)
{
  const Index length = queue->length;
  if (length > 0) {
    *out = queue->buffer[queue->consumer];
    queue->consumer = (queue->consumer + 1) % MESSAGE_QUEUE_CAPACITY;
    _InterlockedDecrement64(&queue->length);
  }
}

Index message_queue_length(const MessageQueue* queue)
{
  return queue->length;
}
