cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "0.1.0")

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(QUELLOG_FONT_PARTITION_IMAGE "${CMAKE_BINARY_DIR}/font_partition.bin")
add_custom_command(
    OUTPUT "${QUELLOG_FONT_PARTITION_IMAGE}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/assets/font_partition.bin"
            "${QUELLOG_FONT_PARTITION_IMAGE}"
    DEPENDS
        "${CMAKE_SOURCE_DIR}/assets/font_partition.bin"
    VERBATIM
)
add_custom_target(quellog_font_partition ALL DEPENDS "${QUELLOG_FONT_PARTITION_IMAGE}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(quellog_eink_firmware)
