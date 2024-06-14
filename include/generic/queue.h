/*******************************************************************************
 * GENERIC QUEUE
 *
 * The user should define QUEUE_ELEMENT as the type parameter.
 ******************************************************************************/

#ifndef GENERIC_QUEUE_H
#define GENERIC_QUEUE_H

#include "prelude.h"

#define QUEUE_CAT(a, ...) QUEUE_CAT_(a, __VA_ARGS__)
#define QUEUE_CAT_(a, ...) a ## __VA_ARGS__
#define QUEUE_TYPE(T) QUEUE_CAT(Queue, T)
#define QUEUE_INIT(T) QUEUE_CAT(queue_init_, T)
#define QUEUE_ENQUEUE(T) QUEUE_CAT(queue_enqueue_, T)
#define QUEUE_DEQUEUE(T) QUEUE_CAT(queue_dequeue_, T)
#define QUEUE_LENGTH(T) QUEUE_CAT(queue_length_, T)

#endif

typedef struct {
    S32 capacity;
    S32 head;
    S32 tail;
    QUEUE_ELEMENT* data;
} QUEUE_TYPE(QUEUE_ELEMENT);

static inline Void QUEUE_INIT(QUEUE_ELEMENT)(
        QUEUE_TYPE(QUEUE_ELEMENT)* queue,
        QUEUE_ELEMENT* data,
        S32 capacity
        )
{
    queue->capacity = capacity;
    queue->head = 0;
    queue->tail = 0;
    queue->data = data;
}

static inline Void QUEUE_ENQUEUE(QUEUE_ELEMENT)(
        QUEUE_TYPE(QUEUE_ELEMENT)* queue,
        QUEUE_ELEMENT element
        )
{
    const S32 new_head = (queue->head + 1) % queue->capacity;
    if (new_head != queue->tail) {
        queue->data[queue->head] = element;
        queue->head = new_head;
    }
}

static inline QUEUE_ELEMENT QUEUE_DEQUEUE(QUEUE_ELEMENT)(
        QUEUE_TYPE(QUEUE_ELEMENT)* queue,
        QUEUE_ELEMENT sentinel
        )
{
    if (queue->tail != queue->head) {
        const QUEUE_ELEMENT element = queue->data[queue->tail];
        queue->tail = (queue->tail + 1) % queue->capacity;
        return element;
    } else {
        return sentinel;
    }
}

static inline S32 QUEUE_LENGTH(QUEUE_ELEMENT)(
        const QUEUE_TYPE(QUEUE_ELEMENT)* queue
        )
{
    const S32 d = queue->head - queue->tail;
    return d >= 0 ? d : d + queue->capacity;
}

#undef QUEUE_ELEMENT
