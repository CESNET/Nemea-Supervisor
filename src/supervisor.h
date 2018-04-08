#ifndef SUP_H
#define SUP_H


#include <inttypes.h>
#include <sys/stat.h>

#include "module.h"
#include "utils.h"

extern int init_paths();
extern int daemon_init_process();
extern int supervisor_initialization();
extern void supervisor_routine();
extern void terminate_supervisor(bool);

#endif
