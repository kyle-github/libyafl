set(ASM_FILES
    src/asm/mips/make_mips32_o32_elf_gas.S
    src/asm/mips/jump_mips32_o32_elf_gas.S
)
enable_language(ASM)
add_compile_options(-Wall -Wextra -Werror -g -O2)