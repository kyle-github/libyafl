#include "include/yafl.h"
#include <stdio.h>

int main() {
    printf("sizeof(yafl_stack_t) = %zu\n", sizeof(yafl_stack_t));
    printf("sizeof(uint32_t) = %zu\n", sizeof(uint32_t));
    printf("sizeof(yafl_alloc_type_t) = %zu\n", sizeof(yafl_alloc_type_t));
    printf("sizeof(size_t) = %zu\n", sizeof(size_t));
    printf("sizeof(bool) = %zu\n", sizeof(bool));
    printf("sizeof(void*) = %zu\n", sizeof(void *));
    printf("sizeof(yafl_t) = %zu\n", sizeof(yafl_t));
    return 0;
}
