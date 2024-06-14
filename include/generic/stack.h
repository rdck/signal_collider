/*******************************************************************************
 * GENERIC STACK
 *
 * The user should define STACK_ELEMENT as the type parameter.
 ******************************************************************************/

#ifndef GENERIC_STACK_H
#define GENERIC_STACK_H

#include "prelude.h"

#define STACK_CAT(a, ...) STACK_CAT_(a, __VA_ARGS__)
#define STACK_CAT_(a, ...) a ## __VA_ARGS__
#define STACK_TYPE(T) STACK_CAT(Stack, T)
#define STACK_INIT(T) STACK_CAT(stack_init_, T)
#define STACK_PUSH(T) STACK_CAT(stack_push_, T)
#define STACK_POP(T) STACK_CAT(stack_pop_, T)
#define STACK_SIZE(T) STACK_CAT(stack_size_, T)

#endif

typedef struct {
    S32 capacity;
    S32 head;
    STACK_ELEMENT* data;
} STACK_TYPE(STACK_ELEMENT);

static inline Void STACK_INIT(STACK_ELEMENT)(
        STACK_TYPE(STACK_ELEMENT)* stack,
        STACK_ELEMENT* data,
        S32 capacity
        )
{
    stack->capacity = capacity;
    stack->head = 0;
    stack->data = data;
}

static inline Void STACK_PUSH(STACK_ELEMENT)(
        STACK_TYPE(STACK_ELEMENT)* stack,
        STACK_ELEMENT element
        )
{
    if (stack->head < stack->capacity) {
        stack->data[stack->head] = element;
        stack->head += 1;
    }
}

static inline STACK_ELEMENT STACK_POP(STACK_ELEMENT)(
        STACK_TYPE(STACK_ELEMENT)* stack,
        STACK_ELEMENT sentinel
        )
{
    if (stack->head > 0) {
        stack->head -= 1;
        return stack->data[stack->head];
    } else {
        return sentinel;
    }
}

static inline S32 STACK_SIZE(STACK_ELEMENT)(
        const STACK_TYPE(STACK_ELEMENT)* stack
        )
{
    return stack->head;
}

#undef STACK_ELEMENT
