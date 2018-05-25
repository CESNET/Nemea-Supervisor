cmake_minimum_required(VERSION 3.8)
project(nemea-supervisor C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules/")

find_package(TRAP REQUIRED)
if(TRAP_FOUND)
    include_directories(TRAP_INCLUDE_DIR)
else(TRAP_FOUND)
    message(FATAL_ERROR "Required dependency libtrap is missing!")
endif(TRAP_FOUND)

find_package(SYSREPO REQUIRED)
if(SYSREPO_FOUND)
    include_directories(SYSREPO_INCLUDE_DIR)
else(SYSREPO_FOUND)
    message(FATAL_ERROR "Required dependency sysrepo is missing!")
endif(SYSREPO_FOUND)

# Testing
SET(ENABLE_TESTS 1 CACHE BOOL "Enable unit tests.")
if(ENABLE_TESTS)
    find_package(CMOCKA)
    if(CMOCKA_FOUND)
        MESSAGE(STATUS "CMocka found, tests are enabled.")
        enable_testing()
        add_subdirectory(tests)
    else(CMOCKA_FOUND)
        MESSAGE(WARNING "CMocka not found, tests are disabled.")
        SET(ENABLE_TESTS false)
    endif(CMOCKA_FOUND)
endif(ENABLE_TESTS)

add_subdirectory(src)