set(ASM_FILES
    src/asm/ppc32/make_ppc32_sysv_elf_gas.S
    src/asm/ppc32/jump_ppc32_sysv_elf_gas.S
)
enable_language(ASM)
add_compile_options(-Wall -Wextra -Werror -g -O2)