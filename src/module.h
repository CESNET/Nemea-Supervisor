/**
 * @file module.h
 * @brief Module & module group structures, handling functions and vectors.
 */

#ifndef NEMEA_SUPERVISOR_MODULE_H
#define NEMEA_SUPERVISOR_MODULE_H

#include <inttypes.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>
#include "utils.h"

/*--BEGIN superglobal symbolic const--*/
/*--END superglobal symbolic const--*/

/*--BEGIN macros--*/
/*--END macros--*/

/*--BEGIN superglobal typedef--*/


/*typedef struct tree_path_s {
   char *group;
   char *module;
   char *inst;
   char *ifc;
} tree_path_t;*/

typedef enum ns_fn_success_e {
   NS_ERR = -1,
   NS_OK = 0
} ns_fn_success_t;

/**
 * @brief Direction of module interface
 * */
typedef enum interface_dir_e {
   NS_IF_DIR_IN,     ///< Input module interface direction
   NS_IF_DIR_OUT,    ///< Output module interface direction
} interface_dir_t;

/**
 * @brief Type of module interface
 * */
typedef enum interface_type_e {
   NS_IF_TYPE_TCP,     ///< TCP
   NS_IF_TYPE_TCP_TLS, ///< TCP TLS
   NS_IF_TYPE_UNIX,    ///< UNIX socket
   NS_IF_TYPE_FILE,    ///< file
   NS_IF_TYPE_BH       ///< blackhole
} interface_type_t;


typedef struct ifc_in_stats_s {
   uint8_t state;
   char type;
   char *id;
   uint64_t recv_msg_cnt;
   uint64_t recv_buff_cnt;
} ifc_in_stats_t;

typedef struct ifc_out_stats_s {
   int32_t num_clients;
   char type;
   char *id;
   uint64_t sent_msg_cnt;
   uint64_t sent_buff_cnt;
   uint64_t dropped_msg_cnt;
   uint64_t autoflush_cnt;
} ifc_out_stats_t;


/**
 * @brief Parameters specific to TCP interface type.
 *
 * Parameters specific to TCP socket interface type. Some parameters are only available
 *  for IN or OUT interface type. For more information see libtrap documentation.
 * */
typedef struct tcp_ifc_params_s {
   char *host; ///< Hostname or IP address identifying TCP socket.
   uint16_t port; ///< Port number indetifying TCP socket.
   uint16_t max_clients; ///< Maximal number of connected clients (input interfaces).
   ///<  Is set only for OUT interfaces.
} tcp_ifc_params_t;

/**
 * @brief Parameters specific to TCP-TLS interface type.
 *
 * Parameters specific to TCP-TLS socket interface type. Some parameters are only available
 *  for IN or OUT interface type. For more information see libtrap documentation.
 * */
typedef struct tcp_tls_ifc_params_s {
   char *host; ///< Hostname or IP address identifying TCP socket.
   uint16_t port; ///< Port number indetifying TCP socket.
   uint16_t max_clients; ///< Maximal number of connected clients (input interfaces).
   ///<  Can be set only for OUT interfaces.
   char *keyfile; ///< Path to private key file in PEM format.
   char *certfile; ///< Path to certificate file in PEM format.
   char *cafile; ///< Path to CA certificate file in PEM format.
} tcp_tls_ifc_params_t;

/**
 * @brief Parameters specific to UNIX socket interface type.
 *
 * Parameters specific to UNIX socket interface type. Some parameters are only available
 *  for IN or OUT interface type. For more information see libtrap documentation.
 * */
typedef struct unix_ifc_params_s {
   uint16_t max_clients; ///< Maximal number of connected clients (input interfaces).
                         ///<  Can be set only for OUT interfaces.
   char *socket_name; ///< Name of UNIX socket to connect to.
} unix_ifc_params_t;

/**
 * @brief Parameters specific to FILE interface type.
 *
 * Parameters specific to FILE interface type. Some parameters are only available
 *  for IN or OUT interface type. For more information see libtrap documentation.
 * */
typedef struct file_ifc_params_s {
   char *name; ///< Name of file to be used as interface.
   char *mode; ///< Defines file access mode. Is set only for OUT interfaces.
   uint16_t time; ///< Interface splits files as often as this value specifies.
                  ///<  Is set only for OUT interfaces.
   uint16_t size; ///< Interface splits files by the size this value specifies.
                  ///<  Is set only for OUT interfaces.
} file_ifc_params_t;

