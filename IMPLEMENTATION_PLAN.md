# fcontext Test Implementation Plan

**Target:** src/tests/fcontext/
**Goal:** Create standalone test environment for fcontext (fiber/stackful coroutine) implementation
**Assignee:** Claude Haiku
**Date:** 2026-01-21

---

## Overview

Extract assembly code from Boost.Context and wrapper code from DaoWen/fcontext to create a standalone, tested fcontext implementation supporting multiple architectures. The code must be pure C11, independent of the main libplctag project, and demonstrate asymmetric coroutine/green thread behavior.

---

## Architecture Support Requirements

Extract assembly for ALL supported architectures from Boost.Context:

### Primary Architectures (Priority 1)
- x86_64 (Intel/AMD 64-bit)
- ARM64/AArch64 (Apple Silicon, modern ARM servers)
- x86 / i386 (32-bit Intel)
- ARM (32-bit ARM)

### Secondary Architectures (Priority 2)
- RISC-V 64-bit
- MIPS32
- MIPS64
- PowerPC 32-bit
- PowerPC 64-bit

### Tertiary Architectures (Priority 3)
- LoongArch64
- s390x (IBM Z)
- SPARC64

### Platform Variants per Architecture
For each architecture, support these OS/ABI combinations where applicable:
- **Linux**: `sysv_elf_gas` (System V ABI, ELF format, GNU assembler)
- **macOS**: `sysv_macho_gas` or `aapcs_macho_gas` (Mach-O format)
- **Windows**: `ms_pe_clang_gas` or `ms_pe_masm` (Microsoft PE format)

---

## Directory Structure

Create the following structure in `src/tests/fcontext/`:

