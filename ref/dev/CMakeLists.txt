# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# set(EXTRA_COMPONENT_DIRS "./managed_components/espressif__mesh_lite")
set(EXTRA_COMPONENT_DIRS "./main/esp_modem")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(PROJECT_NAME "PLP0xx-APx00")

project(${PROJECT_NAME})

# 自动生成编译日期头文件
add_custom_target(generate_build_date_h ALL
    BYPRODUCTS ${CMAKE_BINARY_DIR}/build_date.h
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/gen_build_date.py ${CMAKE_BINARY_DIR}/build_date.h
    COMMENT "Generating build_date.h with current time"
    VERBATIM
)

# 绑定到ESP-IDF的app目标，在bin文件生成后执行
# 说明：地址严格对齐本工程 build/flash_args（ESP32-S3，flash 8MB/80m/dio）
#   0x0      bootloader/bootloader.bin
#   0xA000   partition_table/partition-table.bin
#   0xB000   ota_data_initial.bin
#   0xD000   phy_multiple_init_data.bin
#   0x20000  PLP0xx-APx00.bin（即 ${PROJECT_NAME}.bin）
add_custom_command(
    TARGET app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Starting CRC32 processing..."
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/add_crc32.py ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMAND ${CMAKE_COMMAND} -E echo "CRC32 processing completed."
    COMMAND ${CMAKE_COMMAND} -E echo "Starting merged-bin packaging..."
    COMMAND ${PYTHON} $ENV{IDF_PATH}/components/esptool_py/esptool/esptool.py
            --chip esp32s3 merge_bin
            -o ${CMAKE_BINARY_DIR}/${PROJECT_NAME}-merged.bin
            --flash_mode dio --flash_size 8MB --flash_freq 80m
            0x0      ${CMAKE_BINARY_DIR}/bootloader/bootloader.bin
            0xA000   ${CMAKE_BINARY_DIR}/partition_table/partition-table.bin
            0xB000   ${CMAKE_BINARY_DIR}/ota_data_initial.bin
            0xD000   ${CMAKE_BINARY_DIR}/phy_multiple_init_data.bin
            0x20000  ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMAND ${CMAKE_COMMAND} -E echo "Merged-bin packaging completed: ${CMAKE_BINARY_DIR}/${PROJECT_NAME}-merged.bin"
    COMMENT "Processing binary file with CRC32 and generating merged bin after app build"
    VERBATIM
)