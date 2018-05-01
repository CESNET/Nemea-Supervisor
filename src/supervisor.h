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
 * @brief
 * */
extern int init_paths();

/**
 * @brief TODO
 * */
extern int daemon_init_process();

/**
 * @brief
 * */
extern int supervisor_initialization();

/**
 * @brief
 * */
extern void supervisor_routine();

/**
 * @brief
 * */
extern void terminate_supervisor(bool should_terminate_insts);

#endif