```
src/tests/fcontext/
├── IMPLEMENTATION_PLAN.md        # This file
├── README.md                      # Usage documentation
├── LICENSE                        # Boost Software License 1.0
├── Makefile                       # Build system
├── CMakeLists.txt                 # Alternative build system
│
├── include/
│   ├── fcontext.h                 # Low-level fcontext API (from DaoWen)
│   └── ev_fiber.h                 # High-level unified fiber API (Windows-compatible)
│
├── src/
│   ├── fcontext_wrapper.c         # Low-level fcontext helpers
│   ├── ev_fiber.c                 # Unified fiber API implementation
│   └── asm/                       # Assembly files from Boost.Context
│       ├── README.md              # Assembly file organization
│       │
│       ├── x86_64/
│       │   ├── make_x86_64_sysv_elf_gas.S
│       │   ├── jump_x86_64_sysv_elf_gas.S
│       │   ├── ontop_x86_64_sysv_elf_gas.S
│       │   ├── make_x86_64_sysv_macho_gas.S
│       │   ├── jump_x86_64_sysv_macho_gas.S
│       │   ├── ontop_x86_64_sysv_macho_gas.S
│       │   ├── make_x86_64_ms_pe_clang_gas.S
│       │   ├── jump_x86_64_ms_pe_clang_gas.S
│       │   └── ontop_x86_64_ms_pe_clang_gas.S
│       │
│       ├── arm64/
│       │   ├── make_arm64_aapcs_elf_gas.S
│       │   ├── jump_arm64_aapcs_elf_gas.S
│       │   ├── ontop_arm64_aapcs_elf_gas.S
│       │   ├── make_arm64_aapcs_macho_gas.S
│       │   ├── jump_arm64_aapcs_macho_gas.S
│       │   ├── ontop_arm64_aapcs_macho_gas.S
│       │   ├── make_arm64_aapcs_pe_armclang.S
│       │   ├── jump_arm64_aapcs_pe_armclang.S
│       │   └── ontop_arm64_aapcs_pe_armclang.S
│       │
│       ├── i386/
│       │   ├── make_i386_sysv_elf_gas.S
│       │   ├── jump_i386_sysv_elf_gas.S
│       │   ├── ontop_i386_sysv_elf_gas.S
│       │   ├── make_i386_sysv_macho_gas.S
│       │   ├── jump_i386_sysv_macho_gas.S
│       │   ├── ontop_i386_sysv_macho_gas.S
│       │   ├── make_i386_ms_pe_clang_gas.S
│       │   ├── jump_i386_ms_pe_clang_gas.S
│       │   └── ontop_i386_ms_pe_clang_gas.S
│       │
│       ├── arm/
│       │   ├── make_arm_aapcs_elf_gas.S
│       │   ├── jump_arm_aapcs_elf_gas.S
│       │   ├── ontop_arm_aapcs_elf_gas.S
│       │   ├── make_arm_aapcs_macho_gas.S
│       │   ├── jump_arm_aapcs_macho_gas.S
│       │   └── ontop_arm_aapcs_macho_gas.S
│       │
│       ├── riscv64/
│       │   ├── make_riscv64_sysv_elf_gas.S
│       │   ├── jump_riscv64_sysv_elf_gas.S
│       │   └── ontop_riscv64_sysv_elf_gas.S
│       │
│       ├── mips32/
│       │   ├── make_mips32_o32_elf_gas.S
│       │   ├── jump_mips32_o32_elf_gas.S
│       │   └── ontop_mips32_o32_elf_gas.S
│       │
│       ├── mips64/
│       │   ├── make_mips64_n64_elf_gas.S
│       │   ├── jump_mips64_n64_elf_gas.S
│       │   └── ontop_mips64_n64_elf_gas.S
│       │
│       ├── ppc32/
│       │   ├── make_ppc32_sysv_elf_gas.S
│       │   ├── jump_ppc32_sysv_elf_gas.S
│       │   ├── ontop_ppc32_sysv_elf_gas.S
│       │   ├── make_ppc32_sysv_macho_gas.S
│       │   ├── jump_ppc32_sysv_macho_gas.S
│       │   ├── ontop_ppc32_sysv_macho_gas.S
│       │   ├── make_ppc32_sysv_xcoff_gas.S
│       │   ├── jump_ppc32_sysv_xcoff_gas.S
│       │   └── ontop_ppc32_sysv_xcoff_gas.S
│       │
│       ├── ppc64/
│       │   ├── make_ppc64_sysv_elf_gas.S
│       │   ├── jump_ppc64_sysv_elf_gas.S
│       │   ├── ontop_ppc64_sysv_elf_gas.S
│       │   ├── make_ppc64_sysv_macho_gas.S
│       │   ├── jump_ppc64_sysv_macho_gas.S
│       │   ├── ontop_ppc64_sysv_macho_gas.S
│       │   ├── make_ppc64_sysv_xcoff_gas.S
│       │   ├── jump_ppc64_sysv_xcoff_gas.S
│       │   └── ontop_ppc64_sysv_xcoff_gas.S
│       │
│       ├── loongarch64/
│       │   ├── make_loongarch64_sysv_elf_gas.S
│       │   ├── jump_loongarch64_sysv_elf_gas.S
│       │   └── ontop_loongarch64_sysv_elf_gas.S
│       │
│       ├── s390x/
│       │   ├── make_s390x_sysv_elf_gas.S
│       │   ├── jump_s390x_sysv_elf_gas.S
│       │   └── ontop_s390x_sysv_elf_gas.S
│       │
│       └── sparc64/
│           ├── make_sparc64_sysv_elf_gas.S
│           ├── jump_sparc64_sysv_elf_gas.S
│           └── ontop_sparc64_sysv_elf_gas.S
│
└── tests/
    ├── test_fcontext_basic.c      # Low-level fcontext API test
    ├── test_fiber_basic.c         # Basic unified fiber API test
    ├── test_fiber_data.c          # Test data passing with unified API
    ├── test_fiber_asymmetric.c    # Test asymmetric pattern with scheduler
    ├── test_fiber_stack.c         # Test stack watermarking
    ├── test_fiber_stress.c        # Stress test with many fibers
    └── test_fiber_threads.c       # Green thread simulation with unified API
```

---

## API Design Overview

This implementation provides **two API layers**:

### Low-Level API: fcontext (include/fcontext.h)

Direct wrapper around Boost.Context assembly. For advanced users who need full control.

```c
typedef struct fcontext_opaque_t *fcontext_t;
typedef void (*fcontext_fn_t)(fcontext_transfer_t);

fcontext_t make_fcontext(void *sp, size_t size, fcontext_fn_t fn);
fcontext_transfer_t jump_fcontext(fcontext_t const to, void *vp);
```

**Use when:**
- Maximum performance needed
- Custom context switching patterns
- Research/experimentation

### High-Level API: Unified Fibers (include/ev_fiber.h)

Windows Fibers-compatible API for portable code. Recommended for most users.

```c
typedef struct ev_fiber_s *ev_fiber_t;
typedef void (*ev_fiber_func_t)(void *arg);

int ev_fiber_init_thread(void);
ev_fiber_t ev_fiber_create(size_t stack_size, ev_fiber_func_t func, void *arg);
void ev_fiber_switch(ev_fiber_t fiber);
ev_fiber_t ev_fiber_current(void);
void ev_fiber_destroy(ev_fiber_t fiber);

// POSIX-only extensions
size_t ev_fiber_stack_usage(ev_fiber_t fiber);  // Stack watermarking
```

