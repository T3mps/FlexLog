#pragma once

//=============================================================================
// Operating System Detection
//=============================================================================

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__TOS_WIN__) || defined(__WINDOWS__)
    #define FLOG_PLATFORM_WINDOWS
    
    #if defined(WINAPI_FAMILY)
        #include <winapifamily.h>
        #if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
            #define FLOG_PLATFORM_WINDOWS_DESKTOP
        #elif WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
            #define FLOG_PLATFORM_WINDOWS_STORE
            #define FLOG_PLATFORM_WINDOWS_UWP
        #endif
    #else
        #define FLOG_PLATFORM_WINDOWS_DESKTOP
    #endif

    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #define FLOG_PLATFORM_APPLE

    #if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
        #define FLOG_PLATFORM_IOS
    #elif TARGET_OS_MAC
        #define FLOG_PLATFORM_MACOS
    #else
        #define FLOG_PLATFORM_APPLE_UNKNOWN
    #endif
    
#elif defined(__ANDROID__)
    #define FLOG_PLATFORM_ANDROID
    
#elif defined(__linux__)
    #define FLOG_PLATFORM_LINUX
    
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    #define FLOG_PLATFORM_FREEBSD
    
#elif defined(__OpenBSD__)
    #define FLOG_PLATFORM_OPENBSD
    
#elif defined(__NetBSD__)
    #define FLOG_PLATFORM_NETBSD
    
#elif defined(__DragonFly__)
    #define FLOG_PLATFORM_DRAGONFLY
    
#elif defined(__EMSCRIPTEN__)
    #define FLOG_PLATFORM_WEB
    #define FLOG_PLATFORM_EMSCRIPTEN
    
#elif defined(__CYGWIN__)
    #define FLOG_PLATFORM_CYGWIN
    
#elif defined(__QNX__) || defined(__QNXNTO__)
    #define FLOG_PLATFORM_QNX
    
#elif defined(__unix__)
    #define FLOG_PLATFORM_UNIX
    
#elif defined(__wasi__)
    #define FLOG_PLATFORM_WASI
    
#else
    #error "Unknown platform - please define appropriate FLOG_PLATFORM_XXX macro"
#endif

#if defined(FLOG_PLATFORM_LINUX) || defined(FLOG_PLATFORM_APPLE) || \
    defined(FLOG_PLATFORM_FREEBSD) || defined(FLOG_PLATFORM_OPENBSD) || \
    defined(FLOG_PLATFORM_NETBSD) || defined(FLOG_PLATFORM_DRAGONFLY) || \
    defined(FLOG_PLATFORM_UNIX)
    #define FLOG_PLATFORM_UNIX_LIKE
#endif

#if defined(FLOG_PLATFORM_UNIX_LIKE) || defined(FLOG_PLATFORM_QNX) || defined(FLOG_PLATFORM_CYGWIN)
    #define FLOG_PLATFORM_POSIX
#endif

//=============================================================================
// Compiler Detection
//=============================================================================

#if defined(_MSC_VER)
    #define FLOG_COMPILER_MSVC
    #define FLOG_COMPILER_VERSION _MSC_VER
    #define FLOG_COMPILER_NAME "MSVC"
    
    #pragma warning(disable: 4251) // class X needs to have dll-interface to be used by clients of class Y
    
#elif defined(__clang__)
    #define FLOG_COMPILER_CLANG
    #define FLOG_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
    #define FLOG_COMPILER_NAME "Clang"
    
#elif defined(__GNUC__) || defined(__GNUG__)
    #define FLOG_COMPILER_GCC
    #define FLOG_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
    #define FLOG_COMPILER_NAME "GCC"
    
#elif defined(__INTEL_COMPILER) || defined(__ICC)
    #define FLOG_COMPILER_INTEL
    #define FLOG_COMPILER_VERSION __INTEL_COMPILER
    #define FLOG_COMPILER_NAME "Intel C++"
    
#elif defined(__MINGW32__) || defined(__MINGW64__)
    #define FLOG_COMPILER_MINGW
    #define FLOG_COMPILER_NAME "MinGW"
    
#elif defined(__EMSCRIPTEN__)
    #define FLOG_COMPILER_EMSCRIPTEN
    #define FLOG_COMPILER_NAME "Emscripten"
    
