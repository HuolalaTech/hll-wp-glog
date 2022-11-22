cmake_minimum_required(VERSION 3.8.0)

set(can_use_assembler TRUE)
enable_language(ASM)
IF("${ANDROID_ABI}" STREQUAL "arm64-v8a")
    SET(ASM_OPTIONS "-x assembler-with-cpp")
    SET(CMAKE_ASM_FLAGS "${CFLAGS} ${ASM_OPTIONS} -march=armv8+crypto -D__ANDROID__")
ELSEIF("${ANDROID_ABI}" STREQUAL "armeabi-v7a")
    SET(ASM_OPTIONS "-x assembler-with-cpp")
    SET(CMAKE_ASM_FLAGS "${CFLAGS} ${ASM_OPTIONS} -march=armv7a -D__ANDROID__")
ELSEIF("${ANDROID_ABI}" STREQUAL "armeabi")
    SET(ASM_OPTIONS "-x assembler-with-cpp")
    SET(CMAKE_ASM_FLAGS "${CFLAGS} ${ASM_OPTIONS} -march=armv5 -D__ANDROID__")
ENDIF()

include(CMakePrintHelpers)
cmake_print_variables(CMAKE_SYSTEM_PROCESSOR)
IF(UNIX AND (NOT APPLE))
    IF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64")
        SET(ASM_OPTIONS "-x assembler-with-cpp")
        SET(CMAKE_ASM_FLAGS "${CFLAGS} ${ASM_OPTIONS} -march=armv8-a+crypto")
    ENDIF()
ENDIF()

project(core)

set(CMAKE_CXX_STANDARD 17)

# macro list
# ENABLE_INTERNAL_LOG
# GLOG_DEBUG
# GLOG_ANDROID
# GLOG_APPLE

#add_definitions(-DGLOG_DEBUG)
#add_definitions(-DGLOG_ANDROID)
#add_compile_definitions(DEBUG)

IF(APPLE) # build from command line neither in Xcode nor Android studio
    add_compile_definitions(FORCE_POSIX)
ENDIF()

add_library(core

        # Sets the library as a shared library.
        STATIC

        Glog_IO.cpp
        Glog_IO.h
        GlogBuffer.cpp
        GlogBuffer.h
        GlogFile.cpp
        GlogFile.h
        Glog.cpp
        Glog.h
        InternalLog.cpp
        InternalLog.h
        GlogPredef.h
        utilities.cpp
        utilities.h
        Compressor.h
        ZlibCompress.cpp
        ZlibCompress.h
        MessageQueue.cpp
        MessageQueue.h
        GlogReader.cpp
        GlogReader.h
        ThreadLock.cpp
        ThreadLock.h
        ScopedLock.h
        StopWatch.hpp

        openssl/openssl_aes.h
        openssl/openssl_aes-armv4.S
        openssl/openssl_aes_core.cpp
        openssl/openssl_aes_locl.h
        openssl/openssl_aesv8-armx.S
        openssl/openssl_arm_arch.h
        openssl/openssl_cfb128.cpp

        aes/AESCrypt.h
        aes/AESCrypt.cpp

        micro-ecc/asm_arm.inc
        micro-ecc/asm_arm_mult_square.inc
        micro-ecc/asm_arm_mult_square_umaal.inc
        micro-ecc/asm_avr.inc
        micro-ecc/asm_avr_mult_square.inc
        micro-ecc/curve-specific.inc
        micro-ecc/platform-specific.inc
        micro-ecc/types.h
        micro-ecc/uECC.cpp
        micro-ecc/uECC.h
        micro-ecc/uECC_vli.h
        )

target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(core PROPERTIES
        CXX_STANDARD 17
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
        )

find_library(zlib
        z
        )

target_link_libraries(core ${zlib})