/**
 * @brief Parameters specific to interface of given type.
 *
 * This union holds parameters specific to interface of given type. Only one set
 *  of parameters can be stored though there is no distinction between IN and OUT
 *  interface types. Parameters specific for IN or OUT are merged inside structured
 *  linked by this union.
 * */
typedef union specific_ifc_params_u {
   tcp_ifc_params_t *tcp; ///< Parameters specific to TCP interface type
   tcp_tls_ifc_params_t *tcp_tls; ///< Parameters specific to TCP-TLS interface type
   unix_ifc_params_t *nix; ///< Parameters specific to UNIX socket interface type
   file_ifc_params_t *file; ///< Parameters specific to FILE interface type
} specific_ifc_params_t;

/**
 * @brief Contains information about one loaded interface of module.
 * */
typedef struct interface_s {
   char *name; ///< Interface name
   char *buffer; ///< Specifies buffering of data and whether to send in larger
                 ///<  bulks (increases troughput).
   char *autoflush; ///< Normally data are not sent until the buffer is full. When
                    ///<  autoflush is enabled, even non-full buffers are sent every X microseconds.
   char *timeout; ///< Time in microseconds that an IFC can block waiting for message send/receive.
   interface_dir_t direction; ///< Enum of interface direction - for faster comparison
   interface_type_t type; ///< Enum of interface type - for faster comparison
   specific_ifc_params_t specific_params; ///< Parameters specific for given interaface type.
   char * (*ifc_to_cli_arg_fn)(const struct interface_s *); ///< Pointer to function that converts
                                                          ///<  interface to CLI arguments string
                                                          ///<  and is specific for given
                                                          ///<  interface type.

   //struct interface_s *next_ifc; ///< Next interface of any direction type
   //struct interface_s *next_sd_ifc; ///< Next interface of same direction type as this interface
   void *stats; ///< Pointer to in_ifc_stats_t or out_ifc_stats_t depending on whether
                ///<  this is in/out iface
} interface_t;

