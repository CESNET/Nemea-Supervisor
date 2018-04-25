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
 * @brief Sends SIGINT signal to all modules that TODO ...
 * @param module_ll_head First node of linked list of modules to signal
 * */
extern void insts_stop_sigint();

/**
 * @brief Sends SIGKILL signal to all modules that TODO ...
 * @param module_ll_head First node of linked list of modules to signal
 * */
extern void insts_stop_sigkill();

/**
 * @brief Stops all instances of module, removes all their structures and structure of module.
 * @param name Name of module
 * */
extern void av_module_stop_remove_by_name(const char *name);

/**
 * @brief Stops module of given name
 * @details First it's using stop_modules_sigint, then stop_modules_sigkill and
 *  finally it cleans the module structure.
 * @param name Name of module to stop
 * @param remove Whether to remove module from vector of configured modules
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
