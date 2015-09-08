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

#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <inttypes.h>
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

#include <libtrap/trap.h>

#include <limits.h>

#ifndef PERM_LOGSDIR
#define PERM_LOGSDIR   (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH) ///< Permissions of directory with stdout and stderr logs of modules
#endif

#ifndef PERM_LOGFILE
#define PERM_LOGFILE   (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH) ///< Permissions of files with stdout and stderr logs of module
#endif

#define RUNNING_MODULES_ARRAY_START_SIZE   10 ///< Initial size of allocated running_modules array.
#define IFCES_ARRAY_START_SIZE   5 ///< Initial size of allocated interface_t array of every module.

#define IN_MODULE_IFC_DIRECTION   1  ///< Constant for input module interface direction
#define OUT_MODULE_IFC_DIRECTION   2  ///< Constant for output module interface direction
#define SERVICE_MODULE_IFC_DIRECTION   3  ///< Constant for service module interface direction

#define TCP_MODULE_IFC_TYPE   1  ///< Constant for tcp module interface type
#define UNIXSOCKET_MODULE_IFC_TYPE   2  ///< Constant for unixsocket module interface type
#define SERVICE_MODULE_IFC_TYPE   3  ///< Constant for service module interface type
#define FILE_MODULE_IFC_TYPE   4 ///< Constant for file module interface type

#define INVALID_MODULE_IFC_ATTR   -1  ///< Constant for invalid module interface attribute

/***********STRUCTURES***********/

typedef struct in_ifc_stats_s {
   uint64_t recv_msg_cnt;
   uint64_t recv_buffer_cnt;
} in_ifc_stats_t;

typedef struct out_ifc_stats_s {
   uint64_t sent_msg_cnt;
   uint64_t dropped_msg_cnt;
   uint64_t sent_buffer_cnt;
   uint64_t autoflush_cnt;
} out_ifc_stats_t;


/** Structure with information about one loaded interface of module */
typedef struct interface_s {
   char *ifc_note; ///< Interface note
   char *ifc_type; ///< Interface type (TCP / UNIXSOCKET / SERVICE)
   char *ifc_params; ///< Interface parameters (input interface ~ address, port; output interface ~ port, number of connections; service interface ~ port, number of connections)
   char *ifc_direction; ///< Interface direction (IN / OUT / SERVICE)
   int int_ifc_direction; ///< Integer value of interface direction - for faster comparison
   int int_ifc_type; ///< Integer value of interface type - for faster comparison
   void *ifc_data;
} interface_t;


/** Structure with information about one running module */
typedef struct running_module_s {
   int module_enabled; ///< TRUE if module is enabled, else FALSE.   /*** RELOAD ***/
   char *module_name; ///< Module name (loaded from config file).   /*** RELOAD ***/
   char *module_params; ///< Module parameter (loaded from config file).   /*** RELOAD ***/
   char *module_path; ///< Path to module from current directory   /*** RELOAD ***/

   interface_t   *module_ifces; ///< Array of interface_t structures with information about every loaded interface of module   /*** RELOAD ***/
   unsigned int module_ifces_cnt; ///< Number of modules loaded interfaces.   /*** RELOAD ***/
   unsigned int module_num_out_ifc; ///< Number of modules output interfaces.   /*** RELOAD ***/
   unsigned int module_num_in_ifc; ///< Number of modules input interfaces.   /*** RELOAD ***/
   int module_ifces_array_size; ///< Number of allocated interface_t structures by module.   /*** RELOAD ***/

   int module_served_by_service_thread; ///< TRUE if module was added to graph struct by sevice thread, FALSE on start.   /*** RELOAD ***/
   int module_modified_by_reload;   /*** RELOAD ***/
   char *modules_profile;   /*** RELOAD ***/
   int module_is_my_child;   /*** RELOAD ***/
   int remove_module;   /*** RELOAD ***/
   int init_module;   /*** RELOAD ***/

   int module_status; ///< Module status (TRUE ~ running, FALSE ~ stopped)   /*** SERVICE ***/
   int module_running; ///< TRUE after first start of module, else FALSE.   /*** RELOAD/ALLOCATION ***/
   int module_restart_cnt; ///< Number of module restarts.   /*** INIT ***/
   int module_restart_timer;  ///< Timer used for monitoring max number of restarts/minute.   /*** INIT ***/
   int module_max_restarts_per_minute;   /*** RELOAD ***/
   pid_t module_pid; ///< Modules process PID.   /*** RELOAD/START ***/
   int sent_sigint;   /*** INIT ***/

   unsigned int virtual_memory_usage;   /*** INIT ***/

   long int total_cpu_usage_during_module_startup;   /*** INIT ***/
   int last_period_cpu_usage_kernel_mode; ///< Percentage of CPU usage in last period in kernel mode.   /*** INIT ***/
   int last_period_cpu_usage_user_mode; ///< Percentage of CPU usage in last period in user mode.   /*** INIT ***/
   int last_period_percent_cpu_usage_kernel_mode; ///< Percentage of CPU usage in current period in kernel mode.   /*** INIT ***/
   int last_period_percent_cpu_usage_user_mode; ///< Percentage of CPU usage in current period in user mode.   /*** INIT ***/
   int overall_percent_module_cpu_usage_kernel_mode;   /*** INIT ***/
   int overall_percent_module_cpu_usage_user_mode;   /*** INIT ***/

   int module_has_service_ifc; ///< if module has service interface ~ TRUE, else ~ FALSE   /*** RELOAD ***/
   int module_service_sd; ///< Socket descriptor of the service connection.   /*** INIT ***/
   int module_service_ifc_isconnected; ///< if supervisor is connected to module ~ TRUE, else ~ FALSE   /*** INIT ***/
   int module_service_ifc_conn_attempts; // Count of supervisor's connection attempts to module's service interface    /*** INIT ***/
   int module_service_ifc_conn_fails;   /*** INIT ***/
   int module_service_ifc_conn_block;   /*** INIT ***/
   int module_service_ifc_timer;   /*** INIT ***/
} running_module_t;


