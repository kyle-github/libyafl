set(ASM_FILES
    src/asm/ppc64/make_ppc64_sysv_elf_gas.S
    src/asm/ppc64/jump_ppc64_sysv_elf_gas.S
)
enable_language(ASM)
add_compile_options(-Wall -Wextra -Werror -g -O2)