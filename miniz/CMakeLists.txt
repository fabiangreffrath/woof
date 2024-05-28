include(WoofSettings)

add_library(miniz STATIC miniz.c miniz.h)

target_woof_settings(miniz)

target_compile_definitions(miniz PRIVATE MINIZ_NO_TIME)

target_include_directories(miniz INTERFACE ".")

target_link_libraries(miniz)
