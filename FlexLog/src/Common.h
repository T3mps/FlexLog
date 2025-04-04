#pragma once

#include "Platform.h"


#ifdef FLOG_LOG_EXPORTS
    #define FLOG_API __declspec(dllexport)
#else
    #define FLOG_API __declspec(dllimport)
#endif

#if defined(FLOG_COMPILER_MSVC)
    #define FLOG_FORCE_INLINE __forceinline
    #define FLOG_NEVER_INLINE __declspec(noinline)
    #if _MSC_VER >= 1910 // Visual Studio 2017 and newer
        #define FLOG_FALLTHROUGH __fallthrough
    #else
        #define FLOG_FALLTHROUGH
    #endif
#elif defined(FLOG_COMPILER_CLANG)
    #define FLOG_FORCE_INLINE __attribute__((always_inline)) inline
    #define FLOG_NEVER_INLINE __attribute__((noinline))
    #if __has_cpp_attribute(fallthrough)
        #define FLOG_FALLTHROUGH [[fallthrough]]
    #elif __has_attribute(fallthrough)
        #define FLOG_FALLTHROUGH __attribute__((fallthrough))
    #else
        #define FLOG_FALLTHROUGH
    #endif
#elif defined(FLOG_COMPILER_GCC)
    #define FLOG_FORCE_INLINE __attribute__((always_inline)) inline
    #define FLOG_NEVER_INLINE __attribute__((noinline))
    #if __GNUC__ >= 7
        #define FLOG_FALLTHROUGH __attribute__((fallthrough))
    #else
        #define FLOG_FALLTHROUGH
    #endif
#elif defined(FLOG_COMPILER_INTEL)
    #if defined(FLOG_PLATFORM_WINDOWS)
        #define FLOG_FORCE_INLINE __forceinline
        #define FLOG_NEVER_INLINE __declspec(noinline)
    #else
        #define FLOG_FORCE_INLINE __attribute__((always_inline)) inline
        #define FLOG_NEVER_INLINE __attribute__((noinline))
    #endif
    // Intel compiler fallthrough support
    #if __has_cpp_attribute(fallthrough)
        #define FLOG_FALLTHROUGH [[fallthrough]]
    #else
        #define FLOG_FALLTHROUGH
    #endif
#else
    #define FLOG_FORCE_INLINE inline
    #define FLOG_NEVER_INLINE
    #define FLOG_FALLTHROUGH
#endif