typedef struct modules_profile_s modules_profile_t;

struct modules_profile_s {
   char *profile_name;
   int profile_enabled;
   modules_profile_t *next;
};


typedef struct sup_client_s {
   FILE *client_input_stream;
   FILE *client_output_stream;
   int client_input_stream_fd;
   int client_sd;
   int client_connected;
   int client_id;
   pthread_t client_thread_id;
} sup_client_t;


typedef struct server_internals_s {
   sup_client_t **clients;
   int clients_cnt;
   int server_sd;
   int daemon_terminated;
   uint16_t next_client_id;
   int config_mode_active;
   pthread_mutex_t lock;
} server_internals_t;


typedef struct reload_config_vars_s {
   xmlDocPtr doc_tree_ptr;
   xmlNodePtr current_node;
   xmlNodePtr module_elem;
   xmlNodePtr module_atr_elem;
   xmlNodePtr ifc_elem;
   xmlNodePtr ifc_atr_elem;
   int current_module_idx;
   int new_module;
   int module_ifc_insert;
   int inserted_modules;
   int removed_modules;
   int modified_modules;
} reload_config_vars_t;


typedef struct available_module_s available_module_t;
typedef struct available_modules_path_s available_modules_path_t;

struct available_modules_path_s {
   int is_valid;
   char *path;
   available_module_t *modules;
   available_modules_path_t *next;
   available_modules_path_t *prev;
};


struct available_module_s {
   char *name;
   trap_module_info_t *module_info;
   available_module_t *next;
};

union tcpip_socket_addr {
   struct addrinfo tcpip_addr; ///< used for TCPIP socket
   struct sockaddr_un unix_addr; ///< used for path of UNIX socket
};

/***********FUNCTIONS***********/

/** Function prints list of loaded modules with their params and all their interfaces with params.
 */
void interactive_show_available_modules();

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
int service_update_module_status();

/** SIGPIPE handler.
 */
void sigpipe_handler(int sig);

/** Whole program initialization; global variables, input params parsing, supervisor mode selection.
 * @param[in] argc Argc of main function.
 * @param[in] argv Argv of main function.
 * @return 0 if interactive mode, 2 if daemon mode, 1 if error.
 */
int supervisor_initialization();

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
void  supervisor_termination(int stop_all_modules, int generate_backup);

char *get_param_by_delimiter(const char *source, char **dest, const char delimiter);

long int get_total_cpu_usage();

/** Function updates running modules CPU usage - in kernel/user mode.
 * @param[in] last_total_cpu_usage Total cpu usage in last period.
 */
void update_module_cpu_usage();

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

void print_statistics(struct tm * timeinfo);

void print_statistics_legend();

/** Main routine of service thread.
 * @param[in] arg NULL.
 */
void * service_thread_routine(void* arg);

char * make_formated_statistics();

/** Function creates 2 threads - service and acceptor thread.
 */
int start_service_thread();

/** Function parses program arguments.
 * @param[in] argc Argc of main function.
 * @param[in] argv Argv of main function.
 * @return 1 if success, 0 if error.
 */
int parse_program_arguments(int *argc, char **argv);

/** Function prints a help to supervisor after executing program with -h argument.
 */
void print_help();

int get_shorter_string_length(char * first, char * second);

int find_loaded_module(char * name);

void free_module_on_index(int index);

void free_module_interfaces_on_index(int index);

int reload_configuration(const int choice, xmlNodePtr * node);



/**
 * \defgroup GROUP_NAME short_description
 *
 * TODO group description.
 * @{
 */

