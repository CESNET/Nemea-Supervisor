#ifndef UTILS_H
#define UTILS_H

#include <config.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

/* Logging & helper functions */

 /*--BEGIN superglobal symbolic const--*/

#define INSTANCES_LOGS_DIR_NAME "modules_logs"
#define INSTANCES_STATS_FILE_NAME "modules_statistics"
#define INSTANCES_EVENTS_FILE_NAME "modules_events"
#define SUPERVISOR_DEBUG_LOG_FILE_NAME "supervisor_debug_log"
#define SUPERVISOR_LOG_FILE_NAME "supervisor_log"
#define SUP_TMP_DIR "/tmp/sup_tmp_dir"

#define TRUE 1
#define FALSE 0
#define DEFAULT_SIZE_OF_BUFFER   100

// Constants for print_msg function and VERBOSE macro
#define STATISTICS   1
#define MOD_EVNT   2
#define N_STDOUT   3
#define DEBUG   4
#define SUP_LOG   5
#define N_ERR   6
#define N_WARN   7
#define N_INFO   8
#define V1   9
#define V2   10
#define V3   11


 /*--END superglobal symbolic const--*/

 /*--BEGIN macros--*/

/**
 * @brief Macro for NULL pointer testing, freeing and setting pointer to NULL
 */
#define NULLP_TEST_AND_FREE(pointer) do { \
   if (pointer != NULL) { \
      free(pointer); \
      pointer = NULL; \
   } \
} while (0);

/**
 * @brief Macro for printing messages to predefined streams.
 * */
#define VERBOSE(level, fmt, ...) do { \
   snprintf(verbose_msg, 4095, "%s " fmt "\n", get_formatted_time(), ##__VA_ARGS__); \
   print_msg(level, verbose_msg); \
} while (0);

#define PRINT_ERR(format, args...) if (1) { \
   fprintf(stderr, "[ERROR] " format "\n", ##args); \
}

#define NO_MEM_ERR do { \
   VERBOSE(N_ERR, "Failed to allocate memory in %s %s:%d", \
         __FILE__, __func__,  __LINE__) \
} while (0);

#define IF_NO_MEM_NULL_ERR(ptr) do { \
   if ((ptr) == NULL) { \
      NO_MEM_ERR \
      return NULL; \
   } \
} while (0);

#define IF_NO_MEM_INT_ERR(ptr) do { \
   if ((ptr) == NULL) { \
      NO_MEM_ERR \
      return -1; \
   } \
} while (0);


/**
 * @brief TODO poradny, vsude zkontrolovat jestli je ve vec neco, jinak to spadne
 * @details (vec).items[i] gets implicitly type casted to iter type.
 * */
#define FOR_EACH_IN_VEC(vec, iter) for (uint32_t i = 0; \
                                 (iter) = (vec).items[i], \
                                 i < (vec).total; \
                                i++)

#define FOR_EACH_IN_VEC_PTR(vec, iter) for (uint32_t i = 0; \
                                 (iter) = (vec)->items[i], \
                                 i < (vec)->total; \
                                i++)

#ifndef TEST

#define NS_ROOT_XPATH "/nemea:supervisor"
#define NS_ROOT_XPATH_LEN 17

#endif
 /*--END macros--*/

 /*--BEGIN superglobal typedef--*/
 typedef struct vector_s {
    uint32_t capacity;
    uint32_t total;
    void **items;
 } vector_t;
 /*--END superglobal typedef--*/

 /*--BEGIN superglobal vars--*/

// VERBOSE macro variables
// TODO struct? yes
extern char verbose_msg[4096];
extern FILE *output_fd;
extern FILE *supervisor_debug_log_fd;
extern FILE *supervisor_log_fd;
extern FILE *statistics_fd;
extern FILE *inst_event_fd;
 /*--END superglobal vars--*/

 /*--BEGIN superglobal fn prototypes--*/
extern void print_msg(int level, char *string);
extern char * get_formatted_time();


extern int vector_init(vector_t *v, uint32_t size);
extern int vector_add(vector_t *v, void * item);
extern void vector_delete(vector_t *v, uint32_t index);
extern void vector_free(vector_t *v);
extern void vector_set(vector_t *v, uint32_t index, void *item);
extern void * vector_get(vector_t *v, uint32_t index);
 /*--END superglobal fn prototypes--*/

#endif
