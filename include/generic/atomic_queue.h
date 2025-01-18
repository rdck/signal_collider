/*******************************************************************************
 * GENERIC LOCK FREE QUEUE
 *
 * Define ATOMIC_QUEUE_ELEMENT as the type parameter.
 ******************************************************************************/

#ifndef ATOMIC_QUEUE_H
#define ATOMIC_QUEUE_H

#include <stdatomic.h>
#include "prelude.h"

#define ATOMIC_QUEUE_CAT(a, ...) ATOMIC_QUEUE_CAT_(a, __VA_ARGS__)
#define ATOMIC_QUEUE_CAT_(a, ...) a##__VA_ARGS__
#define ATOMIC_QUEUE_TYPE(T) ATOMIC_QUEUE_CAT(AtomicQueue, T)
#define ATOMIC_QUEUE_INIT(T) ATOMIC_QUEUE_CAT(atomic_queue_init_, T)
#define ATOMIC_QUEUE_ENQUEUE(T) ATOMIC_QUEUE_CAT(atomic_queue_enqueue_, T)
#define ATOMIC_QUEUE_DEQUEUE(T) ATOMIC_QUEUE_CAT(atomic_queue_dequeue_, T)
#define ATOMIC_QUEUE_LENGTH(T) ATOMIC_QUEUE_CAT(atomic_queue_length_, T)

#endif

#ifdef ATOMIC_QUEUE_STATIC
#define ATOMIC_QUEUE_SCOPE static
#else
#define ATOMIC_QUEUE_SCOPE extern
#endif

#ifdef ATOMIC_QUEUE_INTERFACE

typedef struct ATOMIC_QUEUE_TYPE(ATOMIC_QUEUE_ELEMENT) {
  Index capacity;
  Index producer;
  Index consumer;
  _Atomic Index length;
  ATOMIC_QUEUE_ELEMENT* buffer;
} ATOMIC_QUEUE_TYPE(ATOMIC_QUEUE_ELEMENT);

ATOMIC_QUEUE_SCOPE Void ATOMIC_QUEUE_INIT(ATOMIC_QUEUE_ELEMENT)(
    ATOMIC_QUEUE_TYPE(ATOMIC_QUEUE_ELEMENT)* queue,
    ATOMIC_QUEUE_ELEMENT* buffer,
    Index capacity);

ATOMIC_QUEUE_SCOPE Void ATOMIC_QUEUE_ENQUEUE(ATOMIC_QUEUE_ELEMENT)(
    ATOMIC_QUEUE_TYPE(ATOMIC_QUEUE_ELEMENT)* queue,
    ATOMIC_QUEUE_ELEMENT element);

ATOMIC_QUEUE_SCOPE ATOMIC_QUEUE_ELEMENT ATOMIC_QUEUE_DEQUEUE(ATOMIC_QUEUE_ELEMENT)(
    ATOMIC_QUEUE_TYPE(ATOMIC_QUEUE_ELEMENT)* queue,
    ATOMIC_QUEUE_ELEMENT sentinel);

ATOMIC_QUEUE_SCOPE Index ATOMIC_QUEUE_LENGTH(ATOMIC_QUEUE_ELEMENT)(
    const ATOMIC_QUEUE_TYPE(ATOMIC_QUEUE_ELEMENT)* queue);

#endif // end interface

#ifdef ATOMIC_QUEUE_IMPLEMENTATION

ATOMIC_QUEUE_SCOPE Void ATOMIC_QUEUE_INIT(ATOMIC_QUEUE_ELEMENT)(
    ATOMIC_QUEUE_TYPE(ATOMIC_QUEUE_ELEMENT)* queue,
    ATOMIC_QUEUE_ELEMENT* buffer,
    Index capacity)
{
  queue->capacity = capacity;
  queue->producer = 0;
  queue->consumer = 0;
  queue->length = 0;
  queue->buffer = buffer;
}

ATOMIC_QUEUE_SCOPE Void ATOMIC_QUEUE_ENQUEUE(ATOMIC_QUEUE_ELEMENT)(
    ATOMIC_QUEUE_TYPE(ATOMIC_QUEUE_ELEMENT)* queue,
    ATOMIC_QUEUE_ELEMENT element)
{
  if (ATOMIC_QUEUE_LENGTH(ATOMIC_QUEUE_ELEMENT)(queue) < queue->capacity) {

    // write the message into the buffer and update the producer head
    queue->buffer[queue->producer] = element;
    queue->producer = (queue->producer + 1) % queue->capacity;

    // increment the queue length
    Bool swapped = false;
    while (swapped == false) {
      Index length = ATOMIC_QUEUE_LENGTH(ATOMIC_QUEUE_ELEMENT)(queue);
      swapped = atomic_compare_exchange_weak(&queue->length, &length, length + 1);
    }

  }
}

ATOMIC_QUEUE_SCOPE ATOMIC_QUEUE_ELEMENT ATOMIC_QUEUE_DEQUEUE(ATOMIC_QUEUE_ELEMENT)(
    ATOMIC_QUEUE_TYPE(ATOMIC_QUEUE_ELEMENT)* queue,
    ATOMIC_QUEUE_ELEMENT sentinel)
{
  if (ATOMIC_QUEUE_LENGTH(ATOMIC_QUEUE_ELEMENT)(queue) > 0) {

    // read the message out
    const ATOMIC_QUEUE_ELEMENT out = queue->buffer[queue->consumer];
    queue->consumer = (queue->consumer + 1) % queue->capacity;

    // decrement the queue length
    Bool swapped = false;
    while (swapped == false) {
      Index length = ATOMIC_QUEUE_LENGTH(ATOMIC_QUEUE_ELEMENT)(queue);
      swapped = atomic_compare_exchange_weak(&queue->length, &length, length - 1);
    }

    return out;

  } else {

    return sentinel;

  }
}

ATOMIC_QUEUE_SCOPE Index ATOMIC_QUEUE_LENGTH(ATOMIC_QUEUE_ELEMENT)(
    const ATOMIC_QUEUE_TYPE(ATOMIC_QUEUE_ELEMENT)* queue)
{
  return atomic_load(&queue->length);
}

#endif // end implementation

#undef ATOMIC_QUEUE_INTERFACE
#undef ATOMIC_QUEUE_IMPLEMENTATION
#undef ATOMIC_QUEUE_ELEMENT
#undef ATOMIC_QUEUE_SCOPE