**Use when:**
- Need Windows compatibility
- Want thread-like programming model
- Building green thread systems
- Need stack measurement (POSIX)

**Key differences:**
- Unified API matches Windows Fibers semantics
- Entry function signature: `void func(void *arg)` (not `func(fcontext_transfer_t)`)
- No manual context tracking needed
- Built-in stack watermarking (POSIX)
- 24KB default stack size

---

## Task Breakdown

### Task 1: Extract Boost.Context Assembly Files

**Input:** Boost.Context repository (https://github.com/boostorg/context.git, branch: develop)
**Output:** Assembly files organized by architecture in `src/asm/`

**Steps:**

1. Clone Boost.Context repository:
   ```bash
   cd /tmp
   git clone --depth 1 --branch develop https://github.com/boostorg/context.git
   ```

2. For each architecture listed above, copy the three assembly files:
   - `make_<arch>_<abi>_<os>_<asm>.S` → context creation
   - `jump_<arch>_<abi>_<os>_<asm>.S` → context switching
   - `ontop_<arch>_<abi>_<os>_<asm>.S` → switch with function call

3. Copy files to appropriate subdirectories:
   ```bash
   # Example for x86_64 Linux:
   cp boost-context/src/asm/make_x86_64_sysv_elf_gas.S src/tests/fcontext/src/asm/x86_64/
   cp boost-context/src/asm/jump_x86_64_sysv_elf_gas.S src/tests/fcontext/src/asm/x86_64/
   cp boost-context/src/asm/ontop_x86_64_sysv_elf_gas.S src/tests/fcontext/src/asm/x86_64/
   ```

4. Create `src/asm/README.md` documenting:
   - Which file came from where
   - Architecture naming conventions
   - How to select correct files at build time

5. Preserve Boost license headers in all files

**Acceptance Criteria:**
- All assembly files copied with correct paths
- README.md documents file organization
- License headers intact

---

### Task 2: Extract and Adapt DaoWen/fcontext Wrapper

**Input:** DaoWen/fcontext repository (https://github.com/DaoWen/fcontext)
**Output:** Pure C11 wrapper code in `include/fcontext.h` and `src/fcontext_wrapper.c`

**Steps:**

1. Clone DaoWen/fcontext:
   ```bash
   cd /tmp
   git clone https://github.com/DaoWen/fcontext.git
   ```

2. Copy `inc/fcontext.h` to `include/fcontext.h`

3. Review for C++ code - if found, rewrite to C11:
   - Replace `inline` with `static inline`
   - Remove any C++ specific features
   - Ensure all code is valid C11

4. Modify header to:
   - Remove the default 256KB stack size (too large)
   - Make stack size configurable
   - Add header guards compatible with C11
   - Add stdint.h, stddef.h, stdlib.h includes
   - Ensure all inline functions are `static inline`

5. Create `src/fcontext_wrapper.c` with:
   - Implementation of any non-inline functions
   - Helper functions if needed
   - Guard page support (optional, can use mmap/VirtualAlloc)

6. Key fcontext API to preserve (low-level):
   ```c
   typedef struct fcontext_opaque_t *fcontext_t;

   typedef struct {
       fcontext_t prev_context;
       void *data;
   } fcontext_transfer_t;

   typedef void (*fcontext_fn_t)(fcontext_transfer_t);

   // Core functions (implemented in assembly)
   extern fcontext_t make_fcontext(void *sp, size_t size, fcontext_fn_t fn);
   extern fcontext_transfer_t jump_fcontext(fcontext_t const to, void *vp);
   extern fcontext_transfer_t ontop_fcontext(fcontext_t const to, void *vp,
                                             fcontext_ontop_fn_t fn);
   ```

7. Add unified fiber API (Windows Fibers-compatible, high-level):
   ```c
   // ev_fiber.h - Unified API that works with Windows Fibers and fcontext

   typedef struct ev_fiber_s *ev_fiber_t;
   typedef void (*ev_fiber_func_t)(void *arg);

   // Core operations (Windows-compatible)
   int ev_fiber_init_thread(void);
   ev_fiber_t ev_fiber_create(size_t stack_size, ev_fiber_func_t func, void *arg);
   void ev_fiber_switch(ev_fiber_t fiber);
   ev_fiber_t ev_fiber_current(void);
   void *ev_fiber_data(void);
   void ev_fiber_destroy(ev_fiber_t fiber);

   // POSIX-only extensions (not available on Windows)
   ev_fiber_t ev_fiber_previous(void);
   void ev_fiber_init_stack_watermark(ev_fiber_t fiber);
   size_t ev_fiber_stack_usage(ev_fiber_t fiber);
   void ev_fiber_report_stack_usage(ev_fiber_t fiber);
   ```

8. Stack watermarking (POSIX-only):
   - Fill stack with 0xCC pattern at creation
   - Measure high water mark by counting pattern bytes
   - Report stack usage for optimization

9. Create `include/ev_fiber.h` with unified API (Windows Fibers-compatible):
   - Opaque handle type `ev_fiber_t`
   - Entry function signature matching Windows: `void func(void *arg)`
   - Core operations: init, create, switch, current, data, destroy
   - POSIX extensions: previous, stack_usage (clearly marked)
   - Default stack size: 24KB (not 256KB)

10. Implementation in `src/fcontext_wrapper.c`:
    ```c
    struct ev_fiber_s {
        fcontext_t fctx;
        void *stack_base;
        size_t stack_size;
        void *user_data;
        ev_fiber_func_t user_func;
        ev_fiber_t previous;
        bool watermark_enabled;
    };

    #define STACK_FILL_PATTERN 0xCC
    #define DEFAULT_STACK_SIZE (24 * 1024)  // 24KB
    #define GUARD_PAGE_SIZE 4096
    ```

**Acceptance Criteria:**
- Header is pure C11 (no C++)
- Unified API mirrors Windows Fibers (ev_fiber_*)
- Low-level fcontext API also available
- Stack size defaults to 24KB
- Stack watermarking implemented (0xCC pattern)
- License headers preserved

---

### Task 3: Create Build System

**Output:** `Makefile` and `CMakeLists.txt` that detect platform and build tests

**Makefile Requirements:**

```makefile
# Detect platform and architecture
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# Select assembly files based on platform
ifeq ($(UNAME_S),Darwin)
    ifeq ($(UNAME_M),arm64)
        ASM_DIR = src/asm/arm64
        ASM_FILES = make_arm64_aapcs_macho_gas.S jump_arm64_aapcs_macho_gas.S ontop_arm64_aapcs_macho_gas.S
    else ifeq ($(UNAME_M),x86_64)
        ASM_DIR = src/asm/x86_64
        ASM_FILES = make_x86_64_sysv_macho_gas.S jump_x86_64_sysv_macho_gas.S ontop_x86_64_sysv_macho_gas.S
    endif
else ifeq ($(UNAME_S),Linux)
    ifeq ($(UNAME_M),x86_64)
        ASM_DIR = src/asm/x86_64
        ASM_FILES = make_x86_64_sysv_elf_gas.S jump_x86_64_sysv_elf_gas.S ontop_x86_64_sysv_elf_gas.S
    else ifeq ($(UNAME_M),aarch64)
        ASM_DIR = src/asm/arm64
        ASM_FILES = make_arm64_aapcs_elf_gas.S jump_arm64_aapcs_elf_gas.S ontop_arm64_aapcs_elf_gas.S
    else ifeq ($(UNAME_M),riscv64)
        ASM_DIR = src/asm/riscv64
        ASM_FILES = make_riscv64_sysv_elf_gas.S jump_riscv64_sysv_elf_gas.S ontop_riscv64_sysv_elf_gas.S
    # Add more architectures...
    endif
endif

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Iinclude -g -O2
LDFLAGS =

ASM_OBJS = $(addprefix $(ASM_DIR)/,$(ASM_FILES:.S=.o))

TESTS = test_fcontext_basic test_fiber_basic test_fiber_data test_fiber_asymmetric \
        test_fiber_stack test_fiber_stress test_fiber_threads

all: $(TESTS)

$(ASM_DIR)/%.o: $(ASM_DIR)/%.S
	$(CC) -c $< -o $@

test_%: tests/test_%.c $(ASM_OBJS) src/fcontext_wrapper.c src/ev_fiber.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test: $(TESTS)
	@for test in $(TESTS); do \
		echo "Running $$test..."; \
		./$$test || exit 1; \
	done

clean:
	rm -f $(TESTS) $(ASM_DIR)/*.o

.PHONY: all test clean
```

**CMakeLists.txt Requirements:**

```cmake
cmake_minimum_required(VERSION 3.10)
project(fcontext_tests C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable assembly
enable_language(ASM)

# Detect platform and architecture
if(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(ASM_DIR "src/asm/arm64")
        set(ASM_FILES
            ${ASM_DIR}/make_arm64_aapcs_macho_gas.S
            ${ASM_DIR}/jump_arm64_aapcs_macho_gas.S
            ${ASM_DIR}/ontop_arm64_aapcs_macho_gas.S
        )
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
        set(ASM_DIR "src/asm/x86_64")
        set(ASM_FILES
            ${ASM_DIR}/make_x86_64_sysv_macho_gas.S
            ${ASM_DIR}/jump_x86_64_sysv_macho_gas.S
            ${ASM_DIR}/ontop_x86_64_sysv_macho_gas.S
        )
    endif()
elseif(UNIX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
        set(ASM_DIR "src/asm/x86_64")
        set(ASM_FILES
            ${ASM_DIR}/make_x86_64_sysv_elf_gas.S
            ${ASM_DIR}/jump_x86_64_sysv_elf_gas.S
            ${ASM_DIR}/ontop_x86_64_sysv_elf_gas.S
        )
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(ASM_DIR "src/asm/arm64")
        set(ASM_FILES
            ${ASM_DIR}/make_arm64_aapcs_elf_gas.S
            ${ASM_DIR}/jump_arm64_aapcs_elf_gas.S
            ${ASM_DIR}/ontop_arm64_aapcs_elf_gas.S
        )
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
        set(ASM_DIR "src/asm/riscv64")
        set(ASM_FILES
            ${ASM_DIR}/make_riscv64_sysv_elf_gas.S
            ${ASM_DIR}/jump_riscv64_sysv_elf_gas.S
            ${ASM_DIR}/ontop_riscv64_sysv_elf_gas.S
        )
    endif()
endif()

include_directories(include)

# Create test executables
set(TESTS
    test_fcontext_basic
    test_fiber_basic
    test_fiber_data
    test_fiber_asymmetric
    test_fiber_stack
    test_fiber_stress
    test_fiber_threads
)

foreach(test ${TESTS})
    add_executable(${test} tests/${test}.c src/fcontext_wrapper.c src/ev_fiber.c ${ASM_FILES})
endforeach()

# Enable testing
enable_testing()
foreach(test ${TESTS})
    add_test(NAME ${test} COMMAND ${test})
endforeach()
```

**Acceptance Criteria:**
- Both build systems work on macOS (Intel + ARM) and Linux (x86_64 + ARM64)
- Automatically select correct assembly files
- Build all test executables
- `make test` or `ctest` runs all tests

---

### Task 4: Write Test Code

Create seven test programs demonstrating both low-level fcontext and high-level unified fiber API:

#### Test 1: test_fcontext_basic.c

**Purpose:** Test low-level fcontext API (raw assembly interface)

**Requirements:**
```c
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "fcontext.h"

static int global_phase = 0;

void fiber_function(fcontext_transfer_t t) {
    printf("Entered fiber (low-level fcontext)\n");
    assert(global_phase == 0);
    global_phase = 1;

    // Yield back to main
    t = jump_fcontext(t.prev_context, (void *)42);

    // Resumed
    printf("Fiber resumed\n");
    assert(global_phase == 2);
    global_phase = 3;
}

int main(void) {
    printf("=== Low-Level fcontext Test ===\n");

    // Allocate stack (24KB)
    size_t stack_size = 24 * 1024;
    void *stack = malloc(stack_size);
    assert(stack != NULL);

    // Create context
    fcontext_t fiber_ctx = make_fcontext((char *)stack + stack_size, stack_size, fiber_function);
    assert(fiber_ctx != NULL);

    // First switch
    fcontext_transfer_t t = jump_fcontext(fiber_ctx, NULL);
    assert(global_phase == 1);
    assert(t.data == (void *)42);

    // Resume fiber
    global_phase = 2;
    t = jump_fcontext(t.prev_context, NULL);
    assert(global_phase == 3);

    free(stack);
    printf("PASS\n");
    return 0;
}
```

**Acceptance:** Tests raw fcontext API, prints "PASS"

#### Test 2: test_fiber_basic.c

**Purpose:** Test high-level unified fiber API (Windows-compatible)

**Requirements:**
```c
#include <stdio.h>
#include <assert.h>
#include "ev_fiber.h"

static int phase = 0;
static ev_fiber_t main_fiber;

void fiber_function(void *arg) {
    int id = (int)(intptr_t)arg;
    printf("Fiber %d started\n", id);

    assert(phase == 0);
    phase = 1;

    // Yield back to main
    ev_fiber_switch(main_fiber);

    // Resumed
    printf("Fiber %d resumed\n", id);
    assert(phase == 2);
    phase = 3;
}

int main(void) {
    printf("=== Unified Fiber API Test ===\n");

    // Initialize fiber system
    assert(ev_fiber_init_thread() == 0);
    main_fiber = ev_fiber_current();

    // Create fiber (24KB stack)
    ev_fiber_t fiber = ev_fiber_create(24 * 1024, fiber_function, (void *)1);
    assert(fiber != NULL);

    // Switch to fiber
    ev_fiber_switch(fiber);
    assert(phase == 1);

    // Resume fiber
    phase = 2;
    ev_fiber_switch(fiber);
    assert(phase == 3);

    ev_fiber_destroy(fiber);
    printf("PASS\n");
    return 0;
}
```

**Acceptance:** Tests unified API matching Windows Fibers, prints "PASS"

#### Test 3: test_fiber_data.c

**Purpose:** Test data passing with unified API

**Requirements:**
- Pass user data to fiber via ev_fiber_create
- Access data via ev_fiber_data() inside fiber
- Test with different data types
- Verify data integrity

**Pseudocode:**
```c
struct connection_data {
    int conn_id;
    char name[32];
};

void connection_fiber(void *arg) {
    struct connection_data *data = ev_fiber_data();
    // Verify data is correct
    printf("Connection %d: %s\n", data->conn_id, data->name);
}
```

**Acceptance:** Data passed correctly, accessed via ev_fiber_data()

#### Test 4: test_fiber_asymmetric.c

**Purpose:** Test asymmetric pattern with scheduler (fibers yield to scheduler only)

**Requirements:**
- Implement simple round-robin scheduler
- Multiple fibers, each yields to scheduler (never to each other)
- Scheduler manages 3+ fibers
- Fibers identified by ev_fiber_current()

**Pseudocode:**
```c
ev_fiber_t scheduler_fiber;
ev_fiber_t fibers[3];

void fiber_func(void *arg) {
    int id = (int)(intptr_t)arg;

    for (int i = 0; i < 5; i++) {
        printf("Fiber %d: iteration %d\n", id, i);
        ev_fiber_switch(scheduler_fiber);  // Always yield to scheduler
    }
}

int main() {
    ev_fiber_init_thread();
    scheduler_fiber = ev_fiber_current();

    // Create 3 fibers
    for (int i = 0; i < 3; i++) {
        fibers[i] = ev_fiber_create(24 * 1024, fiber_func, (void *)(intptr_t)i);
    }

    // Round-robin scheduler
    bool all_done = false;
    while (!all_done) {
        for (int i = 0; i < 3; i++) {
            ev_fiber_switch(fibers[i]);
        }
    }
}
```

**Acceptance:** Fibers interleave output, demonstrate asymmetric pattern

#### Test 5: test_fiber_stack.c

**Purpose:** Test stack watermarking (POSIX-only feature)

**Requirements:**
- Create fiber with 24KB stack
- Initialize stack watermark (fill with 0xCC)
- Run fiber with varying stack usage
- Measure stack usage with ev_fiber_stack_usage()
- Report usage with ev_fiber_report_stack_usage()
- Test with shallow stack (<1KB) and deeper stack (~10KB)

**Pseudocode:**
```c
void shallow_fiber(void *arg) {
    char buf[512];  // Use only 512 bytes
    memset(buf, 1, sizeof(buf));
}

void deep_fiber(void *arg) {
    char buf[10000];  // Use ~10KB
    memset(buf, 1, sizeof(buf));
}

int main() {
    ev_fiber_init_thread();

    ev_fiber_t f1 = ev_fiber_create(24 * 1024, shallow_fiber, NULL);
    ev_fiber_init_stack_watermark(f1);
    ev_fiber_switch(f1);
    ev_fiber_report_stack_usage(f1);  // Should show ~1KB used

    ev_fiber_t f2 = ev_fiber_create(24 * 1024, deep_fiber, NULL);
    ev_fiber_init_stack_watermark(f2);
    ev_fiber_switch(f2);
    ev_fiber_report_stack_usage(f2);  // Should show ~10KB used
}
```

**Acceptance:** Correctly measures stack usage, reports percentages

#### Test 6: test_fiber_stress.c

**Purpose:** Stress test with many fibers and switches

**Requirements:**
- Create 100 fibers (24KB each = 2.4MB total)
- Each fiber yields 100 times
- Total: 10,000 context switches
- Measure time
- Verify no memory corruption
- Test stack watermarking on all fibers

**Acceptance:** All switches succeed, completes quickly, stack usage reasonable

#### Test 7: test_fiber_threads.c

**Purpose:** Simulate green threads with unified API (thread-like programming)

**Requirements:**
- Use unified fiber API to simulate thread-like behavior
- Multiple concurrent "threads" doing work
- Demonstrate Windows Fibers-compatible code style
- Each fiber represents a connection handler

**Pseudocode:**
```c
ev_fiber_t scheduler;

void connection_handler(void *arg) {
    int conn_id = (int)(intptr_t)arg;

    for (int i = 0; i < 3; i++) {
        printf("Connection %d: processing request %d\n", conn_id, i);

        // Simulate blocking I/O
        ev_fiber_switch(scheduler);
    }

    printf("Connection %d: done\n", conn_id);
}

int main() {
    ev_fiber_init_thread();
    scheduler = ev_fiber_current();

    // Create multiple connection fibers
    ev_fiber_t conns[5];
    for (int i = 0; i < 5; i++) {
        conns[i] = ev_fiber_create(24 * 1024, connection_handler, (void *)(intptr_t)i);
    }

    // Simple scheduler
    // ... round-robin between connections
}
```

**Acceptance:** Multiple "threads" interleave, demonstrates portable thread-like code

---

### Task 5: Create Documentation

Create two documentation files:

#### README.md

**Content:**

1. **Project Overview**
   - Purpose: Portable fiber/coroutine implementation for libplctag
   - Two API layers: low-level fcontext, high-level unified fibers
   - Based on Boost.Context assembly

2. **API Documentation**
   - **Low-Level API (fcontext.h)**: Direct assembly interface
     - When to use
     - Example code
   - **Unified API (ev_fiber.h)**: Windows Fibers-compatible
     - When to use
     - Windows compatibility notes
     - Example code
     - Stack watermarking (POSIX only)

3. **Supported Architectures and Platforms**
   - List all 13 architectures
   - OS support: Linux, macOS, BSD (Windows via Windows Fibers separately)
   - 24KB default stack size

4. **Build Instructions**
   - Make: `make && make test`
   - CMake: `mkdir build && cd build && cmake .. && make && ctest`

5. **Running Tests**
   - test_fcontext_basic: Low-level API
   - test_fiber_*: Unified API demos
   - test_fiber_stack: Stack watermarking

6. **Example Code**
   - Simple fiber creation and switching
   - Asymmetric scheduler pattern
   - Stack usage measurement

7. **License and Attribution**
   - Boost Software License 1.0
   - References to Boost.Context and DaoWen/fcontext

#### LICENSE

**Content:**
- Copy Boost Software License 1.0 from Boost.Context
- Add attribution to Boost.Context and DaoWen/fcontext

**Acceptance Criteria:**
- README clearly explains both API layers and when to use each
- Code examples for both low-level and unified APIs
- Windows compatibility notes clearly stated
- Stack watermarking explained with examples

---

## Validation Checklist

Before marking complete, verify:

- [ ] All assembly files copied from Boost.Context for all 13 architectures
- [ ] Assembly files organized in arch-specific subdirectories
- [ ] fcontext.h is pure C11 (no C++), low-level API
- [ ] ev_fiber.h provides Windows Fibers-compatible unified API
- [ ] All code compiles with `-std=c11 -Wall -Wextra` with no warnings
- [ ] Makefile works on macOS Intel, macOS ARM, Linux x86-64
- [ ] CMakeLists.txt works on same platforms
- [ ] All 7 tests pass on all platforms
- [ ] test_fcontext_basic demonstrates low-level API
- [ ] test_fiber_* tests demonstrate unified API (Windows-compatible)
- [ ] test_fiber_stack demonstrates stack watermarking (24KB stacks)
- [ ] Stack watermarking correctly measures usage (0xCC pattern)
- [ ] Code is independent of main libplctag project
- [ ] README.md documents both APIs and their use cases
- [ ] License file present with proper attribution
- [ ] No hardcoded paths (build system is portable)

---

## Expected File Sizes

As a sanity check, expected line counts:

| File | Lines |
|------|-------|
| include/fcontext.h | 100-150 |
| include/ev_fiber.h | 100-150 |
| src/fcontext_wrapper.c | 50-100 |
| src/ev_fiber.c | 300-400 |
| src/asm/README.md | 50-100 |
| Each assembly file | 100-200 |
| Each test file | 150-250 |
| Makefile | 50-80 |
| CMakeLists.txt | 60-100 |
| README.md | 150-300 |

Total assembly files: ~78 files (13 architectures × 3 files × ~2 ABIs)
Total lines for assembly: ~15,000 lines
Total C code: ~1,200-1,800 lines (including unified API)

---

## Common Pitfalls to Avoid

1. **Don't modify assembly files** - Copy them exactly as-is from Boost.Context
2. **Preserve all license headers** - Every file must have Boost license
3. **Don't use C99 VLAs** - Use C11 features only (static assertions, stdatomic if needed)
4. **Test on actual hardware** - Don't assume cross-compilation works without testing
5. **Watch for alignment** - Stack pointer must be properly aligned (16-byte on x86-64, ARM64)
6. **Stack grows down** - Pass stack_top to make_fcontext, not stack_base
7. **No C++** - Everything must be pure C11, no exceptions, no RTTI, no templates
8. **Use 24KB stacks by default** - Not 8KB or 256KB (tested and verified size)
9. **Stack watermarking is POSIX-only** - Document that it won't work on Windows
10. **Unified API matches Windows Fibers** - Keep semantics identical for portability
11. **Call ev_fiber_init_thread() first** - Required before creating fibers (like ConvertThreadToFiber on Windows)

---

## Build and Test Commands

```bash
# Using Make
cd src/tests/fcontext
make clean
make
make test

# Using CMake
cd src/tests/fcontext
mkdir build
cd build
cmake ..
cmake --build .
ctest

# Run individual tests
./test_fcontext_basic        # Low-level fcontext API
./test_fiber_basic           # Unified fiber API
./test_fiber_asymmetric      # Scheduler pattern
./test_fiber_stack           # Stack watermarking (24KB stacks)
./test_fiber_stress          # 100 fibers, 10,000 switches
./test_fiber_threads         # Green thread simulation
```

---

## Success Criteria

Implementation is complete when:

1. All assembly files extracted and organized (78 files, 13 architectures)
2. All code is pure C11 (no C++)
3. Both APIs implemented:
   - Low-level fcontext API (fcontext.h)
   - High-level unified API (ev_fiber.h) - Windows Fibers compatible
4. Stack watermarking implemented with 24KB default stacks
5. Both build systems work (Make and CMake)
6. All 7 tests pass on macOS (Intel and ARM) and Linux (x86-64)
7. Tests demonstrate:
   - Low-level fcontext usage
   - Unified API matching Windows Fibers style
   - Asymmetric coroutine pattern (scheduler-based)
   - Stack measurement and reporting
8. Documentation explains both APIs and when to use each
9. Code is standalone (no dependencies on main project)

---

## Questions to Resolve

None - plan is complete and ready for implementation.

---

## Timeline Estimate

For Claude Haiku:
- Task 1 (Assembly extraction): 30 minutes
- Task 2 (Wrapper adaptation + unified API): 45 minutes
- Task 3 (Build system): 15 minutes
- Task 4 (7 tests including stack watermarking): 60 minutes
- Task 5 (Documentation): 20 minutes

**Total: ~2.5-3 hours**

---

## Notes for Implementer

- Use `/tmp` for cloning repositories
- Boost.Context is at: https://github.com/boostorg/context.git (branch: develop)
- DaoWen/fcontext is at: https://github.com/DaoWen/fcontext
- All work happens in `src/tests/fcontext/` directory
- Do not modify anything outside this directory
- Code must compile and run on the implementer's machine
- If a test fails, debug before moving to next task

**Key implementation points:**

1. **Two API layers required:**
   - Low-level: fcontext.h (direct assembly wrapper)
   - High-level: ev_fiber.h (Windows Fibers-compatible)

2. **Stack size: 24KB default**
   - Not 8KB (too small for real use)
   - Not 256KB (wastes memory)
   - 24KB = 24 * 1024 bytes

3. **Stack watermarking pattern: 0xCC**
   - Fill stack at creation
   - Measure by counting unused pattern bytes
   - POSIX-only feature (document this clearly)

4. **Unified API must match Windows Fibers semantics:**
   - Entry function: `void func(void *arg)` (not fcontext_transfer_t)
   - No return values from switch operations
   - Thread must call init before creating fibers
   - Opaque handle type

5. **Test coverage:**
   - At least one test for low-level API
   - Most tests use unified API (it's the recommended interface)
   - Stack measurement test required
   - Stress test with 100 fibers minimum
