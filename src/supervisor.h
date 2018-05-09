/**
 * @file supervisor.h
 * @brief Core of the supervisor, contains main daemon loop/routine.
 */
#ifndef SUPERVISOR_H
#define SUPERVISOR_H


#include <inttypes.h>
#include <sys/stat.h>
#include "module.h"
#include "utils.h"


/**
 * @brief Initializes paths for logs.
 * @params 0 on succcess, -1 on error
 * */
extern int init_paths();

/**
 * @brief Forks Supervisor daemon to background.
 * @params 0 on succcess, -1 on error
 * */
extern int daemon_init_process();

/**
 * @brief Connects to sysrepo, subsribes callbacks, loads data and registers signal handlers
 * @param 0 on success, -1 or other SR error code on error
 * */
extern int supervisor_initialization();

/**
 * @brief Daemon routine of the Supervisor
 * */
extern void supervisor_routine();

/**
 * @brief Shut down procedure of the supervisor, frees everything
 * @param should_terminate_insts Whether to kill all instances
 * */
extern void terminate_supervisor(bool should_terminate_insts);

#endif
