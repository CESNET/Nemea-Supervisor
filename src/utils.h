#ifndef UTILS_H
#define UTILS_H

#include <config.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#define INSTANCES_LOGS_DIR_NAME "modules_logs"
#define SUPERVISOR_LOG_FILE_NAME "supervisor_log"

#define DEFAULT_SIZE_OF_BUFFER   100

// Constants for print_msg function and VERBOSE macro
#define N_ERR   6 ///< Verbosity level for more generic errors. This is used for error at the end of the functions in error_cleanup most of the time
#define V1   9 ///< Default verbosity level, informative messages
#define V2   10 ///< Detailed errors inside functions are printed
#define V3   11 ///< Prints even non error messages used for debugging

/**
 * @brief Macro for NULL pointer testing, freeing and setting pointer to NULL
 */
#define NULLP_TEST_AND_FREE(pointer) do { \
   if ((pointer) != NULL) { \
      free((pointer)); \
      (pointer) = NULL; \
   } \
} while (0);

/**
 * @brief Macro for printing messages to predefined streams.
 * */
#define VERBOSE(level, fmt, ...) do { \
   snprintf(verbose_msg, 4095, "%s " fmt "\n", get_formatted_time(), ##__VA_ARGS__); \
   print_msg(level, verbose_msg); \
} while (0);

/**
 * @brief Macro for printing error messages before VERBOSE macro variables are initialized.
 * */
#define PRINT_ERR(format, args...) if (1) { \
   fprintf(stderr, "[ERR] " format "\n", ##args); \
}

#define NO_MEM_ERR do { \
   VERBOSE(V2, "Failed to allocate memory in file '%s' %s:%d", \
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

// NS_TEST is defined in tests/CMakeLists.txt
#ifndef NS_TEST

#define NS_ROOT_XPATH "/nemea:supervisor"
#define NS_ROOT_XPATH_LEN 17

#endif

typedef struct vector_s {
   uint32_t capacity;
   uint32_t total;
   void **items;
} vector_t;

// VERBOSE macro variables
extern char verbose_msg[4096];
extern FILE *output_fd;
extern FILE *supervisor_log_fd;
extern uint8_t verbosity_level;

// VERBOSE macro functions
extern void print_msg(int level, char *string);
extern char * get_formatted_time();

// Vector implementation functions
extern int vector_init(vector_t *v, uint32_t size);
extern int vector_add(vector_t *v, void * item);
extern void vector_delete(vector_t *v, uint32_t index);
extern void vector_free(vector_t *v);
#endif