#else
    #define FLOG_COMPILER_UNKNOWN
    #define FLOG_COMPILER_NAME "Unknown"
#endif

//=============================================================================
// Architecture Detection
//=============================================================================

#if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__) || defined(__aarch64__) || \
    defined(__LP64__) || defined(_LP64) || (defined(__WORDSIZE) && __WORDSIZE == 64)
    #define FLOG_ARCH_64BIT
#else
    #define FLOG_ARCH_32BIT
#endif

#if defined(__i386__) || defined(_M_IX86)
    #define FLOG_ARCH_X86
    #define FLOG_ARCH_NAME "x86"
    
#elif defined(__x86_64__) || defined(_M_X64)
    #define FLOG_ARCH_X64
    #define FLOG_ARCH_X86_FAMILY
    #define FLOG_ARCH_NAME "x86_64"

#elif defined(__arm__) || defined(_M_ARM)
    #define FLOG_ARCH_ARM
    #define FLOG_ARCH_ARM32
    #define FLOG_ARCH_NAME "ARM"
    
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define FLOG_ARCH_ARM
    #define FLOG_ARCH_ARM64
    #define FLOG_ARCH_NAME "ARM64"
    
#elif defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC)
    #define FLOG_ARCH_POWERPC
    #define FLOG_ARCH_PPC32
    #define FLOG_ARCH_NAME "PowerPC"
    
#elif defined(__ppc64__)
    #define FLOG_ARCH_POWERPC
    #define FLOG_ARCH_PPC64
    #define FLOG_ARCH_NAME "PowerPC64"
    
#elif defined(__riscv)
    #define FLOG_ARCH_RISCV
    #if __riscv_xlen == 32
        #define FLOG_ARCH_RISCV32
        #define FLOG_ARCH_NAME "RISC-V 32-bit"
    #elif __riscv_xlen == 64
        #define FLOG_ARCH_RISCV64
        #define FLOG_ARCH_NAME "RISC-V 64-bit"
    #endif
    
#elif defined(__mips__) || defined(__mips) || defined(__MIPS__)
    #define FLOG_ARCH_MIPS
    #define FLOG_ARCH_NAME "MIPS"
    
#else
    #define FLOG_ARCH_UNKNOWN
    #define FLOG_ARCH_NAME "Unknown"
#endif

//=============================================================================
// Platform String Helper
//=============================================================================

#if defined(FLOG_PLATFORM_WINDOWS)
    #define FLOG_PLATFORM_NAME "Windows"
#elif defined(FLOG_PLATFORM_MACOS)
    #define FLOG_PLATFORM_NAME "macOS"
#elif defined(FLOG_PLATFORM_IOS)
    #define FLOG_PLATFORM_NAME "iOS"
#elif defined(FLOG_PLATFORM_ANDROID)
    #define FLOG_PLATFORM_NAME "Android"
#elif defined(FLOG_PLATFORM_LINUX)
    #define FLOG_PLATFORM_NAME "Linux"
#elif defined(FLOG_PLATFORM_WEB)
    #define FLOG_PLATFORM_NAME "Web"
#elif defined(FLOG_PLATFORM_FREEBSD)
    #define FLOG_PLATFORM_NAME "FreeBSD"
#elif defined(FLOG_PLATFORM_OPENBSD)
    #define FLOG_PLATFORM_NAME "OpenBSD"
#elif defined(FLOG_PLATFORM_NETBSD)
    #define FLOG_PLATFORM_NAME "NetBSD"
#elif defined(FLOG_PLATFORM_DRAGONFLY)
    #define FLOG_PLATFORM_NAME "DragonFlyBSD"
#elif defined(FLOG_PLATFORM_QNX)
    #define FLOG_PLATFORM_NAME "QNX"
#elif defined(FLOG_PLATFORM_CYGWIN)
    #define FLOG_PLATFORM_NAME "Cygwin"
#elif defined(FLOG_PLATFORM_UNIX)
    #define FLOG_PLATFORM_NAME "Unix"
#else
    #define FLOG_PLATFORM_NAME "Unknown"
#endif

// Normalize platform line endings
#if defined(FLOG_PLATFORM_WINDOWS)
    #define FLOG_NEWLINE "\r\n"
#else
    #define FLOG_NEWLINE "\n"
#endif
