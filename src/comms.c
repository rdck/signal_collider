#include "comms.h"

#define ATOMIC_QUEUE_ELEMENT Index
#define ATOMIC_QUEUE_IMPLEMENTATION
#include "generic/atomic_queue.h"

ATOMIC_QUEUE_TYPE(Index) allocation_queue = {0};
ATOMIC_QUEUE_TYPE(Index) free_queue = {0};
