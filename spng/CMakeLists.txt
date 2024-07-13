include(WoofSettings)

add_library(spng STATIC spng.c spng.h)

target_woof_settings(spng)

target_compile_definitions(spng PRIVATE SPNG_USE_MINIZ INTERFACE SPNG_STATIC)

target_include_directories(spng INTERFACE ".")

target_link_libraries(spng miniz)
