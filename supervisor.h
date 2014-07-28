/**
 * \file supervisor.h
 * \brief Structures and functions for supervisor.
 * \author Marek Svepes <svepemar@fit.cvut.cz>
 * \date 2013
 * \date 2014
 */
/*
 * Copyright (C) 2013,2014 CESNET
 *
 * LICENSE TERMS
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <error.h>
#include <errno.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <limits.h>

#ifndef PERM_LOGSDIR
#define PERM_LOGSDIR    (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH) ///< Permissions of directory with stdout and stderr logs of modules
#endif

#ifndef PERM_LOGFILE
#define PERM_LOGFILE    (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH) ///< Permissions of files with stdout and stderr logs of module
#endif

#define TRUE                        1 ///< Bool true
#define FALSE                       0 ///< Bool false
#define RUNNING_MODULES_ARRAY_START_SIZE  10 ///< Initial size of allocated running_modules array.
#define IFCES_ARRAY_START_SIZE            5 ///< Initial size of allocated interface_t array of every module.



/***********STRUCTURES***********/

/** Structure with information about one loaded interface of module */
typedef struct interface_s {
   char *ifc_note; ///< Interface note
   char *ifc_type; ///< Interface type (TCP/UNIXSOCKET)
   char *ifc_params; ///< Interface parameters (input interface ~ address,port; output interface ~ port,number of connections)
   char *ifc_direction; ///< Interface direction (IN, OUT)
} interface_t;

/** Structure with information about one running module */
typedef struct running_module_s {
   int            module_enabled; ///< TRUE if module is enabled, else FALSE.
   char          *module_params; ///< Module parameter (loaded from config file).
   char          *module_name; ///< Module name (loaded from config file).
   char          *module_path; ///< Path to module from current directory
   interface_t   *module_ifces; ///< Array of interface_t structures with information about every loaded interface of module
   int            module_ifces_array_size; ///< Number of allocated interface_t structures by module.
   int            module_ifces_cnt; ///< Number of modules loaded interfaces.
   int            module_num_out_ifc; ///< Number of modules output interfaces.
   int            module_num_in_ifc; ///< Number of modules input interfaces.
   int            module_restart_timer;  ///< Timer used for monitoring max number of restarts/minute.
   int            module_restart_cnt; ///< Number of module restarts.
   int            module_has_service_ifc; ///< if module has service interface ~ TRUE, else ~ FALSE
   int            module_service_ifc_isconnected; ///< if supervisor is connected to module ~ TRUE, else ~ FALSE
   int            module_served_by_service_thread; ///< TRUE if module was added to graph struct by sevice thread, FALSE on start.
   int            module_running; ///< TRUE after first start of module, else FALSE.
   int            module_status; ///< Module status (TRUE ~ running, FALSE ~ stopped)
   int            sent_sigint;
   pid_t          module_pid; ///< Modules process PID.
   int           *module_counters_array; ///< Array of statistics with counters.
   int            module_service_sd; ///< Socket descriptor of the service connection.
   int            module_number; ///< Index to running_modules array.
   int            module_modified_by_reload;
   long int       total_cpu_usage_during_module_startup;
   int            last_period_cpu_usage_kernel_mode; ///< Percentage of CPU usage in last period in kernel mode.
   int            last_period_cpu_usage_user_mode; ///< Percentage of CPU usage in last period in user mode.
   int            last_period_percent_cpu_usage_kernel_mode; ///< Percentage of CPU usage in current period in kernel mode.
   int            last_period_percent_cpu_usage_user_mode; ///< Percentage of CPU usage in current period in user mode.
   int            overall_percent_module_cpu_usage_kernel_mode;
   int            overall_percent_module_cpu_usage_user_mode;
} running_module_t;

/***********FUNCTIONS***********/

/** Function parses XML file or buffer with XML code and saves modules configuration in running_modules array.
 * @param[in] choice If TRUE -> parse file, else parse buffer.
 * @param[in] buffer If choice TRUE -> file name, else buffer with XML code.
 * @return TRUE if success, else FALSE.
 */
int load_configuration(const int choice, const char * buffer);

/** Function prints list of loaded modules with their params and all their interfaces with params.
 */
