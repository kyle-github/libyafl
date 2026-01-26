#include "include/fcontext.h"
#include <stdio.h>

int main() {
    printf("sizeof(fcontext_stack_t) = %zu\n", sizeof(fcontext_stack_t));
    printf("sizeof(uint32_t) = %zu\n", sizeof(uint32_t));
    printf("sizeof(fcontext_alloc_type_t) = %zu\n", sizeof(fcontext_alloc_type_t));
    printf("sizeof(size_t) = %zu\n", sizeof(size_t));
    printf("sizeof(bool) = %zu\n", sizeof(bool));
    printf("sizeof(void*) = %zu\n", sizeof(void*));
    printf("sizeof(fcontext_t) = %zu\n", sizeof(fcontext_t));
    return 0;
}
