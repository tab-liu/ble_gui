file(GLOB_RECURSE SOURCES   *.c
                            ./period_task/*.c
                            ./iot_timer/*.c)

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS
                    "."
                    ./period_task/
                    ./iot_timer/)

# Generate build date header file					
target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_BINARY_DIR})	

# 确保生成的 build_date.h 在编译时可用
add_dependencies(${COMPONENT_LIB} generate_build_date_h)

# 保证特定源文件依赖 build_date.h，每次 build_date.h 变动都重新编译
set_source_files_properties(${CMAKE_SOURCE_DIR}/main/general_build_get.c PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/build_date.h)