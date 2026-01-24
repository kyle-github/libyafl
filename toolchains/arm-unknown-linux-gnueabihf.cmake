set(ASM_FILES
    src/asm/arm/make_arm_aapcs_elf_gas.S
    src/asm/arm/jump_arm_aapcs_elf_gas.S
    src/asm/arm/ontop_arm_aapcs_elf_gas.S
)
enable_language(ASM)
add_compile_options(-Wall -Wextra -Werror -g -O2)