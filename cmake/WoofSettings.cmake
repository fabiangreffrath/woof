# Add woof settings to a target.

include(CheckCCompilerFlag)
include(CheckLinkerFlag)

# Ninja can suppress colored output, toggle this to enable it again.
option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." FALSE)

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

_checked_add_compile_option(-Wdeclaration-after-statement)
_checked_add_compile_option(-Werror=implicit-function-declaration)
_checked_add_compile_option(-Werror=incompatible-pointer-types)
_checked_add_compile_option(-Werror=int-conversion)
_checked_add_compile_option(-Wformat=2)
_checked_add_compile_option(-Wnull-dereference)
_checked_add_compile_option(-Wredundant-decls)
_checked_add_compile_option(-Wrestrict)

# Hardening flags (from dpkg-buildflags)

_checked_add_compile_option(-fstack-protector-strong)
_checked_add_compile_option(-D_FORTIFY_SOURCE=2)
_checked_add_link_option(-Wl,-z,relro)

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

    # Extra warnings for clang-cl.exe - prevents warning spam in SDL headers.
    _checked_add_compile_option(-Wno-pragma-pack)
else()
    # We only want -Wall on GCC compilers, since /Wall on MSVC is noisy.
    _checked_add_compile_option(-Wall)
endif()

option(ENABLE_WERROR "Treat warnings as errors" OFF)
if(ENABLE_WERROR)
  _checked_add_compile_option(-Werror)
  if(MSVC)
    _checked_add_compile_option(/WX)
  endif()
endif()

if(${FORCE_COLORED_OUTPUT})
    _checked_add_compile_option(-fdiagnostics-color=always F_DIAG_COLOR)
    if (NOT F_DIAG_COLOR)
        _checked_add_compile_option(-fcolor-diagnostics F_COLOR_DIAG)
    endif()
endif()

function(target_woof_settings)
    foreach(target ${ARGN})
        target_compile_options(${target} PRIVATE ${COMMON_COMPILE_OPTIONS})
        target_link_options(${target} PRIVATE ${COMMON_LINK_OPTIONS})
    endforeach()
endfunction()
