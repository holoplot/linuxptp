# linuxptp/src/CMakeLists.txt  --  Ravenna Project
# Copyright (C) 2013, ALC NetworX GmbH -- All rights reserved.
# For copyright information and disclaimer see file COPYRIGHT in root directory of source tree (or contact ALC NetworX GmbH)

find_package(PkgConfig REQUIRED)

include(CheckLibraryExists)

CHECK_LIBRARY_EXISTS(rt clock_adjtime "time.h" HAVE_CLOCK_ADJTIME)
if(HAVE_CLOCK_ADJTIME)
    add_definitions (-DHAVE_CLOCK_ADJTIME)
endif(HAVE_CLOCK_ADJTIME)

add_definitions(-D_LARGEFILE64_SOURCE)
add_definitions(-DHAVE_ONESTEP_SYNC)
add_definitions(-DVER=1.8)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")

# generate schema.inc in the binary directory if someone wants to check against the json schema
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/schema.inc
    COMMAND cat "${CMAKE_CURRENT_SOURCE_DIR}/schema.json" | "${CMAKE_SOURCE_DIR}/cmake/json2cString.sh" linuxptp_schema > "${CMAKE_CURRENT_BINARY_DIR}/schema.inc"
)

FILE(GLOB linuxptp_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.[ch])
source_group("Private Source Files" FILES ${linuxptp_SOURCES})

#add_executable(hwstamp_ctl hwstamp_ctl.c ${CMAKE_CURRENT_BINARY_DIR}/schema.inc)
#target_link_libraries(hwstamp_ctl linuxptp)
#install (TARGETS hwstamp_ctl DESTINATION ./)

#add_executable(phc2sys phc2sys.c)
#target_link_libraries(phc2sys linuxptp)
#install (TARGETS phc2sys DESTINATION ./)

#add_executable(pmc pmc.c pmc.c)
#target_link_libraries(pmc linuxptp)
#install (TARGETS pmc DESTINATION ./)

if(NOT DEFINED pkg_uv_FOUND)
    pkg_search_module(pkg_uv REQUIRED libuv)
endif()

if(NOT DEFINED pkg_jansson_FOUND)
    pkg_search_module(pkg_jansson REQUIRED jansson)
endif()

find_library(PAHO_MQTT paho-mqtt3as REQUIRED)

add_executable(ptp4l ${linuxptp_SOURCES})

target_include_directories(ptp4l 
    PRIVATE 
        ${pkg_uv_INCLUDE_DIRS} 
        ${pkg_jansson_INCLUDE_DIRS} 
)

target_link_libraries(ptp4l
    PRIVATE
        ${pkg_uv_LIBRARIES} 
        ${pkg_jansson_LIBRARIES} 
        ${PAHO_MQTT}
        ${EXT_LIBS}
	m
)

install (TARGETS ptp4l DESTINATION bin)
