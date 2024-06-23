#include <stdatomic.h>
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

Index message_queue_length(const MessageQueue* queue)
{
  return atomic_load(&queue->length);
}

Void message_enqueue(MessageQueue* queue, Message message)
{
  if (message_queue_length(queue) < MESSAGE_QUEUE_CAPACITY) {

    // write the message into the buffer and update the producer head
    queue->buffer[queue->producer] = message;
    queue->producer = (queue->producer + 1) % MESSAGE_QUEUE_CAPACITY;

    // increment the queue length
    Bool swapped = false;
    while (swapped == false) {
      Index length = message_queue_length(queue);
      swapped = atomic_compare_exchange_weak(&queue->length, &length, length + 1);
    }

  }
}

Void message_dequeue(MessageQueue* queue, Message* out)
{
  if (message_queue_length(queue) > 0) {

    // read the message out
    *out = queue->buffer[queue->consumer];
    queue->consumer = (queue->consumer + 1) % MESSAGE_QUEUE_CAPACITY;

    // decrement the queue length
    Bool swapped = false;
    while (swapped == false) {
      Index length = message_queue_length(queue);
      swapped = atomic_compare_exchange_weak(&queue->length, &length, length - 1);
    }

  }
}
