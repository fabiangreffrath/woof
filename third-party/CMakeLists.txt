include(WoofSettings)

add_library(base64 STATIC base64/base64.c)
target_woof_third_party_settings(base64)
target_include_directories(base64 SYSTEM INTERFACE base64)

add_library(md5 STATIC md5/md5.c)
target_woof_third_party_settings(md5)
target_include_directories(md5 SYSTEM INTERFACE md5)

add_library(minimp3 INTERFACE)
target_include_directories(minimp3 SYSTEM INTERFACE minimp3)

add_library(sha1 STATIC sha1/sha1.c)
target_woof_third_party_settings(sha1)
target_include_directories(sha1 SYSTEM INTERFACE sha1)

if(NOT libebur128_FOUND)
    add_subdirectory(libebur128)
    target_include_directories(ebur128 SYSTEM INTERFACE libebur128)
    add_library(libebur128::libebur128 ALIAS ebur128)
endif()

if(NOT clap_FOUND)
    add_library(clap INTERFACE)
    target_include_directories(clap SYSTEM INTERFACE .)
endif()

if(NOT miniz_FOUND)
    add_library(miniz STATIC miniz/miniz.c)
    target_woof_third_party_settings(miniz)
    target_compile_definitions(miniz PRIVATE MINIZ_NO_TIME)
    target_include_directories(miniz SYSTEM INTERFACE miniz)
    add_library(miniz::miniz ALIAS miniz)
endif()

if(NOT pffft_FOUND)
    add_library(pffft STATIC pffft/pffft.c)
    target_woof_third_party_settings(pffft)
    target_include_directories(pffft SYSTEM INTERFACE .)
    add_library(pffft::pffft ALIAS pffft)
endif()

if(NOT spng_FOUND)
    add_library(spng STATIC spng/spng.c)
    target_woof_third_party_settings(spng)
    target_compile_definitions(spng PRIVATE SPNG_USE_MINIZ INTERFACE SPNG_STATIC)
    target_include_directories(spng SYSTEM INTERFACE spng)
    target_link_libraries(spng miniz::miniz)
    add_library(spng::spng ALIAS spng)
endif()

if(NOT yyjson_FOUND)
    add_library(yyjson STATIC yyjson/yyjson.c)
    target_woof_third_party_settings(yyjson)
    target_compile_definitions(
        yyjson PRIVATE YYJSON_DISABLE_UTILS=1
    )
    target_include_directories(yyjson SYSTEM INTERFACE yyjson)
    add_library(yyjson::yyjson ALIAS yyjson)
endif()
