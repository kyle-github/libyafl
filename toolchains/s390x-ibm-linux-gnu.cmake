set(ASM_FILES
    src/asm/s390x/make_s390x_sysv_elf_gas.S
    src/asm/s390x/jump_s390x_sysv_elf_gas.S
    src/asm/s390x/ontop_s390x_sysv_elf_gas.S
)
enable_language(ASM)
add_compile_options(-Wall -Wextra -Werror -g -O2)