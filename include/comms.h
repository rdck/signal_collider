#pragma once

#include "prelude.h"

#define ATOMIC_QUEUE_ELEMENT Index
#define ATOMIC_QUEUE_INTERFACE
#include "generic/atomic_queue.h"

// FIFO of allocation messages from render thread to audio thread
extern ATOMIC_QUEUE_TYPE(Index) allocation_queue;

// FIFO of allocation messages from audio thread to render thread
extern ATOMIC_QUEUE_TYPE(Index) free_queue;
