/**
 * @file main.h
 * @brief TODO
 * */

#include <getopt.h>
#include "conf.h"
#include "supervisor.h"

#define USAGE_MSG "Usage:  supervisor  MANDATORY  [OPTIONAL]...\n"\
                  "   MANDATORY parameters:\n"\
                  "      -L, --logs-path=path   "\
                  "Path of the directory where the logs (both supervisor's and modules') will be saved.\n"\
                  "   OPTIONAL parameters:\n"\
                  "      [-d, --daemon]   Runs supervisor as a system daemon.\n"\
                  "      [-v, --verbosity=level]   Verbosity to use. Levels are 0-3, 1 is default..\n"\
                  "      [-h, --help]   Prints this help.\n"\
                  "Path of the unix socket which is used for supervisor daemon and client communication.\n"\

/**
 * @details Function parses program arguments using SUP_GETOPT macro (it is set
 *  by configure script to getopt or getopt_long function according to the
 *  available system libraries).
 *
 * @param[in] argc Argument counter passed from the main function.
 * @param[in] argv Argument values passed from the main function.
 * @return DAEMON_MODE_CODE or INTERACTIVE_MODE_CODE in case of success, otherwise -1.
 * */
int parse_prog_args(int argc, char **argv);


int parse_prog_args(int argc, char **argv)
{
   static struct option long_options[] = {
      {"logs-path",  required_argument, 0, 'L'},
      {"verbosity",  required_argument, 0, 'v'},
      {"daemon", no_argument, 0, 'd'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}
   };

   int c = 0;

   while (1) {
      c = getopt_long(argc, argv, "L:dv:h", long_options, NULL);
      if (c == -1) {
         break;
      }

      switch (c) {
         case ':':
            PRINT_ERR("Wrong arguments, use 'supervisor -h' for help.")
            return -1;
         case '?':
            PRINT_ERR("Unknown option, use 'supervisor -h' for help.")
            return -1;
         case 'h':
            printf(USAGE_MSG);
            return -1;
         case 'v':
            if (optarg == NULL) {
               PRINT_ERR("Invalid verbosity level.")
               PRINT_ERR(USAGE_MSG);
               return -1;
            }
            switch (optarg[0]) {
               case '0':
                  verbosity_level = 0;
                  break;
               case '1':
                  verbosity_level = V1;
                  break;
               case '2':
                  verbosity_level = V2;
                  break;
               case '3':
                  verbosity_level = V3;
                  break;
               default:
                  PRINT_ERR("Invalid verbosity level.")
                  PRINT_ERR(USAGE_MSG);
                  return -1;
            }
            break;
         case 'd':
            daemon_flag = true;
            break;
         case 'L':
            logs_path = strdup(optarg);
            IF_NO_MEM_INT_ERR(logs_path)
            break;
         default:
            PRINT_ERR("Invalid option '%c'.", c)
            return -1;

      }
   }

   if (logs_path == NULL) {
      PRINT_ERR("Missing required logs directory path.")
      PRINT_ERR(USAGE_MSG);
      return -1;
   }

   return daemon_flag;
}



int main (int argc, char *argv [])
{
   if (parse_prog_args(argc, argv) == -1) {
      NULLP_TEST_AND_FREE(logs_path);
      exit(EXIT_FAILURE);
   }


   if (init_paths() == -1) {
      terminate_supervisor(false);
      exit(EXIT_FAILURE);
   }

   VERBOSE(V1, "======= STARTING =======")

   if (daemon_flag) {
      // Initialize a new daemon process
      fflush(stdout);
      if (daemon_init_process() == 0) {
         VERBOSE(V3, "Logs path: %s", logs_path);
         VERBOSE(V3, "Daemon mode: %d", daemon_flag);

         // Initialize supervisor's structures, service thread, 
         //  output, signal handler and load startup configuration
         if (supervisor_initialization() != -1) {
            supervisor_routine();
         }
      }
   } else {
      VERBOSE(V3, "Starting in debug mode")
      if (supervisor_initialization() != -1) {
         supervisor_routine();
      }
   }

   // TODO revisit
   // Kill all instances and cleanup all structures
   terminate_supervisor(true);

   exit(EXIT_SUCCESS);
}
