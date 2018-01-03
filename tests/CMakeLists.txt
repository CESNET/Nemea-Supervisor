cmake_minimum_required(VERSION 3.8)
set (CMAKE_C_STANDARD 11)
#-lpthread -ltrap -lsysrepo
#-I/home/ioku/school/diplomka/libs_from_git/installed_libs/include
# plus poresit vsechny checky v config.h
set (CMAKE_C_FLAGS "-Wall -g -O0 ${CMAKE_C_FLAGS}") # debug mode


# my debug settings
#include_directories(../ /home/ioku/school/diplomka/libs_from_git/installed_libs/include/ /home/ioku/school/diplomka/libs_from_git/installed_libs/include/sysrepo/)
#link_directories(/home/ioku/school/diplomka/libs_from_git/installed_libs/lib64)
###
include_directories(../)



# TODO compile this alongside with conf_test
add_executable(intable_module intable_module.c)



set (SRC_FILES_1 ../src/instance_control.c ../src/conf.c ../src/utils.c ../src/module.c)
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

set (SRC_FILES_5 ../src/utils.c ../src/module.c ../src/conf.c ../src/instance_control.c ../src/run_changes.c ../src/stats.c)
add_executable(test_supervisor test_supervisor.c ${SRC_FILES_5})
target_link_libraries(test_supervisor cmocka sysrepo trap pthread)
