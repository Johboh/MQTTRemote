FILE(GLOB_RECURSE app_sources *.*)

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."
)

target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
target_add_binary_data(${COMPONENT_TARGET} "isrgrootx1.pem" TEXT)
