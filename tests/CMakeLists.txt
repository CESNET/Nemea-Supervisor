cmake_minimum_required(VERSION 3.8)
set (CMAKE_C_STANDARD 11)

set (CMAKE_C_FLAGS "-Wall -g -O0 ${CMAKE_C_FLAGS}") # debug mode
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

include_directories(../)

add_executable(intable_module intable_module.c)

add_definitions(-DNS_TEST=1)
add_definitions(-DNS_ROOT_XPATH="/nemea-test-1:supervisor")
add_definitions(-DNS_ROOT_XPATH_LEN=24)


set (SRC_FILES_1 ../src/utils.c ../src/module.c ../src/inst_control.c ../src/conf.c)
add_executable(test_run_changes test_run_changes.c ${SRC_FILES_1})
target_link_libraries(test_run_changes sysrepo pthread cmocka trap)

set (SRC_FILES_2 ../src/utils.c)
add_executable(test_module test_module.c ${SRC_FILES_2})
target_link_libraries(test_module cmocka trap sysrepo)

set (SRC_FILES_3 ../src/utils.c ../src/module.c ../src/conf.c)
add_executable(test_stats test_stats.c ${SRC_FILES_3})
target_link_libraries(test_stats cmocka sysrepo trap pthread)

set (SRC_FILES_4 ../src/utils.c ../src/module.c)
add_executable(test_conf test_conf.c ${SRC_FILES_4})
target_link_libraries(test_conf cmocka sysrepo trap pthread)

set (SRC_FILES_5 ../src/utils.c ../src/module.c ../src/conf.c ../src/inst_control.c ../src/run_changes.c ../src/stats.c ../src/service.c)
add_executable(test_supervisor test_supervisor.c ${SRC_FILES_5})
target_link_libraries(test_supervisor cmocka sysrepo trap pthread)

set (SRC_FILES_6 ../src/utils.c ../src/module.c ../src/conf.c)
add_executable(test_inst_control test_inst_control.c ${SRC_FILES_6})
target_link_libraries(test_inst_control cmocka sysrepo trap pthread)

add_executable(test_utils test_utils.c)
target_link_libraries(test_utils cmocka)
