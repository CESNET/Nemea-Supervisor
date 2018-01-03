cmake_minimum_required(VERSION 3.8)
project(nemea_supervisor)

add_subdirectory(src)

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