/**
 * @file inst_control.h
 * @brief Takes care of starting and stopping of instances.
 */

#ifndef MODULE_CONTROL_H
#define MODULE_CONTROL_H

#include "module.h"

/**
 * @details Time in micro seconds between sending SIGINT and SIGKILL to running
 *  modules_ll. Service thread sends SIGINT to stop running module, after time
 *  defined by this constant it checks modules_ll status and if the module is still
 *  running, service thread sends SIGKILL to stop it.
 */
#define WAIT_FOR_INSTS_TO_HANDLE_SIGINT 500000

/**
 * @brief Permissions of directory with stdout and stderr logs of instances
 */
#define PERM_LOGSDIR   (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | \
                        S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH)

extern char *logs_path; ///< Path to where logs directory should reside

/**
 * @brief Checks and assigns running status of each instance
 * @return number of running instances
 * */
extern uint32_t get_running_insts_cnt();

/**
 * @brief Checks and assigns running status of single instance
 * */
extern void inst_set_running_status(inst_t *inst);

/**
 * @brief Sends SIGINT signal to all instances that have (.enabled=false || .should_die=true) && .sigint_sent=false
 * */
extern void insts_stop_sigint();

/**
 * @brief Sends SIGKILL signal to all instances that (.enabled=false || .should_die=true) && .sigint_sent=true
 * */
extern void insts_stop_sigkill();

/**
 * @brief Stops all instances of given module, removes all their structures and structure of module.
 * @param name Name of module
 * */
extern void av_module_stop_remove_by_name(const char *name);

/**
 * @brief Stops instance of given name
 * @param name Name of instance to stop
 * */
extern void inst_stop_remove_by_name(const char *name);

/**
 * @brief Start all instances in insts_v vector
 * */
extern void insts_start();

/**
 * @brief Terminates all instances in insts_v vector
 * */
extern void insts_terminate();

#endif
