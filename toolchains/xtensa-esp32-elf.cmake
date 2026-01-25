# DISABLED - Xtensa/ESP32 support is not maintained
# This toolchain file is kept for historical reference only

set(ASM_FILES
    src/asm/xtensa/make_xtensa_call0_sysv_elf_gas.S
    src/asm/xtensa/jump_xtensa_call0_sysv_elf_gas.S
)
enable_language(ASM)
add_compile_options(-Wall -Wextra -Werror -g -O2)