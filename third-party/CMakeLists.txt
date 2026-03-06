include(WoofSettings)

add_library(md5 STATIC md5/md5.c)
target_woof_settings(md5)
target_include_directories(md5 INTERFACE md5)

add_library(sha1 STATIC sha1/sha1.c)
target_woof_settings(sha1)
target_include_directories(sha1 INTERFACE sha1)

if(NOT MINIMP3_INCLUDE_DIRS)
    set(MINIMP3_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}" PARENT_SCOPE)
endif()

if(NOT miniz_FOUND)
    add_library(miniz STATIC miniz/miniz.c)
    target_woof_settings(miniz)
    target_compile_definitions(miniz PRIVATE MINIZ_NO_TIME)
    target_include_directories(miniz INTERFACE miniz)
    add_library(miniz::miniz ALIAS miniz)
endif()

if(NOT pffft_FOUND)
    add_library(pffft STATIC pffft/pffft.c)
    target_woof_settings(pffft)
    target_include_directories(pffft INTERFACE .)
    add_library(pffft::pffft ALIAS pffft)
endif()

add_library(spng STATIC spng/spng.c)
target_woof_settings(spng)
target_compile_definitions(spng PRIVATE SPNG_USE_MINIZ INTERFACE SPNG_STATIC)
target_include_directories(spng INTERFACE spng)
target_link_libraries(spng miniz::miniz)

if(NOT yyjson_FOUND)
    add_library(yyjson STATIC yyjson/yyjson.c)
    target_woof_settings(yyjson)
    target_compile_definitions(
        yyjson PRIVATE YYJSON_DISABLE_WRITER=1
                       YYJSON_DISABLE_UTILS=1
    )
    target_include_directories(yyjson INTERFACE yyjson)
    add_library(yyjson::yyjson ALIAS yyjson)
endif()
