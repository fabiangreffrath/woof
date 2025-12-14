# Add woof settings to a target.

include(CheckCCompilerFlag)
include(CheckLinkerFlag)

# Add compiler option if compiler supports it.
function(_checked_add_compile_option FLAG)
    # Turn flag into suitable internal cache variable.
    string(REGEX REPLACE "-(.*)" "CFLAG_\\1" FLAG_FOUND ${FLAG})
    string(REPLACE "=" "_" FLAG_FOUND "${FLAG_FOUND}")

    check_c_compiler_flag(${FLAG} ${FLAG_FOUND})
    if(${FLAG_FOUND})
        set(COMMON_COMPILE_OPTIONS ${COMMON_COMPILE_OPTIONS} ${FLAG} PARENT_SCOPE)
    endif()
endfunction()

function(_checked_add_link_option FLAG)
    # Turn flag into suitable internal cache variable.
    string(REGEX REPLACE "-(.*)" "LDFLAG_\\1" FLAG_FOUND ${FLAG})
    string(REGEX REPLACE "[,=-]+" "_" FLAG_FOUND "${FLAG_FOUND}")

    check_linker_flag(C ${FLAG} ${FLAG_FOUND})
    if(${FLAG_FOUND})
        set(COMMON_LINK_OPTIONS ${COMMON_LINK_OPTIONS} ${FLAG} PARENT_SCOPE)
    endif()
endfunction()

# Parameters we want to check for on all compilers.
#
# Note that we want to check for these, even on MSVC, because some compilers
# that pretend to be MSVC can take both GCC and MSVC-style parameters at the
# same time, like clang-cl.exe.

if(MSVC)
    # Silence the usual warnings for POSIX and standard C functions.
    list(APPEND COMMON_COMPILE_OPTIONS "/D_CRT_NONSTDC_NO_DEPRECATE")
    list(APPEND COMMON_COMPILE_OPTIONS "/D_CRT_SECURE_NO_WARNINGS")

    # Default warning setting for MSVC.
    _checked_add_compile_option(/W3)

    # Extra warnings for cl.exe.
    _checked_add_compile_option(/we4013) # Implicit function declaration.
    _checked_add_compile_option(/we4133) # Incompatible types.
    _checked_add_compile_option(/we4477) # Format string mismatch.

    # Disable several warnings for cl.exe.
    # An integer type is converted to a smaller integer type.
    _checked_add_compile_option(/wd4244)
    # Conversion from size_t to a smaller type.
    _checked_add_compile_option(/wd4267)
    # Using the token operator to compare signed and unsigned numbers required
    # the compiler to convert the signed value to unsigned.
    _checked_add_compile_option(/wd4018)
else()
    _checked_add_compile_option(-Werror=array-bounds)
    _checked_add_compile_option(-Werror=clobbered)
    _checked_add_compile_option(-Werror=format-security)
    _checked_add_compile_option(-Werror=implicit-function-declaration)
    _checked_add_compile_option(-Werror=incompatible-pointer-types)
    _checked_add_compile_option(-Werror=int-conversion)
    _checked_add_compile_option(-Werror=volatile-register-var)
    _checked_add_compile_option(-Wformat=2)
    _checked_add_compile_option(-Wnull-dereference)
    _checked_add_compile_option(-Wrestrict)
    # We only want -Wall on GCC compilers, since /Wall on MSVC is noisy.
    _checked_add_compile_option(-Wall)
endif()

if(ENABLE_WERROR)
    if(MSVC)
        _checked_add_compile_option(/WX)
    else()
        _checked_add_compile_option(-Werror)
    endif()
endif()

if(ENABLE_ASAN)
    if(MSVC)
        _checked_add_compile_option(-fsanitize=address)
    else()
        # Set -Werror to catch "argument unused during compilation" warnings.
        # Also needs to be a link flag for test to pass.
        set(CMAKE_REQUIRED_FLAGS "-Werror -fsanitize=address")
        _checked_add_compile_option(-fsanitize=address)
        unset(CMAKE_REQUIRED_FLAGS)
    endif()
    _checked_add_compile_option(-fno-omit-frame-pointer)
    _checked_add_link_option(-fsanitize=address)
endif()

if(ENABLE_UBSAN)
    # Set -Werror to catch "argument unused during compilation" warnings.
    # Also needs to be a link flag for test to pass.
    set(CMAKE_REQUIRED_FLAGS "-Werror -fsanitize=undefined")
    _checked_add_compile_option(-fsanitize=undefined)
    unset(CMAKE_REQUIRED_FLAGS)
    _checked_add_link_option(-fsanitize=undefined)
endif()

if(ENABLE_TSAN)
    # Set -Werror to catch "argument unused during compilation" warnings.
    # Also needs to be a link flag for test to pass.
    set(CMAKE_REQUIRED_FLAGS "-Werror -fsanitize=thread")
    _checked_add_compile_option(-g)
    _checked_add_compile_option(-fsanitize=thread)
    unset(CMAKE_REQUIRED_FLAGS)
    _checked_add_link_option(-fsanitize=thread)
endif()

if(ENABLE_HARDENING)
    _checked_add_compile_option(-fstack-protector-strong)
    _checked_add_compile_option(-D_FORTIFY_SOURCE=2)
    _checked_add_link_option(-Wl,-z,relro)
endif()

function(target_woof_settings)
    foreach(target ${ARGN})
        target_compile_options(${target} PRIVATE ${COMMON_COMPILE_OPTIONS})
        target_link_options(${target} PRIVATE ${COMMON_LINK_OPTIONS})
        if(HAVE_LTO)
            set_target_properties(${target} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif()
    endforeach()
endfunction()
