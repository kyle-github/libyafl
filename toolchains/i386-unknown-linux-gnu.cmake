set(ASM_FILES
    src/asm/i386/make_i386_sysv_elf_gas.S
    src/asm/i386/jump_i386_sysv_elf_gas.S
)
enable_language(ASM)
add_compile_options(-Wall -Wextra -Werror -g -O2 -m32)