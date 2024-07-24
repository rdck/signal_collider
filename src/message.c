#include <stdatomic.h>
#include "message.h"
#include "log.h"

Message message_write(V2S point, Value value)
{
  Message out;
  out.tag = MESSAGE_WRITE;
  out.write.point = point;
  out.write.value = value;
  return out;
}

Message message_power(V2S point)
{
  Message out;
  out.tag = MESSAGE_POWER;
  out.write.point = point;
  out.write.value = value_none;
  return out;
}

Message message_clear()
{
  Message out;
  out.tag = MESSAGE_CLEAR;
  return out;
}

Message message_alloc(Index index)
{
  Message out;
  out.tag = MESSAGE_ALLOCATE;
  out.alloc.index = index;
  return out;
}

Message message_load(ModelStorage* storage)
{
  Message out;
  out.tag = MESSAGE_LOAD;
  out.storage = storage;
  return out;
}

Message message_tempo(S32 tempo)
{
  Message out;
  out.tag = MESSAGE_TEMPO;
  out.tempo = tempo;
  return out;
}

Message message_palette(Palette* palette)
{
  Message out;
  out.tag = MESSAGE_PALETTE;
  out.palette = palette;
  return out;
}

Message message_global_volume(F32 volume)
{
  Message out;
  out.tag = MESSAGE_GLOBAL_VOLUME;
  out.parameter = volume;
  return out;
}

Message message_envelope_coefficient(F32 coefficient)
{
  Message out;
  out.tag = MESSAGE_ENVELOPE_COEFFICIENT;
  out.parameter = coefficient;
  return out;
}

Message message_envelope_exponent(F32 exponent)
{
  Message out;
  out.tag = MESSAGE_ENVELOPE_EXPONENT;
  out.parameter = exponent;
  return out;
}

Message message_reverb_status(Bool status)
{
  Message out;
  out.tag = MESSAGE_REVERB_STATUS;
  out.flag = status;
  return out;
}

Message message_reverb_size(F32 size)
{
  Message out;
  out.tag = MESSAGE_REVERB_SIZE;
  out.parameter = size;
  return out;
}

Message message_reverb_cutoff(F32 cutoff)
{
  Message out;
  out.tag = MESSAGE_REVERB_CUTOFF;
  out.parameter = cutoff;
  return out;
}

Message message_reverb_mix(F32 mix)
{
  Message out;
  out.tag = MESSAGE_REVERB_MIX;
  out.parameter = mix;
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
  } else {
    platform_log_warn("message queue is full");
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