/**
 *
 */
char *get_absolute_file_path(char *file_name);
char *create_backup_file_path();
void create_shutdown_info(char **backup_file_path);
void print_module_ifc_stats(int module_number);
int convert_json_module_info(const char * json_str, trap_module_info_t **info);
void print_xmlDoc_to_stream(xmlDocPtr doc_ptr, FILE *stream);
struct tm *get_sys_time();
char *get_stats_formated_time();
char **make_module_arguments(const int number_of_module);
int get_number_from_input_choosing_option();
int get_numbers_from_input_dis_enable_module(int **array);
void init_module_variables(int module_number);
char *get_param_by_delimiter(const char *source, char **dest, const char delimiter);
void print_statistics(struct tm *timeinfo);
void print_statistics_legend();
char *make_formated_statistics();
int find_loaded_module(char *name);
void generate_backup_config_file();
/**@}*/



/**
 * \defgroup GROUP_NAME short_description
 *
 * TODO group description.
 * @{
 */

/**
 *
 */
void *netconf_server_routine_thread(void *arg);
int netconf_supervisor_initialization(xmlNodePtr *running);
xmlDocPtr netconf_get_state_data();
/**@}*/



 /**
 * \defgroup GROUP_NAME short_description
 *
 * TODO group description.
 * @{
 */

/**
 *
 */
void reload_process_supervisor_element(reload_config_vars_t **config_vars);
void reload_process_module_atribute(reload_config_vars_t **config_vars, char **module_ifc_atr);
int reload_process_module_interface_atribute(reload_config_vars_t **config_vars, char **module_ifc_atr);
void reload_check_modules_interfaces_count(reload_config_vars_t  **config_vars);
int reload_find_and_check_module_basic_elements(reload_config_vars_t **config_vars);
int reload_find_and_check_modules_profile_basic_elements(reload_config_vars_t **config_vars);
void reload_count_module_interfaces(reload_config_vars_t **config_vars);
void reload_check_module_allocated_interfaces(const int running_module_idx, const int ifc_cnt);
void check_running_modules_allocated_memory();
void reload_resolve_module_enabled(reload_config_vars_t **config_vars, const int modules_got_profile);
char const * sperm(__mode_t mode);
void reload_process_availablemodules_element(reload_config_vars_t **config_vars);
int reload_configuration(const int choice, xmlNodePtr *node);
void check_missing_interface_attributes();
void check_duplicated_ports();
/**@}*/



 /**
 * \defgroup GROUP_NAME short_description
 *
 * TODO group description.
 * @{
 */

/**
 *
 */
void create_output_dir();
void create_output_files_strings();
void supervisor_signal_handler(int catched_signal);
void supervisor_flags_initialization();
int supervisor_initialization();
int start_service_thread();
int parse_program_arguments(int *argc, char **argv);
/**@}*/



 /**
 * \defgroup GROUP_NAME short_description
 *
 * TODO group description.
 * @{
 */

/**
 *
 */
void free_module_on_index(int index);
void free_module_interfaces_on_index(int index);
void free_output_file_strings_and_streams();
void free_module_and_shift_array(const int module_idx);
void free_available_modules_structs();
void supervisor_termination(int stop_all_modules, int generate_backup);
/**@}*/



 /**
 * \defgroup GROUP_NAME short_description
 *
 * TODO group description.
 * @{
 */

/**
 *
 */
void interactive_show_available_modules();
int interactive_get_option();
void interactive_start_configuration();
void interactive_stop_configuration();
void interactive_set_module_enabled();
void interactive_stop_module();
void interactive_show_running_modules_status();
/**@}*/



/**
 * \defgroup service_functions Functions used by service thread
 *
 * These functions are used in service thread routine which takes care of modules
 * status (enabled modules has to run and disabled modules has to be stopped).
 * Besides, it connects to modules with service interface and receives their statistics
 * in JSON format which are decoded and saved to modules structures.
 * @{
 */

/**
 * Function checks the status of every module (whether it is running or not).
 *
 * @return Returns a number of running modules (a non-negative number).
 */
int service_check_modules_status();

/**
 * Function starts stopped modules which are enabled (they should run).
 * Part of this function is also restart limit checking of every module which is being stared.
 */
void service_update_modules_status();

/**
 * Creates a new process and executes modules binary with all needed parameters.
 * It also redirects stdout and stderr of the new process.
 *
 * @param[in] module_idx Index to array of modules (array of structures).
 */
void service_start_module(const int module_idx);

/**
 * Function cleans up after finished children processes of stopped modules.
 */
void service_clean_after_children();

/**
 * Function tries to stop running modules which are disabled using SIGINT signal.
 */
void service_stop_modules_sigint();

/**
 * Function stops running modules which are disabled using SIGKILL signal.
 * Used only if the module does not responds to SIGINT signal.
 */