void interactive_show_available_modules ();

/** Function creates array of strings for function execvp() before executing new module (its process).
 * @param[in] number_of_module Index to running_modules array.
 * @return Array of strings. 
 */
char **make_module_arguments(const int number_of_module);

int get_number_from_input();

/** Function prints a list of user operations and scans one number as an input.
 * @return Number of selected operation.
 */
int interactive_get_option();

/** Function is used for first start of module, allocates its array for statistics, initialize its variables, creates new process, redirects stdout, stderr and executes module.
 * @param[in] module_number Index to running_modules array.
 */
void re_start_module(const int module_number);

/** Function sends SIGINT to selected module.
 * @param[in] module_number Index to running_modules array.
 */
void stop_module(const int module_number);

/** Function updates running modules processes status.
 */
void service_update_module_status();

/** SIGPIPE handler.
 */
void sigpipe_handler(int sig);

/** Whole program initialization; global variables, input params parsing, supervisor mode selection.
 * @param[in] argc Argc of main function.
 * @param[in] argv Argv of main function.
 * @return 0 if interactive mode, 2 if daemon mode, 1 if error.
 */
int supervisor_initialization(int *argc, char **argv);

/** Starts every loaded module.
 */
void interactive_start_configuration();

/** Stops every loaded module.
 */
void interactive_stop_configuration();

/** Function scans one number as an input and uses it as an index to running_module array setting selected module enabled.
 */
void interactive_set_module_enabled();

/** Function scans one number as an input and pastes it as and argument to function stop_module();
 */
void interactive_stop_module();

/** Function uses restart_module() for stopped, enabled modules.
 */
void service_restart_modules();

/** Function prints a list of loaded modules with their status - running/stopped/remote.
 */
void interactive_show_running_modules_status();

/** Function stops service thread and acceptor thread and frees allocated memory.
 */
void supervisor_termination();

char *get_param_by_delimiter(const char *source, char **dest, const char delimiter);

long int get_total_cpu_usage();

/** Function updates running modules CPU usage - in kernel/user mode.
 * @param[in] last_total_cpu_usage Total cpu usage in last period.
 */
void update_module_cpu_usage(long int * last_total_cpu_usage);

void generate_periodic_picture();

void interactive_show_graph();

/** Function receives data from running modules with statistics.
 * @param[in] sd Socket descriptor of module.
 * @param[in] running_module_number Index to running_modules array.
 * @return 0 if error, else 1;
 */
int service_get_data(int sd, int running_module_number);

/** Function tries to connect to selected module.
 * @param[in] module Index to running_modules array.
 * @param[in] num_ifc Index to selected modules interfaces array.
 */
void connect_to_module_service_ifc(int module, int num_ifc);

void print_statistics_and_cpu_usage(struct tm * timeinfo);

void print_statistics_legend();

/** Main routine of service thread.
 * @param[in] arg NULL.
 */
void * service_thread_routine(void* arg);

char * make_formated_statistics();

/** Function creates 2 threads - service and acceptor thread.
 */
void start_service_thread();

/** Function parses program arguments.
 * @param[in] argc Argc of main function.
 * @param[in] argv Argv of main function.
 * @return 1 if success, 0 if error.
 */
int parse_arguments(int *argc, char **argv);

/** Function prints a help to supervisor after executing program with -h argument.
 */
void print_help();

/** Function creates daemon socket, initialize daemon and redirect stdout.
 * @param[out] d_sd Socket descriptor of the created socket.
 * @return 0 if success, else error;
 */
int daemon_init(int * d_sd);

/** Function accepts new connection from supervisor_cli.
 * @param[in] d_sd Daemon socket descriptor.
 * @return Returns clients socket descriptor, -1 if error.
 */
int daemon_get_client (int * d_sd);

/** Function creates input and output filestream via supervisor_cli socket. After that it represents main loop function.
 * @param[in] arg Daemon socket descriptor.
 */
void daemon_mode(int * arg);

int get_shorter_string_length(char * first, char * second);

int find_loaded_module(char * name);

void free_module_on_index(int index);

void free_module_interfaces_on_index(int index);

int reload_configuration(const int choice, xmlNodePtr node);

#endif

