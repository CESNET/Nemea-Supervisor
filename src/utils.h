/**
 * @file utils.h
 * @brief This module provides helper macros and functions shared accross the whole supervisor.
 */
#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#define INSTANCES_LOGS_DIR_NAME "modules_logs" ///< Directory of instances logs
#define SUPERVISOR_LOG_FILE_NAME "supervisor_log" ///< Directory of supervisor log
#define DEFAULT_SIZE_OF_BUFFER 100 ///< Multipurpose string buffer size. See usages

// Constants for print_msg function and VERBOSE macro
#define N_ERR 6 ///< Verbosity level for more generic errors. This is used for error at the end of the functions in error_cleanup most of the time
#define V1 9 ///< Default verbosity level, informative messages
#define V2 10 ///< Detailed errors inside functions are printed
#define V3 11 ///< Prints even non error messages used for debugging

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

/**
 * @brief Helper macro to print traceback and error message when there is no memory left
 * */
#define NO_MEM_ERR do { \
   VERBOSE(V2, "Failed to allocate memory in file '%s' %s:%d", \
         __FILE__, __func__,  __LINE__) \
} while (0);

/**
 * @brief Helper macro to test pointer for NULL, print error message and return NULL from function
 * @param ptr Pointer to test
 * */
#define IF_NO_MEM_NULL_ERR(ptr) do { \
   if ((ptr) == NULL) { \
      NO_MEM_ERR \
      return NULL; \
   } \
} while (0);

/**
 * @brief Helper macro to test pointer for NULL, print error message and return -1 from function
 * */
#define IF_NO_MEM_INT_ERR(ptr) do { \
   if ((ptr) == NULL) { \
      NO_MEM_ERR \
      return -1; \
   } \
} while (0);

/**
 * NS_TEST is defined in tests/CMakeLists.txt
 * It is used for tests so they have their own YANG schema
 * */
#ifndef NS_TEST
#define NS_ROOT_XPATH "/nemea:supervisor"
#define NS_ROOT_XPATH_LEN 17
#endif

/**
 * @brief Implementation of dynamic array
 * */
typedef struct vector_s {
   uint32_t capacity; ///< Maximum capacity of this vector
   uint32_t total; ///< Total number of items in vector
   void **items; ///< Actual dynamic array
} vector_t;

extern char verbose_msg[4096]; ///< String buffer for VERBOSE macro
extern FILE *output_fd; ///< Output file descriptor for VERBOSE macro. stdout or supervisor_log_fd is used
extern FILE *supervisor_log_fd; ///< File descriptor of supervisor's log file
extern uint8_t verbosity_level; ///< Global application's verbosity level to use

/**
 * @brief Prints given string according to rules set by level
 * @param level Level to print given string on
 * @param string String to print
 * */
extern void print_msg(int level, char *string);

/**
 * @brief Returns formatted time as string
 * @return Time as string in format [%Y-%m-%d %H:%M:%S]
 * */
extern char * get_formatted_time();

/**
 * @brief Initializes given vector
 * @param v Vector to initialize
 * @param size Capacity to set vector to
 * @return -1 on error, 0 on success
 * */
extern int vector_init(vector_t *v, uint32_t size);

/**
 * @brief Adds given item to given vector
 * @param v Vector to add item to
 * @param item Item to add to vector
 * @return -1 on error, 0 on success
 * */
extern int vector_add(vector_t *v, void * item);

/**
 * @brief Removes item at given index from vector. If needed shifts the vector to fill gap created by removal of item
 * @param v Vector to remove item from
 * @param index Index at which the item to be removed is
 * */
extern void vector_delete(vector_t *v, uint32_t index);

/**
 * @brief Resets vector to default values and clears items array
 * @param v Vector to free
 * */
extern void vector_free(vector_t *v);
#endif
