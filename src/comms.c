#include "comms.h"

#define ATOMIC_QUEUE_ELEMENT Index
#define ATOMIC_QUEUE_IMPLEMENTATION
#include "generic/atomic_queue.h"

#define ATOMIC_QUEUE_ELEMENT ControlMessage
#define ATOMIC_QUEUE_IMPLEMENTATION
#include "generic/atomic_queue.h"

ATOMIC_QUEUE_TYPE(Index) allocation_queue = {0};
ATOMIC_QUEUE_TYPE(Index) free_queue = {0};
ATOMIC_QUEUE_TYPE(ControlMessage) control_queue = {0};

ControlMessage control_message_write(V2S point, Value value)
{
  ControlMessage message;
  message.tag = CONTROL_MESSAGE_WRITE;
  message.write.point = point;
  message.write.value = value;
  return message;
}

ControlMessage control_message_power(V2S point)
{
  ControlMessage message;
  message.tag = CONTROL_MESSAGE_POWER;
  message.power.point = point;
  return message;
}

ControlMessage control_message_sound(S32 slot, Sound sound)
{
  ControlMessage message;
  message.tag = CONTROL_MESSAGE_SOUND;
  message.sound.slot = slot;
  message.sound.sound = sound;
  return message;
}

ControlMessage control_message_tempo(S32 tempo)
{
  ControlMessage message;
  message.tag = CONTROL_MESSAGE_TEMPO;
  message.tempo = tempo;
  return message;
}

ControlMessage control_message_memory_resize(V2S dimensions)
{
  ControlMessage message;
  message.tag = CONTROL_MESSAGE_MEMORY_RESIZE;
  message.dimensions = dimensions;
  return message;
}
