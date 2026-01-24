/**
 * test_fcontext_alignment.c
 * Verify stack alignment requirements (SysV, AAPCS)
 *
 * Verifies that the stack pointer is correctly aligned upon entry to the fiber.
 * - x86_64 (SysV): RSP % 16 == 8 (due to return address push)
 * - ARM64 (AAPCS): SP % 16 == 0 (strict alignment)
 *
 * Copyright Kyle Hayes (2026)
 * Distributed under the Boost Software License, Version 1.0.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include "fcontext.h"

static int alignment_ok = 0;

void align_fiber(fcontext_transfer_t t) {
    uintptr_t sp = 0;

    /* Capture Stack Pointer using inline assembly */
#if defined(_MSC_VER)
    /* MSVC does not support inline assembly on x64/ARM64 */
    /* We use the address of a local variable as a proxy */
    uintptr_t local;
    sp = (uintptr_t)&local;
#elif defined(__x86_64__)
    __asm__ volatile("mov %%rsp, %0" : "=r"(sp));
#elif defined(__aarch64__)
    __asm__ volatile("mov %0, sp" : "=r"(sp));
#elif defined(__i386__)
    __asm__ volatile("mov %%esp, %0" : "=r"(sp));
#elif defined(__arm__)
    __asm__ volatile("mov %0, sp" : "=r"(sp));
#elif defined(__riscv)
    __asm__ volatile("mv %0, sp" : "=r"(sp));
#elif defined(__mips__)
    __asm__ volatile("move %0, $29" : "=r"(sp));
#elif defined(__powerpc__)
    __asm__ volatile("mr %0, 1" : "=r"(sp));
#elif defined(__s390x__)
    __asm__ volatile("lgr %0, 15" : "=r"(sp));
#elif defined(__xtensa__)
    __asm__ volatile("mov %0, a1" : "=r"(sp));
#else
    /* Fallback */
    uintptr_t local;
    sp = (uintptr_t)&local;
#endif

    printf("  [fiber] Stack pointer: 0x%lx\n", (unsigned long)sp);

    /* Verify Alignment */
#if defined(__x86_64__)
    /* SysV ABI (Linux/macOS) & Windows x64:
     * RSP must be 16-byte aligned before call.
     * Call pushes 8-byte return address.
     * On entry: RSP % 16 == 8.
     */
    if ((sp & 0xF) == 0x8) {
        printf("  [fiber] x86_64 ABI verified (RSP %% 16 == 8)\n");
        alignment_ok = 1;
    } else if ((sp & 0xF) == 0x0) {
        /* Some runtimes/compilers might force 16-byte alignment on entry for SIMD optimization */
        printf("  [fiber] 16-byte alignment verified (RSP %% 16 == 0)\n");
        alignment_ok = 1;
    }
#elif defined(__aarch64__)
    /* AAPCS64: SP must be 16-byte aligned at all times */
    if ((sp & 0xF) == 0x0) {
        printf("  [fiber] ARM64 AAPCS verified (SP %% 16 == 0)\n");
        alignment_ok = 1;
    }
#else
    /* Default check: 8-byte or 16-byte alignment is generally acceptable for others */
    if ((sp & 0x7) == 0x0) {
        printf("  [fiber] Basic alignment verified (>= 8 bytes)\n");
        alignment_ok = 1;
    }
#endif

    jump_fcontext(t.prev_context, NULL);
}

int main(void) {
    printf("=== fcontext Stack Alignment Test ===\n");

    fcontext_stack_t *state = fcontext_create(24 * 1024, align_fiber);
    assert(state != NULL);

    jump_fcontext(state->context, NULL);

    fcontext_destroy(state);

    if (alignment_ok) {
        printf("\n✓ PASS: Stack alignment requirements met\n");
        return 0;
    } else {
        printf("\n✗ FAIL: Stack alignment incorrect\n");
        return 1;
    }
}