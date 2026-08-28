
file(GLOB_RECURSE SOURCES   *.c
                            ./uart/*.c
                            ./do_di_ctrl/*.c
                            ./comm_cellular/*.c
                            ./ethernet/*.c
                            ./iot_auth_host/*.c
                            ./bluetooth/*.c
                            ./iot_can/*.c
                            ./iot_can_data/*.c
                            ./iot_can_ota/*.c
                            ./iot_crc/*.c
                            ./iot_modbus/*.c
                            ./wifi/*.c
                            ./iot_xmodem/*.c
							./low_level_param/*.c
							./period_task/*.c
							./filesystem/*.c
							./iic_bus/*.c
							./ext_rtc/*.c
							./pppos_client/*.c
							./server2_internet/*.c
							./network_security/*.c
							./json_cmd/*.c
							./image_handle/*.c
							./http_client_ota/*.c
							./universal_data_transmission/*.c
							./udp_multicast/*.c
							./device_discovery/*.c
							./ems/*.c
							./iot_control_strategy/*.c
							./mesh_network/
							./webserver/*.c
							./tou_ctrl/*.c
							./wlcc/*.c
							./dev_info/*.c
							./utilities/*.c
							./low_power/*.c
							./smart_ctrl/*.c
                            )


# GLOB 在 cmake 配置时缓存，新增源文件后需显式补充或重新配置
list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/wifi/cloud_diag.c")
list(REMOVE_DUPLICATES SOURCES)

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS
                    "."
                    ./uart/
                    ./do_di_ctrl/
                    ./comm_cellular/
					./ethernet/
					./iot_auth_host/
					./bluetooth/
					./iot_can/
					./iot_can_data/
					./iot_can_ota/
					./iot_crc/
					./iot_modbus/
					./wifi/
					./iot_xmodem/
					./low_level_param/
					./period_task/
					./filesystem/
					./iic_bus/
					./ext_rtc/
					./pppos_client/
					./server2_internet/
					./network_security/
					./json_cmd/
					./image_handle/
					./http_client_ota/
					./universal_data_transmission/
					./udp_multicast/
					./device_discovery/
					./ems/
					./iot_control_strategy/
					./mesh_network/
					./webserver/
					./tou_ctrl/
					./wlcc/
					./dev_info/
					./utilities/
					./low_power/
					./smart_ctrl/
                    EMBED_FILES ./webserver/webserver.html
                    )

# Suppress noisy "defined but not used" warnings in this component.
target_compile_options(${COMPONENT_LIB} PRIVATE
                    -Wno-unused-function
                    -Wno-unused-variable
                    -Wno-unused-but-set-variable
                    -Wno-unused-const-variable
                    -Wno-unused-parameter
                    )

# set(INSIDE_COMPONENTS_DIRS  ${CMAKE_CURRENT_SOURCE_DIR}/mesh_network/libmesh_network.a)
# add_prebuilt_library(prebuilt ${INSIDE_COMPONENTS_DIRS}
#                 PRIV_REQUIRES esp_event esp_wifi mqtt )
# target_link_libraries(${COMPONENT_LIB} PRIVATE prebuilt)

# Generate build date header file					
target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_BINARY_DIR})	

# 确保生成的 build_date.h 在编译时可用
add_dependencies(${COMPONENT_LIB} generate_build_date_h)

# 保证特定源文件依赖 build_date.h，每次 build_date.h 变动都重新编译
set_source_files_properties(${CMAKE_SOURCE_DIR}/main/general_build_get.c PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/build_date.h)