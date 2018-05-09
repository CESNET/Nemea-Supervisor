/**
 * @file module.h
 * @brief Contains internal structures of all available modules and instances, functions for allocation/release of internal structures and also dynamic arrays that contain lists of available modules and instances that are part of the running configuration.
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

/**
 * @brief Direction of module interface
 * */
typedef enum interface_dir_e {
   NS_IF_DIR_IN,     ///< Input module interface direction
   NS_IF_DIR_OUT,    ///< Output module interface direction
} interface_dir_t;

/**
 * @brief Type of instance's interface
 * */
typedef enum interface_type_e {
   NS_IF_TYPE_TCP,     ///< TCP
   NS_IF_TYPE_TCP_TLS, ///< TCP TLS
   NS_IF_TYPE_UNIX,    ///< UNIX socket
   NS_IF_TYPE_FILE,    ///< file
   NS_IF_TYPE_BH       ///< blackhole
} interface_type_t;

/**
 * @brief Interface statistics for IN direction
 * */
typedef struct ifc_in_stats_s {
   uint8_t state;
   char type;
   char *id;
   uint64_t recv_msg_cnt;
   uint64_t recv_buff_cnt;
} ifc_in_stats_t;

/**
 * @brief Interface statistics for OUT direction
 * */
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
 * @details Parameters specific to TCP socket interface type. Some parameters are only available
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
 * @details Parameters specific to TCP-TLS socket interface type. Some parameters are only available
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
 * @details Parameters specific to UNIX socket interface type. Some parameters are only available
 *  for IN or OUT interface type. For more information see libtrap documentation.
 * */
typedef struct unix_ifc_params_s {
   uint16_t max_clients; ///< Maximal number of connected clients (input interfaces).
                         ///<  Can be set only for OUT interfaces.
   char *socket_name; ///< Name of UNIX socket to connect to.
} unix_ifc_params_t;

/**
 * @brief Parameters specific to FILE interface type.
 * @details Parameters specific to FILE interface type. Some parameters are only available
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
 * @details This union holds parameters specific to interface of given type. Only one set
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
   void *stats; ///< Pointer to in_ifc_stats_t or out_ifc_stats_t depending on whether
                ///<  this is in/out iface
} interface_t;

/**
 * @brief Structure that holds available module.
 * */
typedef struct av_module_s {
   char * name; ///< Name of this module
   char * path; ///< Path to executable file
   bool sr_rdy; ///< Is module sysrepo ready?
   bool trap_mon; ///< Is module monitorable via TRAP's service interface?
   bool trap_ifces_cli; ///< Is passing TRAP interfaces params at CLI?
} av_module_t;

/**
 * @brief Structure that holds an instance.
 * */
typedef struct inst_s {
   vector_t in_ifces; ///< Vector of IN interfaces
   vector_t out_ifces; ///< Vector of OUT interfaces

   bool enabled; ///< Specifies whether module is enabled.
   bool use_sysrepo; ///< Specifies whether to use sysrepo. This option can be true only to sysrepo ready modules
   bool is_my_child; ///< Specifies whether supervisor started this module.
   char *name; ///< Module name (loaded from config file).
   char *params; ///< Module parameter (loaded from config file).
   char **exec_args; ///< Array of arguments to execv function. Module name at first
                     ///<  place inside the array and NULL at the last, e.g.
                     ///<  ["module_name", "-a", "blah", NULL]


   av_module_t *mod_ref; ///< Module executable of this process
   bool root_perm_needed; ///< Does module require root permissions?
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
} inst_t;

extern pthread_mutex_t config_lock;
extern vector_t avmods_v;
extern vector_t insts_v;


/**
 * @brief Adds given interface to given instance.
 * @param inst Instance
 * @param ifc Interface
 * @return 0 on success or -1 on error
 * */
extern int inst_interface_add(inst_t *inst, interface_t *ifc);

/**
 * @brief Prints debug information about given module.
 * @param mod Module
 * */
extern void av_module_print(const av_module_t *mod);

/**
 * @brief Prints debug information about given instance.
 * @param inst Instance
 * */
extern void inst_print(inst_t *inst);

/**
 * @brief Prints debug information about given interface.
 * @param ifc Interface
 * */
extern void print_ifc(interface_t *ifc);

/**
 * @brief Frees the avmods_v array of available modules.
 * */
extern void av_modules_free();

/**
 * @brief Frees the insts_v array of instances.
 * */
extern void insts_free();

/**
 * @brief Frees single given instance.
 * @param inst Instance to free
 * */
extern void inst_free(inst_t *inst);

/**
 * @brief Frees single given modules.
 * @param mod Module to free
 * */
extern void av_module_free(av_module_t *mod);

/**
 * @brief Frees and NULLs all interfaces of given instance
 * @param inst Pointer to instance which interfaces are going to be freed.
 * */
extern void interfaces_free(inst_t *inst);

/**
 * @brief Frees single given interface
 * @param ifc Pointer to interface that should be freed.
 * */
extern void interface_free(interface_t *ifc);

/**
 * @brief Frees interface stats structures of given interface
 * @param ifc Pointer to interface that should have stats freed.
 * */
extern void interface_stats_free(interface_t *ifc);

/**
 * @brief Allocates new av_module_t
 * @return Pointer to new av_module_t or NULL on error
 * */
av_module_t * av_module_alloc();

/**
 * @brief Allocates new inst_t
 * @return Pointer to new inst_t or NULL on error
 * */
extern inst_t * inst_alloc();


/**
 * @brief Allocates new interface_t
 * @return Pointer to new interface_t or NULL on error
 * */
extern interface_t * interface_alloc();

/**
 * @brief Allocates and assigns interface stats for given interface
 * @param ifc Interface to use
 * @return 0 on success or -1 on error
 * */
extern int interface_stats_alloc(interface_t *ifc);

/**
 * @brief Allocates structures for specific interface params and assings it to given ifc
 * @param ifc Interface to use
 * @return 0 on success or -1 on error
 * */
extern int interface_specific_params_alloc(interface_t *ifc);

/**
 * @brief Loads exec_args from instance parameters and interfaces.
 * @param inst Instance for which exec_args should be loaded.
 * @return 0 on success or -1 on error
 * */
extern int inst_gen_exec_args(inst_t *inst);

/**
 * @brief Finds instance by it's name inside insts_v and fills it's index inside
 *  vector to index parameter.
 * @param name Name of instance to find
 * @param index[out] Found index inside insts_v vector. It's not filled if inst is not found.
 * @return Pointer to found intance or NULL if not found.
 * */
extern inst_t * inst_get_by_name(const char *name, uint32_t *index);

/**
 * @brief Clear UNIX socket files left after killed instance.
 * @param inst Instance after which the socket files should be cleaned.
 * */
extern void inst_clear_socks(inst_t *inst);

#endif //NEMEA_SUPERVISOR_MODULE_H
