set(ASM_FILES
    src/asm/riscv64/make_riscv64_sysv_elf_gas.S
    src/asm/riscv64/jump_riscv64_sysv_elf_gas.S
    src/asm/riscv64/ontop_riscv64_sysv_elf_gas.S
)
enable_language(ASM)
add_compile_options(-Wall -Wextra -Werror -g -O2)