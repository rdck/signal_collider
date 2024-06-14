#ifndef QUEUE_MACROS_H
#define QUEUE_MACROS_H

#define QUEUE_CAT(a, ...) QUEUE_CAT_(a, __VA_ARGS__)
#define QUEUE_CAT_(a, ...) a ## __VA_ARGS__
#define QUEUE_TYPE(T) QUEUE_CAT(Queue, T)
#define QUEUE_INIT(T) QUEUE_CAT(queue_init_, T)
#define QUEUE_ENQUEUE(T) QUEUE_CAT(queue_enqueue_, T)
#define QUEUE_DEQUEUE(T) QUEUE_CAT(queue_dequeue_, T)
#define QUEUE_LENGTH(T) QUEUE_CAT(queue_length_, T)

#endif
