set(ASM_FILES
    src/asm/mips64/make_mips64_n64_elf_gas.S
    src/asm/mips64/jump_mips64_n64_elf_gas.S
)
enable_language(ASM)
add_compile_options(-Wall -Wextra -Werror -g -O2)