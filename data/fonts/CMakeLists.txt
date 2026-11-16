find_package(Python3 QUIET COMPONENTS Interpreter)

set(INPUT_PNG "${CMAKE_CURRENT_SOURCE_DIR}/hauge-8x18-v1-6.png")
set(SOURCE_H "${CMAKE_CURRENT_SOURCE_DIR}/normal.h")
set(OUTPUT_H "${CMAKE_CURRENT_BINARY_DIR}/normal.h")

if(ENABLE_FONTGEN AND Python3_EXECUTABLE)
    set(CONVERT_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/convert-font")

    # Generate to build directory
    add_custom_command(
        OUTPUT ${OUTPUT_H}
        COMMAND ${Python3_EXECUTABLE} ${CONVERT_SCRIPT} normal ${INPUT_PNG} ${OUTPUT_H}
        DEPENDS ${INPUT_PNG} ${CONVERT_SCRIPT}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM
        COMMENT "Generating font header: normal.h"
    )
else()
    # Fallback: copy committed header when Python unavailable
    add_custom_command(
        OUTPUT ${OUTPUT_H}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_H} ${OUTPUT_H}
        DEPENDS ${SOURCE_H}
        VERBATIM
        COMMENT "Copying pre-generated font header: normal.h"
    )
endif()

add_custom_target(generate-font DEPENDS ${OUTPUT_H})
set_property(TARGET generate-font PROPERTY EXCLUDE_FROM_ALL TRUE)