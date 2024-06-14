#include <intrin.h>
#include "message.h"

Void Message_enqueue(Message_Queue* queue, Message_Message message)
{
  const Index length = queue->length;
  if (length < Message_QUEUE_CAPACITY) {
    queue->buffer[queue->producer] = message;
    queue->producer = (queue->producer + 1) % Message_QUEUE_CAPACITY;
    _InterlockedIncrement64(&queue->length);
  }
}

Void Message_dequeue(Message_Queue* queue, Message_Message* out)
{
  const Index length = queue->length;
  if (length > 0) {
    *out = queue->buffer[queue->consumer];
    queue->consumer = (queue->consumer + 1) % Message_QUEUE_CAPACITY;
    _InterlockedDecrement64(&queue->length);
  }
}

Index Message_queue_length(const Message_Queue* queue)
{
  return queue->length;
}
