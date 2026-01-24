set(ASM_FILES
    src/asm/x86_64/make_x86_64_ms_pe_gas.S
    src/asm/x86_64/jump_x86_64_ms_pe_gas.S
    src/asm/x86_64/ontop_x86_64_ms_pe_gas.S
)
enable_language(ASM)
add_compile_options(-Wall -Wextra -Werror -g -O2)