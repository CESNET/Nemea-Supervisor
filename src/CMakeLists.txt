set (CMAKE_C_STANDARD 11)
#-lpthread -ltrap -lsysrepo
#-I/home/ioku/school/diplomka/libs_from_git/installed_libs/include
# plus poresit vsechny checky v config.h
set (EXECUTABLE_NAME nemea-supervisor)
set (SOURCE_FILES supervisor.c main.c utils.c module.c conf.c inst_control.c run_changes.c stats.c service.c)
set (CMAKE_C_FLAGS "-Wall -g -O0 ${CMAKE_C_FLAGS}") # debug mode


# my debug settings
include_directories(../)
#/home/ioku/school/diplomka/libs_from_git/installed_libs/include/ /home/ioku/school/diplomka/libs_from_git/installed_libs/include/sysrepo/)
#link_directories(/home/ioku/school/diplomka/libs_from_git/installed_libs/lib64)
###


add_executable(${EXECUTABLE_NAME} ${SOURCE_FILES})
target_link_libraries(${EXECUTABLE_NAME} sysrepo)
target_link_libraries(${EXECUTABLE_NAME} trap)