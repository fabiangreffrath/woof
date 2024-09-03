include(WoofSettings)

add_library(miniz STATIC miniz/miniz.c)
target_compile_definitions(miniz PRIVATE MINIZ_NO_TIME)
target_include_directories(miniz INTERFACE miniz)

add_library(spng STATIC spng/spng.c)
target_compile_definitions(spng PRIVATE SPNG_USE_MINIZ INTERFACE SPNG_STATIC)
target_include_directories(spng INTERFACE spng)
target_link_libraries(spng miniz)