/*
typedef struct module_group_s {
   char *name; ///< Name of module group
   //uint16_t mods_cnt; ///< Number of owned modules
   //uint16_t insts_cnt; ///< Number of owned module instances
   bool enabled; ///< Is module group enabled?
} module_group_t;

typedef struct module_s {
   char *name; ///< Name of module
   char *path; ///< Absolute path to module executable directory
   //vector_t insts; ///< Instances of this module
   module_group_t *group; ///< Module group where this module belongs
} module_t;

typedef struct instance_s {
   vector_t in_ifces; ///< Vector of IN interfaces
   vector_t out_ifces; ///< Vector of OUT interfaces

   // These were used as arrays, i'm placing this inside each interface
   //ifc_in_stats_t *in_ifces_data;  ///< Contains statistics about all input interfaces
                                   ///<  the module is running with (size of total_in_ifces_cnt)
   //ifc_out_stats_t *out_ifces_data;  ///< Contains statistics about all output interfaces
                                     ///<  the module is running with (size of total_out_ifces_cnt)

   //uint32_t ifces_cnt;  ///< Number of interfaces loaded from the configuration file.
   //TODO uint32_t config_ifces_arr_size;  ///< Size of allocated array for interfaces loaded from the configuration file (array "config_ifces").


   bool enabled; ///< Specifies whether module is enabled.
   bool is_my_child; ///< Specifies whether supervisor started this module.
   char *name; ///< Module name (loaded from config file).
   char *params; ///< Module parameter (loaded from config file).
   char **exec_args; ///< Array of arguments to execv function. Module name at first
                     ///<  place inside the array and NULL at the last, e.g.
                     ///<  ["module_name", "-a", "blah", NULL]


   //TODO int module_served_by_service_thread; ///< TRUE if module was added to graph struct by sevice thread, FALSE on start.
   //TODO uint8_t module_modified_by_reload; ///< Variable used during reload_configuration, TRUE if already loaded module is changed by reload, else FALSE
   //TODO uint8_t module_checked_by_reload; ///< Variable used during reload_configuration, TRUE if a new module is added or already loaded module is checked (used for excluding modules_ll with non-unique name)
   module_t *module; ///< Module to which this instance bellongs to
   //TODO int module_is_my_child;
   bool root_perm_needed; ///< Does module require root permissions? TODO what for??
   //TODO int remove_module; // resi jestli se ma modul odstranit, protoze neni v novy konfiguraci
   //TODO int init_module;   // resi jestli se ma modul znovu nastavit kvuli novy konfiguraci

   bool running; ///< Is module running?
   bool should_die; ///< Whether instance is marked for kill
   bool sigint_sent; ///< Specifies whether SIGKILL was sent to module.
   pid_t pid; ///< Module process PID.
   uint8_t restarts_cnt; ///< Number of attempts at starting the module in last
                         ///<  minute from restarts_timer
   uint8_t max_restarts_minute; ///< Maximum number of restarts per minute
   time_t restart_time; ///< Time used for monitoring max number of restarts/minute.

   uint64_t mem_vms;  ///< Loaded from /proc/PID/stat in B
   uint64_t mem_rss;  ///< Loaded from /proc/PID/status in kB
   uint64_t last_cpu_perc_kmode; ///< Percentage of CPU usage in last period in kernel mode.
   uint64_t last_cpu_kmode; ///< CPU usage in last period in kernel mode.
   uint64_t last_cpu_perc_umode; ///< Percentage of CPU usage in last period in user mode.
   uint64_t last_cpu_umode; ///< CPU usage in last period in user mode.

   int service_sd; ///< Socket descriptor of the service connection or -1 if not set
   bool service_ifc_connected; ///< Is supervisor connected to module?
   uint64_t service_ifc_conn_timer; ///< Indicator of whether to attempt to connect to service ifc.
                                    ///<  Gets incremented once every supervisor_routine loop to
                                    ///<  connect each NUM_SERVICE_IFC_PERIOD
   //TODO dunno what for bool has_service_ifc; ///< Indicator of whether module has service interface (socket)
} instance_t;
*/

typedef struct av_module_s {
   char * name;
   char * path;
   bool is_sr_en;
   bool is_nemea;
} av_module_t;

typedef struct run_module_s {
   vector_t in_ifces; ///< Vector of IN interfaces
   vector_t out_ifces; ///< Vector of OUT interfaces

   // These were used as arrays, i'm placing this inside each interface
   //ifc_in_stats_t *in_ifces_data;  ///< Contains statistics about all input interfaces
   ///<  the module is running with (size of total_in_ifces_cnt)
   //ifc_out_stats_t *out_ifces_data;  ///< Contains statistics about all output interfaces
   ///<  the module is running with (size of total_out_ifces_cnt)

   //uint32_t ifces_cnt;  ///< Number of interfaces loaded from the configuration file.
   //TODO uint32_t config_ifces_arr_size;  ///< Size of allocated array for interfaces loaded from the configuration file (array "config_ifces").


   bool enabled; ///< Specifies whether module is enabled.
   bool is_my_child; ///< Specifies whether supervisor started this module.
   char *name; ///< Module name (loaded from config file).
   char *params; ///< Module parameter (loaded from config file).
   char **exec_args; ///< Array of arguments to execv function. Module name at first
                     ///<  place inside the array and NULL at the last, e.g.
                     ///<  ["module_name", "-a", "blah", NULL]


   //TODO int module_served_by_service_thread; ///< TRUE if module was added to graph struct by sevice thread, FALSE on start.
   //TODO uint8_t module_modified_by_reload; ///< Variable used during reload_configuration, TRUE if already loaded module is changed by reload, else FALSE
   //TODO uint8_t module_checked_by_reload; ///< Variable used during reload_configuration, TRUE if a new module is added or already loaded module is checked (used for excluding modules_ll with non-unique name)
   av_module_t *mod_kind; ///< Module executable of this process
   //TODO int module_is_my_child;
   bool root_perm_needed; ///< Does module require root permissions? TODO what for??
   //TODO int remove_module; // resi jestli se ma modul odstranit, protoze neni v novy konfiguraci
   //TODO int init_module;   // resi jestli se ma modul znovu nastavit kvuli novy konfiguraci

   bool running; ///< Is module running?
   bool should_die; ///< Whether instance is marked for kill
   bool sigint_sent; ///< Specifies whether SIGKILL was sent to module.
   pid_t pid; ///< Module process PID.
   uint8_t restarts_cnt; ///< Number of attempts at starting the module in last
                         ///<  minute from restarts_timer
   uint8_t max_restarts_minute; ///< Maximum number of restarts per minute
   time_t restart_time; ///< Time used for monitoring max number of restarts/minute.

   uint64_t mem_vms;  ///< Loaded from /proc/PID/stat in B
   uint64_t mem_rss;  ///< Loaded from /proc/PID/status in kB
   uint64_t last_cpu_perc_kmode; ///< Percentage of CPU usage in last period in kernel mode.
   uint64_t last_cpu_kmode; ///< CPU usage in last period in kernel mode.
   uint64_t last_cpu_perc_umode; ///< Percentage of CPU usage in last period in user mode.
   uint64_t last_cpu_umode; ///< CPU usage in last period in user mode.

   int service_sd; ///< Socket descriptor of the service connection or -1 if not set
   bool service_ifc_connected; ///< Is supervisor connected to module?
   uint64_t service_ifc_conn_timer; ///< Indicator of whether to attempt to connect to service ifc.
   ///<  Gets incremented once every supervisor_routine loop to
   ///<  connect each NUM_SERVICE_IFC_PERIOD
   //TODO dunno what for bool has_service_ifc; ///< Indicator of whether module has service interface (socket)
} run_module_t;
/*--END superglobal typedef--*/

