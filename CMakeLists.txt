FILE(GLOB_RECURSE lib_sources "./espidf/*.*")

if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    set(MQTT_COMPONENT "") # Moved to component manager
else()
    set(MQTT_COMPONENT "mqtt")
endif()

idf_component_register(COMPONENT_NAME "MQTTRemote"
                        SRCS ${lib_sources}
                        INCLUDE_DIRS "./espidf/"
                        REQUIRES ${MQTT_COMPONENT} esp_event)


if(IDF_VERSION_MAJOR LESS 5) # 5+ compiles with c++23, 6+ compiles with c++26.
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
endif()