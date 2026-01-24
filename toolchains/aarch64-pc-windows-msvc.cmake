set(ASM_FILES
    src/asm/arm64/make_arm64_aapcs_pe_armasm.asm
    src/asm/arm64/jump_arm64_aapcs_pe_armasm.asm
    src/asm/arm64/ontop_arm64_aapcs_pe_armasm.asm
)

find_program(ARMASM64 armasm64.exe REQUIRED)

# Create custom commands for each ARM64 assembly file
set(ARM64_ASM_OBJECTS "")
foreach(asm_file ${ASM_FILES})
    get_filename_component(asm_name ${asm_file} NAME_WE)
    set(obj_file "${CMAKE_CURRENT_BINARY_DIR}/${asm_name}.obj")
    add_custom_command(
        OUTPUT ${obj_file}
        COMMAND ${ARMASM64} -machine ARM64 -o ${obj_file} ${CMAKE_CURRENT_SOURCE_DIR}/${asm_file}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${asm_file}
        COMMENT "Assembling ARM64 ${asm_file}"
        VERBATIM
    )
    list(APPEND ARM64_ASM_OBJECTS ${obj_file})
endforeach()
set(ASM_FILES ${ARM64_ASM_OBJECTS})

add_compile_options($<$<COMPILE_LANGUAGE:C>:/W4>)
add_compile_options($<$<COMPILE_LANGUAGE:C>:/WX>)