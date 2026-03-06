include(WoofSettings)

add_library(netlib STATIC netlib.c netlib.h)
target_woof_settings(netlib)
target_include_directories(netlib INTERFACE ".")

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_link_libraries(netlib ws2_32 iphlpapi)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_link_libraries(netlib wsock32 iphlpapi)
    endif()
endif()
