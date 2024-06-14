#ifndef STACK_MACROS_H
#define STACK_MACROS_H

#define STACK_CAT(a, ...) STACK_CAT_(a, __VA_ARGS__)
#define STACK_CAT_(a, ...) a ## __VA_ARGS__
#define STACK_TYPE(T) STACK_CAT(Stack, T)
#define STACK_INIT(T) STACK_CAT(stack_init_, T)
#define STACK_PUSH(T) STACK_CAT(stack_push_, T)
#define STACK_POP(T) STACK_CAT(stack_pop_, T)
#define STACK_SIZE(T) STACK_CAT(stack_size_, T)

#endif