/*--BEGIN superglobal vars--*/
extern pthread_mutex_t config_lock;
extern vector_t avmods_v;
extern vector_t rnmods_v;

/*--END superglobal vars--*/

/*--BEGIN superglobal fn prototypes--*/


extern int run_module_interface_add(run_module_t *inst, interface_t *ifc);
/**
 * TODO
 * */
extern void av_module_print(const av_module_t *mod);
/**
 * TODO
 * */
extern void run_module_print(run_module_t *inst);

/**
 * TODO
 * */
extern void print_ifc(interface_t *ifc);

/**
 * TODO
 * */
extern void av_modules_free();

/**
 * @brief Frees module_t linked list from given parameter and
 *  sets given *module_ll_head to NULL
 * @see free_module_groups(module_group_t **group_ll_head)
 * @param module_ll_head First node from which the rest of the linked list is
 *  going to be freed
 * */
extern void run_modules_free();

// TODO
extern void run_module_free(run_module_t *inst);

// TODO
extern void av_module_free(av_module_t *mod);

/**
 * @brief Frees and NULLs all interfaces of given module
 * @param module Pointer to module which interfaces are going to be freed.
 * */
extern void interfaces_free(run_module_t *module);

/**
 * TODO
 * */
extern void interface_stats_free(interface_t *ifc);


av_module_t * av_module_alloc();
/**
 * @brief Dynamically allocates new module_t structure and fills it with default values.
 * @return Pointer to newly allocated module or NULL on failure
 * */
extern run_module_t * run_module_alloc();

/**
 * TODO
 * */
extern interface_t * interface_alloc();

/**
 * TODO
 * */
extern int interface_stats_alloc(interface_t *ifc);

/**
 * TODO
 * */
extern int interface_specific_params_alloc(interface_t *ifc);

/**
 * @brief Loads exec_args from module parameters and interfaces.
 * @param inst Module for which exec_args should be loaded.
 * @return TODO status
 * */
extern int run_module_gen_exec_args(run_module_t *inst);

/**
 * @brief Finds module by it's name inside mods_v and fills it's index inside
 *  vector to index parameter.
 * @param name Name of module to find
 * @param index Found index inside mods_v vector. It's not filled if module is not found.
 * @return Pointer to found module or NULL if not found.
 * */
extern run_module_t * run_module_get_by_name(const char *name, uint32_t *index);

/**
 * @brief Clear UNIX socket files left after killed module.
 * @param inst Module after which the socket files should be cleaned.
 * */
extern void run_module_clear_socks(run_module_t *inst);

/*--END superglobal fn prototypes--*/

#endif //NEMEA_SUPERVISOR_MODULE_H
