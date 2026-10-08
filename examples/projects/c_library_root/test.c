#include "stack.h"
int main(void) {
    stack s = {0};
    stack_push(&s, 1); stack_push(&s, 2);
    int ok = stack_pop(&s) == 2 && stack_pop(&s) == 1;
    stack_free(&s);
    return ok ? 0 : 1;
}
