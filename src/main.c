
#include <getopt.h>
#include "conf.h"
#include "supervisor.h"

/*--BEGIN superglobal vars--*/
/*--END superglobal vars--*/

/*--BEGIN local #define--*/
#define USAGE_MSG "Usage:  supervisor  MANDATORY...  [OPTIONAL]...\n"\
                  "   MANDATORY parameters:\n"\
                  "      -L, --logs-path=path   "\
                  "Path of the directory where the logs (both supervisor's and modules') will be saved.\n"\
                  "   OPTIONAL parameters:\n"\
                  "      [-d, --daemon]   Runs supervisor as a system daemon.\n"\
                  "      [-h, --help]   Prints this help.\n"\
                  "Path of the unix socket which is used for supervisor daemon and client communication.\n"\

/*--END local #define--*/

/*--BEGIN local typedef--*/
/*--END local typedef--*/

/* --BEGIN local vars-- */
/* --END local vars-- */

/* --BEGIN full fns prototypes-- */
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
/* --END full fns prototypes-- */


/* --BEGIN local fns-- */
int parse_prog_args(int argc, char **argv)
{
   static struct option long_options[] = {
      {"daemon", no_argument, 0, 'd'},
      {"config-template", required_argument, 0, 'T'},
      {"configs-path",  required_argument,    0, 'C'},
      {"help", no_argument,           0,  'h' },
      {"logs-path",  required_argument,  0, 'L'},
      {0, 0, 0, 0}
   };

   int c = 0;

   while (1) {
      c = getopt_long(argc, argv, "dC:T:hs:L:", long_options, NULL);
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
      fprintf(stderr, USAGE_MSG);
      return -1;
   }

   return daemon_flag;
}
/* --END local fns-- */




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
   VERBOSE(DEBUG, "======= STARTING =======")
   // TODO verbosity arguments
   // if argv.contains(-vvv) ->
   //sr_log_stderr(SR_LL_DBG);

   if (daemon_flag) {
      // Initialize a new daemon process
      fflush(stdout); // TODO why?
      if (daemon_init_process() == 0) {

VERBOSE(DEBUG, "logs path: %s", logs_path);
VERBOSE(DEBUG, "daemon mode: %d", daemon_flag);

         // Initialize supervisor's structures, service thread, 
         //  output, signal handler and load startup configuration

         if (supervisor_initialization() != -1) {
            supervisor_routine();
         }
      }
   } else {
      VERBOSE(DEBUG, "Starting in debug mode")
      if (supervisor_initialization() != -1) {
         supervisor_routine();
      }
   }

   // TODO revisit
   // Kill all instances and cleanup all structures
   terminate_supervisor(true);

   exit(EXIT_SUCCESS);
}
