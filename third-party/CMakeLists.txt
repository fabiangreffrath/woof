include(ProjectSettings)

add_library(base64 STATIC base64/base64.c)
target_project_settings(base64)
target_include_directories(base64 INTERFACE base64)

add_library(md5 STATIC md5/md5.c)
target_project_settings(md5)
target_include_directories(md5 INTERFACE md5)

file(GLOB SRC_OPL opl/*)
add_library(opl STATIC ${SRC_OPL})
target_project_settings(opl)
target_include_directories(opl INTERFACE opl
                           PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../" "../src/")

file(GLOB SRC_SETUP setup/*)
add_library(setup STATIC ${SRC_SETUP})
target_project_settings(setup)
target_include_directories(setup INTERFACE setup
                           PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../" "../src/")
target_link_libraries(setup textscreen SDL3::SDL3)

add_library(sha1 STATIC sha1/sha1.c)
target_project_settings(sha1)
target_include_directories(sha1 INTERFACE sha1)

file(GLOB SRC_TXT textscreen/*)
add_library(textscreen STATIC ${SRC_TXT})
target_project_settings(textscreen)
target_include_directories(textscreen INTERFACE textscreen
                           PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../" "../src/")
target_link_libraries(textscreen SDL3::SDL3)

if(ENABLE_FONTGEN)
    add_subdirectory(textscreen/fonts)
    add_dependencies(textscreen textscreen-font)
endif()

if(NOT libebur128_FOUND)
    add_subdirectory(libebur128)
    target_include_directories(ebur128 INTERFACE libebur128)
    add_library(libebur128::libebur128 ALIAS ebur128)
endif()

if(NOT MINIMP3_INCLUDE_DIRS)
    set(MINIMP3_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}" PARENT_SCOPE)
endif()

if(NOT miniz_FOUND)
    add_library(miniz STATIC miniz/miniz.c)
    target_project_settings(miniz)
    target_compile_definitions(miniz PRIVATE MINIZ_NO_TIME)
    target_include_directories(miniz INTERFACE miniz)
    add_library(miniz::miniz ALIAS miniz)
endif()

if(NOT pffft_FOUND)
    add_library(pffft STATIC pffft/pffft.c)
    target_project_settings(pffft)
    target_include_directories(pffft INTERFACE .)
    add_library(pffft::pffft ALIAS pffft)
endif()

if(NOT spng_FOUND)
    add_library(spng STATIC spng/spng.c)
    target_project_settings(spng)
    target_compile_definitions(spng PRIVATE SPNG_USE_MINIZ INTERFACE SPNG_STATIC)
    target_include_directories(spng INTERFACE spng)
    target_link_libraries(spng miniz::miniz)
    add_library(spng::spng ALIAS spng)
endif()

if(NOT yyjson_FOUND)
    add_library(yyjson STATIC yyjson/yyjson.c)
    target_project_settings(yyjson)
    target_compile_definitions(
        yyjson PRIVATE YYJSON_DISABLE_UTILS=1
    )
    target_include_directories(yyjson INTERFACE yyjson)
    add_library(yyjson::yyjson ALIAS yyjson)
endif()