void service_stop_modules_sigkill();

/**
 * Function checks connections between supervisor and service interface of every module.
 * It also checks number of errors during the connection with every module and if the limit of errors is reached,
 * it blocks the connection.
 */
void service_check_connections();

/**
 * Connects to service interface of the specified module.
 * It also checks number of connection attempts and if the limit is reached, connection is blocked.
 *
 * @param[in] module_idx Index to array of modules (array of structures).
 * @param[in] ifc_idx Index to array of modules interfaces.
 */
void service_connect_to_module(int module_idx, int ifc_idx);

/**
 * Closes the connection with service interface of the specified module.
 *
 * @param[in] module_idx Index to array of modules (array of structures).
 */
void service_disconnect_from_module(const int module_idx);

/**
 * Function receives the data from a service interface of the specified module and saves it to the output parameter data.
 *
 * @param[in] module_idx Index to array of modules (array of structures).
 * @param[in] size Number of bytes to receive.
 * @param[out] data Already allocated memory used for saving the data.
 * @return Returns 0 if success, otherwise -1.
 */
int service_recv_data(int module_idx, uint32_t size, void **data);

/**
 * Function sends the data to a service interface of the specified module.
 *
 * @param[in] module_idx Index to array of modules (array of structures).
 * @param[in] size Number of bytes to send.
 * @param[in] data Memory with the data.
 * @return Returns 0 if success, otherwise -1.
 */
int service_send_data(int module_idx, uint32_t size, void **data);

/**
 * Service thread routine is periodically checking and updating modules status
 * and connection with the modules. It also receives statistics from modules service interface.
 */
void *service_thread_routine(void *arg __attribute__ ((unused)));

/**
 * Function decodes the data in JSON format and saves the statistics to specified modules structure.
 *
 * @param[in] data Memory with the data in JSON format.
 * @param[in] module_idx Index to array of modules (array of structures).
 * @return Returns 0 if success, otherwise -1.
 */
int service_decode_module_stats(char **data, int module_idx);
/**@}*/



/**
 * \defgroup GROUP_NAME short_description
 *
 * TODO group description.
 * @{
 */

/**
 *
 */
long int get_total_cpu_usage();
void update_module_cpu_usage();
void update_module_mem_usage();
/**@}*/



/**
 * \defgroup daemon_functions Supervisor daemon mode functions
 *
 * Set of functions used by supervisor while running as a daemon process.
 * It behaves as a server and serves incoming supervisor clients and their requests.
 * The request can be: reloading configuration (reloading the configuration file),
 * receiving modules statistics (their interfaces counters) or changing configuration
 * (start / stop module, check modules status etc.).
 *
 * @{
 */

/**
 * Creates a new process and disconnects it from terminal (daemon process).
 *
 * @return Returns 0 if success, otherwise -1.
 */
int daemon_init_process();

/**
 * Allocates needed structures and variables (clients array etc.)
 *
 * @return Returns 0 if success, otherwise -1.
 */
int daemon_init_structures();

/**
 * Creates and binds a socket for incoming connections.
 *
 * @return Returns 0 if success, otherwise -1.
 */
int daemon_init_socket();

/**
 * Function initializes daemon process, structures and socket using daemon_init_{process,structures,socket} functions.
 *
 * @return Returns 0 if success, otherwise -1.
 */
int daemon_mode_initialization();

/**
 * Server routine for daemon process (acceptor thread).
 * It accepts a new incoming connection and starts a serving thread (if there is a space for another client).
 */
void daemon_mode_server_routine();

/**
 * Tries to receive initial data from a new client and reads a mode code.
 *
 * @param[in] cli Structure with clients private data.
 * @return If success, it returns mode code (client wants to configure, reload configuration or receive stats about modules).
 * Otherwise it returns negative value (-3 timeout, -2 client disconnection, -1 another error).
 */
int daemon_get_code_from_client(sup_client_t **cli);

/**
 * Function sends options to a client during configuration mode (a menu with options the client can choose from - start or stop module etc.).
 */
void daemon_send_options_to_client();

/**
 * Function opens input and output streams for a new client.
 *
 * @param[in] cli Structure with clients private data.
 * @return Returns 0 if success, otherwise -1.
 */
int daemon_open_client_streams(sup_client_t **cli);

/**
 * Function disconnects a client and makes needed clean up (closing streams etc.).
 *
 * @param[in] cli Structure with clients private data.
 */
void daemon_disconnect_client(sup_client_t *cli);

/**
 * Daemons routine for serving new incoming clients (every client gets a thread doing this routine).
 * This routine opens clients streams, receives a mode code from the client and according to the the received mode code
 * it performs required operations.
 *
 * @param[in] cli Structure with clients private data.
 */
void *daemon_serve_client_routine (void *cli);
/**@}*/

#endif
