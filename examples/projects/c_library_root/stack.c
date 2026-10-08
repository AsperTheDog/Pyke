#include "stack.h"
#include <stdlib.h>
#include <math.h>
void stack_push(stack* s, int v) {
    if (s->size == s->cap) { s->cap = s->cap ? s->cap * 2 : 4; s->data = realloc(s->data, s->cap * sizeof(int)); }
    s->data[s->size++] = v;
}
int stack_pop(stack* s) { return s->data[--s->size]; }
void stack_free(stack* s) { free(s->data); s->data = NULL; s->size = s->cap = 0; (void)sqrt(4.0); }
