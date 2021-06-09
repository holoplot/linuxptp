# linuxptp/CMakeLists.txt  --  Ravenna Project
# Copyright (C) 2013, ALC NetworX GmbH -- All rights reserved.
# For copyright information and disclaimer see file COPYRIGHT in root directory of source tree (or contact ALC NetworX GmbH)

cmake_minimum_required(VERSION 2.8)
project(linuxptp)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds are not allowed!")
endif()

if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE RELWITHDEBINFO)
    message (STATUS "No build type selected, default to RELWITHDEBINFO")
endif ()

if((${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm-imx") OR (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm") OR (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "armv5e-poky"))
    message(STATUS "RAVENNA target platform set to COMIMX Linux")

    if (RV_PROFILE_GENERATE)
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-generate")
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-generate")
    elseif (RV_PROFILE_USE)
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-use")
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-use")
    endif ()
    
    if (NOT DEFINED CMAKE_INSTALL_PREFIX)
        set (CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/build/arm-imx25-linux-gnueabi/release)
    endif ()
    
    set (EXT_LIBS ${EXT_LIBS} atomic)
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "core2-64")
    message(STATUS "RAVENNA target platform set to UHDCORE Linux")

    if (RV_PROFILE_GENERATE)
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-generate")
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-generate")
    elseif (RV_PROFILE_USE)
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-use")
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-use")
    endif ()
    
    if (NOT DEFINED CMAKE_INSTALL_PREFIX)
        set (CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/build/i686-mcx-linux-gnu/release)
    endif ()
    
    set (EXT_LIBS ${EXT_LIBS} atomic)
else()
    message(STATUS "RAVENNA target platform set to PC Linux")

    if (RV_PROFILE_GENERATE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
    endif ()
    
    # install folder for the x86 build lawo build system
    if (NOT DEFINED CMAKE_INSTALL_PREFIX)
        set (CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/build/i686-mcx-linux-gnu/release)
    endif ()
endif ()

message (STATUS ${CMAKE_GENERATOR})

# Linux/gcc compiler settings
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Wall -Wextra -Wformat=2 -Wsign-compare -Wshadow -Wno-unused-parameter -Wno-unused-result -ffast-math")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra -Wsign-compare -Wno-unused-parameter")

set (CMAKE_C_FLAGS_DEBUG "-g -fsanitize=address -fsanitize=leak -fsanitize=undefined -fstack-protector-all")
set (CMAKE_CXX_FLAGS_DEBUG "-g -fsanitize=address -fsanitize=leak -fsanitize=undefined -fstack-protector-all")

set (CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g")
set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

add_definitions (-D_GNU_SOURCE -D_POSIX_SYNCHRONIZED_IO -D_POSIX_C_SOURCE=200112L)
set (EXT_LIBS ${EXT_LIBS} rt m dl)

add_subdirectory(src)
#add_subdirectory(test)
