cmake_minimum_required(VERSION 3.9)
set (CMAKE_C_STANDARD 99)
#-lpthread -ltrap -lsysrepo
#-I/home/ioku/school/diplomka/libs_from_git/installed_libs/include
# plus poresit vsechny checky v config.h
set (CMAKE_C_FLAGS "-Wall -g -O0 ${CMAKE_C_FLAGS}") # debug mode


# my debug settings
#include_directories(../ /home/ioku/school/diplomka/libs_from_git/installed_libs/include/ /home/ioku/school/diplomka/libs_from_git/installed_libs/include/sysrepo/)
#link_directories(/home/ioku/school/diplomka/libs_from_git/installed_libs/lib64)
###


set (EXEC1 subscribe)
add_executable(${EXEC1} ${EXEC1}.c)
target_link_libraries(${EXEC1} sysrepo)

#set (EXEC2 nemea_get_subtree_noniter)
#add_executable(${EXEC2} ${EXEC2}.c)
#target_link_libraries(${EXEC2} sysrepo)


#set (EXEC3 types)
#add_executable(${EXEC3} ${EXEC3}.c)

#set (EXEC4 parse_stats)
#add_executable(${EXEC4} ${EXEC4}.c)