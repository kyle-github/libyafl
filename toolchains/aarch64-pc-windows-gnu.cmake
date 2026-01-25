set(ASM_FILES
    src/asm/arm64/make_arm64_aapcs_pe_armclang.S
    src/asm/arm64/jump_arm64_aapcs_pe_armclang.S
)
enable_language(ASM)
add_compile_options(-Wall -Wextra -Werror -g -O2)