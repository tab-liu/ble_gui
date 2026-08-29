# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Set the project name
project(IoT_ESP32S3_Module)

# 自动生成编译日期头文件
add_custom_target(generate_build_date_h ALL
    BYPRODUCTS ${CMAKE_BINARY_DIR}/build_date.h
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/gen_build_date.py ${CMAKE_BINARY_DIR}/build_date.h
    COMMENT "Generating build_date.h with current time"
    VERBATIM
)

# 绑定到ESP-IDF的app目标，在bin文件生成后执行
add_custom_command(
    TARGET app POST_BUILD

    # 计算CRC
    COMMAND ${CMAKE_COMMAND} -E echo "Starting CRC32 processing..."
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/add_crc32.py ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMAND ${CMAKE_COMMAND} -E echo "CRC32 processing completed."

    # 自动合并文件
    COMMAND ${CMAKE_COMMAND} -E echo "Starting merge-bin..."
    COMMAND ${PYTHON} -m esptool
        --chip ${IDF_TARGET}
        merge_bin
        --output ${CMAKE_BINARY_DIR}/${PROJECT_NAME}_merged.bin
        @${CMAKE_BINARY_DIR}/flash_args
    COMMAND ${CMAKE_COMMAND} -E echo "Merged binary: ${CMAKE_BINARY_DIR}/${PROJECT_NAME}_merged.bin"

    COMMENT "Processing binary file with CRC32 and merging after app build"
    VERBATIM
)
