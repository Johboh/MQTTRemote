FILE(GLOB_RECURSE lib_sources "./espidf/*.*")

idf_component_register(COMPONENT_NAME "MQTTRemote"
                        SRCS ${lib_sources}
                        INCLUDE_DIRS "./includes/" "./espidf/"
                        REQUIRES mqtt esp_event)
