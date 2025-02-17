#pragma once

#include "model.h"
#include "sound.h"

#define MESSAGE_QUEUE_CAPACITY 0x100

typedef struct WriteMessage {
  V2S point;
  Value value;
} WriteMessage;

typedef struct PowerMessage {
  V2S point;
} PowerMessage;

typedef struct SoundMessage {
  S32 slot;
  Sound sound;
} SoundMessage;

typedef struct ResizeMessage {
  ProgramHistory primary;
  ProgramHistory secondary;
} ResizeMessage;

typedef enum ControlMessageTag {
  CONTROL_MESSAGE_NONE,
  CONTROL_MESSAGE_WRITE,
  CONTROL_MESSAGE_POWER,
  CONTROL_MESSAGE_SOUND,
  CONTROL_MESSAGE_TEMPO,
  CONTROL_MESSAGE_MEMORY_RESIZE,
  CONTROL_MESSAGE_CLEAR,
  CONTROL_MESSAGE_CARDINAL,
} ControlMessageTag;

typedef struct ControlMessage {
  ControlMessageTag tag;
  union {
    WriteMessage write;
    PowerMessage power;
    SoundMessage sound;
    ResizeMessage resize;
    S32 tempo;
  };
} ControlMessage;

ControlMessage control_message_write(V2S point, Value value);
ControlMessage control_message_power(V2S point);
ControlMessage control_message_sound(S32 slot, Sound sound);
ControlMessage control_message_tempo(S32 tempo);
ControlMessage control_message_memory_resize(ProgramHistory primary, ProgramHistory secondary);
ControlMessage control_message_clear();

#define ATOMIC_QUEUE_ELEMENT Index
#define ATOMIC_QUEUE_INTERFACE
#include "generic/atomic_queue.h"

#define ATOMIC_QUEUE_ELEMENT ControlMessage
#define ATOMIC_QUEUE_INTERFACE
#include "generic/atomic_queue.h"

// FIFO of allocation messages from render thread to audio thread
extern ATOMIC_QUEUE_TYPE(Index) allocation_queue;

// FIFO of allocation messages from audio thread to render thread
extern ATOMIC_QUEUE_TYPE(Index) free_queue;

// FIFO of control messages from render thread to audio thread
extern ATOMIC_QUEUE_TYPE(ControlMessage) control_queue;
