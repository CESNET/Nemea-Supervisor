set (CMAKE_C_STANDARD 11)
set (EXECUTABLE_NAME nemea-supervisor)
set (SOURCE_FILES supervisor.c main.c utils.c module.c conf.c inst_control.c run_changes.c stats.c service.c)
set (CMAKE_C_FLAGS "-Wall -g -O0 ${CMAKE_C_FLAGS}") # debug mode


# my debug settings
include_directories(../)

add_executable(${EXECUTABLE_NAME} ${SOURCE_FILES})
target_link_libraries(${EXECUTABLE_NAME} sysrepo)
target_link_libraries(${EXECUTABLE_NAME} trap)