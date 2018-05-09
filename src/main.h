/**
 * @file main.h
 * @brief Entry file containing main unction, where CLI arguments are processed and where the Supervisor is launched.
 * */

#ifndef MAIN_H
#define MAIN_H

#include <stdbool.h>

extern bool daemon_flag; ///< CLI startup option to tell whether to start as daemon
extern char *logs_path; ///< Path to where logs directory should reside

#endif
