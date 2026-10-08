#pragma once
#include <stddef.h>
typedef struct { int* data; size_t size, cap; } stack;
void stack_push(stack* s, int v);
int stack_pop(stack* s);
void stack_free(stack* s);
