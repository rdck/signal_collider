/*******************************************************************************
 * GENERIC LOCKLESS QUEUE
 *
 * The user should define GENERIC as the type parameter.
 ******************************************************************************/

#ifndef LOCKLESS_QUEUE_H
#define LOCKLESS_QUEUE_H

#include "prelude.h"

#define LOCKLESS_QUEUE_TYPE(T) CAT(T, _lockless_queue_Queue)
#define LOCKLESS_QUEUE_INIT(T) CAT(T, _lockless_queue_init)
#define LOCKLESS_QUEUE_ENQUEUE(T) CAT(T, _lockless_queue_enqueue)
#define LOCKLESS_QUEUE_DEQUEUE(T) CAT(T, _lockless_queue_dequeue)
#define LOCKLESS_QUEUE_LENGTH(T) CAT(T, _lockless_queue_length)

#endif // LOCKLESS_QUEUE_H

#define TYPE_INSTANCE LOCKLESS_QUEUE_TYPE(GENERIC)
#define INIT_INSTANCE LOCKLESS_QUEUE_INIT(GENERIC)
#define ENQUEUE_INSTANCE LOCKLESS_QUEUE_ENQUEUE(GENERIC)
#define DEQUEUE_INSTANCE LOCKLESS_QUEUE_DEQUEUE(GENERIC)
#define LENGTH_INSTANCE LOCKLESS_QUEUE_LENGTH(GENERIC)

typedef struct {
    S32 capacity;
    S32 producer_head;
    S32 consumer_head;
    volatile GENERIC* ring;
    volatile S32 head;
} TYPE_INSTANCE;

static inline Void INIT_INSTANCE(
        TYPE_INSTANCE* queue,
        volatile GENERIC* ring,
        S32 capacity
        )
{
    queue->capacity = capacity;
    queue->ring = ring;
    queue->producer_head = 0;
    queue->consumer_head = 0;
    queue->head = 0;
}

static inline S32 LENGTH_INSTANCE(const TYPE_INSTANCE* queue)
{
    return queue->head;
}

#ifdef _WIN32

// @K-MONK: We could interpose an abstraction of the atomic operations, so that
// we don't have to include the full windows header here.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static_assert(sizeof(LONG) == sizeof(S32), "sizeof(LONG) == sizeof(S32)");

static inline Void ENQUEUE_INSTANCE(TYPE_INSTANCE* queue, GENERIC element)
{
    const S32 head = queue->head;
    if (head < queue->capacity) {
        queue->ring[queue->producer_head] = element;
        queue->producer_head = (queue->producer_head + 1) % queue->capacity;
        InterlockedIncrement((volatile LONG*) &queue->head);
    }
}

static inline GENERIC DEQUEUE_INSTANCE(TYPE_INSTANCE* queue, GENERIC sentinel)
{
    const S32 head = queue->head;
    if (head > 0) {
        const GENERIC element = queue->ring[queue->consumer_head];
        queue->ring[queue->consumer_head] = (GENERIC) {0};
        queue->consumer_head = (queue->consumer_head + 1) % queue->capacity;
        InterlockedDecrement((volatile LONG*) &queue->head);
        return element;
    } else {
        return sentinel;
    }
}

#undef TYPE_INSTANCE
#undef INIT_INSTANCE
#undef ENQUEUE_INSTANCE
#undef DEQUEUE_INSTANCE
#undef HEADROOM_INSTANCE
#undef GENERIC

#endif // _WIN32
