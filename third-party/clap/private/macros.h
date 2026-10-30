/*
 * CLAP - CLever Audio Plugin
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Copyright (c) 2014...2022 Alexandre BIQUE <bique.alexandre@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

// Define CLAP_EXPORT
#if !defined(CLAP_EXPORT)
#   if defined _WIN32 || defined __CYGWIN__
#      ifdef __GNUC__
#         define CLAP_EXPORT __attribute__((dllexport))
#      else
#         define CLAP_EXPORT __declspec(dllexport)
#      endif
#   else
#      if __GNUC__ >= 4 || defined(__clang__)
#         define CLAP_EXPORT __attribute__((visibility("default")))
#      else
#         define CLAP_EXPORT
#      endif
#   endif
#endif

#if !defined(CLAP_ABI)
#   if defined _WIN32 || defined __CYGWIN__
#      define CLAP_ABI __cdecl
#   else
#      define CLAP_ABI
#   endif
#endif

#if defined(_MSVC_LANG)
#   define CLAP_CPLUSPLUS _MSVC_LANG
#elif defined(__cplusplus)
#   define CLAP_CPLUSPLUS __cplusplus
#endif

#if defined(CLAP_CPLUSPLUS) && CLAP_CPLUSPLUS >= 201103L
#   define CLAP_HAS_CXX11
#   define CLAP_CONSTEXPR constexpr
#else
#   define CLAP_CONSTEXPR
#endif

#if defined(CLAP_CPLUSPLUS) && CLAP_CPLUSPLUS >= 201703L
#   define CLAP_HAS_CXX17
#   define CLAP_NODISCARD [[nodiscard]]
#else
#   define CLAP_NODISCARD
#endif

#if defined(CLAP_CPLUSPLUS) && CLAP_CPLUSPLUS >= 202002L
#   define CLAP_HAS_CXX20
#endif