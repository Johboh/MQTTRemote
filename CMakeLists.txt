FILE(GLOB_RECURSE lib_sources "./espidf/*.*")

idf_component_register(COMPONENT_NAME "MQTTRemote"
                        SRCS ${lib_sources}
                        INCLUDE_DIRS "./espidf/"
                        REQUIRES mqtt esp_event)

if(IDF_VERSION_MAJOR LESS 5) # 5+ compiles with c++23.
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
endif()