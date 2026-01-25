set(ASM_FILES
    src/asm/sparc64/make_sparc64_sysv_elf_gas.S
    src/asm/sparc64/jump_sparc64_sysv_elf_gas.S
)
enable_language(ASM)
add_compile_options(-Wall -Wextra -Werror -g -O2)