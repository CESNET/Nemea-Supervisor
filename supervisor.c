/**
 * \file supervisor.c
 * \brief Supervisor implementation.
 * \author Marek Svepes <svepemar@fit.cvut.cz>
 * \author Tomas Cejka <cejkat@cesnet.cz>
 * \date 2013
 * \date 2014
 */
/*
 * Copyright (C) 2013-2014 CESNET
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

#ifdef nemea_plugin
   #include "./ncnemea/ncnemea.h"
#endif

#include "supervisor.h"
#include "internal.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <ifaddrs.h>

#define TRAP_PARAM               "-i" ///< Interface parameter for libtrap
#define DEFAULT_MAX_RESTARTS_PER_MINUTE  3  ///< Maximum number of module restarts per minute
#define SERVICE_IFC_CONN_ATTEMPTS_LIMIT 3 // Maximum count of connection attempts to service interface

#define MODULES_UNIXSOCKET_PATH_FILENAME_FORMAT   "/tmp/trap-localhost-%s.sock" ///< Modules output interfaces socket, to which connects service thread.
#define DEFAULT_DAEMON_UNIXSOCKET_PATH_FILENAME  "/tmp/supervisor_daemon.sock"  ///<  Daemon socket.

#define NC_DEFAULT_LOGSDIR_PATH "/tmp/supervisor_logs/"
#define DEFAULT_BACKUP_FILE_PREFIX "sup_backup"

#define RET_ERROR    -1

/*******GLOBAL VARIABLES*******/
running_module_t *   running_modules = NULL;  ///< Information about running modules

unsigned int         running_modules_array_size = 0;  ///< Current size of running_modules array.
unsigned int         loaded_modules_cnt = 0; ///< Current number of loaded modules.

pthread_mutex_t running_modules_lock; ///< mutex for locking counters
int         service_thread_continue = FALSE; ///< condition variable of main loop of the service_thread
int         max_restarts_per_minute_config = DEFAULT_MAX_RESTARTS_PER_MINUTE;

modules_profile_t * first_profile_ptr = NULL;
modules_profile_t * actual_profile_ptr = NULL;

pthread_t   service_thread_id; ///< Service thread identificator.
pthread_t   nc_clients_thread_id;

struct tm * init_time_info = NULL;
int service_stop_all_modules = FALSE;

// supervisor flags
int      file_flag = FALSE;        // -f "file" arguments
int      verbose_flag = FALSE;     // -v messages and cpu_usage stats
int      daemon_flag = FALSE;      // --daemon
int      netconf_flag = FALSE;
char *   config_file = NULL;
char *   socket_path = NULL;
char *   logs_path = NULL;

char *   statistics_file_path = NULL;
char *   module_event_file_path = NULL;
char *   supervisor_debug_log_file_path = NULL;
char *   supervisor_log_file_path = NULL;

daemon_internals_t * daemon_internals = NULL;

/**************************************/

union tcpip_socket_addr {
   struct addrinfo tcpip_addr; ///< used for TCPIP socket
   struct sockaddr_un unix_addr; ///< used for path of UNIX socket
};

long int get_total_cpu_usage();
void start_service_thread();
int parse_arguments(int *argc, char **argv);
void daemon_mode();
char *get_param_by_delimiter(const char *source, char **dest, const char delimiter);
void free_output_file_strings_and_streams();
void generate_backup_config_file();
char * get_stats_formated_time();

void print_xmlDoc_to_stream(xmlDocPtr doc_ptr, FILE * stream)
{
   if (doc_ptr != NULL && stream != NULL) {
      xmlChar * formated_xml_output = NULL;
      int size = 0;
      xmlDocDumpFormatMemory(doc_ptr, &formated_xml_output, &size, 1);
      if (formated_xml_output == NULL) {
         return;
      } else {
         fprintf(stream, "%s\n", formated_xml_output);
         xmlFree(formated_xml_output);
      }
   }
}

void create_output_dir()
{
   char * buffer = NULL;
   if (netconf_flag) { // netconf mode
      logs_path = (char *) calloc (strlen(NC_DEFAULT_LOGSDIR_PATH)+1, sizeof(char));
      strcpy(logs_path, NC_DEFAULT_LOGSDIR_PATH);
   } else { // interactive mode or daemon mode
      if (logs_path == NULL) {
         if ((buffer = getenv("HOME")) != NULL) {
            logs_path = (char *) calloc (strlen(buffer)+strlen("/supervisor_logs/")+1, sizeof(char));
            sprintf(logs_path,"%s/supervisor_logs/", buffer);
         }
      } else {
         if (logs_path[strlen(logs_path)-1] != '/') {
            buffer = (char *) calloc (strlen(logs_path)+2, sizeof(char));
            sprintf(buffer, "%s/", logs_path);
            free(logs_path);
            logs_path = buffer;
         }
      }
   }

   struct stat st = {0};
   /* check and create output directiory */
   if (stat(logs_path, &st) == -1) {
      if (errno == EACCES) {
         if (logs_path != NULL) {
            free(logs_path);
            logs_path = NULL;
         }
         logs_path = (char *) calloc (strlen(NC_DEFAULT_LOGSDIR_PATH)+1, sizeof(char));
         strcpy(logs_path, NC_DEFAULT_LOGSDIR_PATH);
         mkdir(logs_path, PERM_LOGSDIR);
      }
      if (mkdir(logs_path, PERM_LOGSDIR) == -1) {
         if (errno == EACCES) {
            if (logs_path != NULL) {
               free(logs_path);
               logs_path = NULL;
            }
            logs_path = (char *) calloc (strlen(NC_DEFAULT_LOGSDIR_PATH)+1, sizeof(char));
            strcpy(logs_path, NC_DEFAULT_LOGSDIR_PATH);
            mkdir(logs_path, PERM_LOGSDIR);
         }
      }
   }
}

void create_output_files_strings()
{
   free_output_file_strings_and_streams();

   supervisor_debug_log_file_path = (char *) calloc (strlen(logs_path)+strlen("supervisor_debug_log")+1, sizeof(char));
   sprintf(supervisor_debug_log_file_path, "%ssupervisor_debug_log", logs_path);
   statistics_file_path = (char *) calloc (strlen(logs_path)+strlen("supervisor_log_statistics")+1, sizeof(char));
   sprintf(statistics_file_path, "%ssupervisor_log_statistics", logs_path);
   module_event_file_path = (char *) calloc (strlen(logs_path)+strlen("supervisor_log_module_event")+1, sizeof(char));
   sprintf(module_event_file_path, "%ssupervisor_log_module_event", logs_path);

   supervisor_debug_log_fd = fopen(supervisor_debug_log_file_path, "a");
   if (supervisor_debug_log_fd == NULL) {
      fprintf(stderr, "%s [ERROR] Could not open supervisor_debug_log file stream!\n",get_stats_formated_time());
   } else {
      fprintf(supervisor_debug_log_fd,"-------------------- %s --------------------\n", get_stats_formated_time());
   }
   statistics_fd = fopen(statistics_file_path, "a");
   if (statistics_fd == NULL) {
      fprintf(stderr, "%s [ERROR] Could not open supervisor_log_statistics file stream!\n",get_stats_formated_time());
   } else {
      VERBOSE(STATISTICS,"-------------------- %s --------------------\n", get_stats_formated_time());
      print_statistics_legend();
   }
   module_event_fd = fopen(module_event_file_path, "a");
   if (module_event_fd == NULL) {
      fprintf(stderr, "%s [ERROR] Could not open supervisor_log_module_event file stream!\n",get_stats_formated_time());
   } else {
      VERBOSE(MODULE_EVENT,"-------------------- %s --------------------\n", get_stats_formated_time());
   }

   if (netconf_flag || daemon_flag) {
      supervisor_log_file_path = (char *) calloc (strlen(logs_path)+strlen("supervisor_log")+1, sizeof(char));
      sprintf(supervisor_log_file_path, "%ssupervisor_log", logs_path);

      supervisor_log_fd = fopen (supervisor_log_file_path, "a");
      if (supervisor_log_fd == NULL) {
         fprintf(stderr, "%s [ERROR] Could not open supervisor_log file stream!\n",get_stats_formated_time());
      } else {
         fprintf(supervisor_log_fd,"-------------------- %s --------------------\n", get_stats_formated_time());
      }
      if (netconf_flag) {
         output_fd = supervisor_log_fd;
      } else if (!(daemon_internals->client_connected)) {
         output_fd = supervisor_log_fd;
      }
   }
}

struct tm * get_sys_time()
{
   time_t rawtime;
   time(&rawtime);
   return localtime(&rawtime);
}

char * get_stats_formated_time()
{
   static char formated_time_buffer[DEFAULT_SIZE_OF_BUFFER];
   memset(formated_time_buffer,0,DEFAULT_SIZE_OF_BUFFER);
   struct tm * act_time = get_sys_time();

   sprintf(formated_time_buffer,"%d.%d %d:%d:%d",  act_time->tm_mday, act_time->tm_mon+1, act_time->tm_hour, act_time->tm_min, act_time->tm_sec);

   return formated_time_buffer;
}

void interactive_show_available_modules ()
{
   unsigned int x,y = 0;
   VERBOSE(N_STDOUT,"[PRINTING CONFIGURATION]\n");

   for (x=0; x < loaded_modules_cnt; x++) {
      VERBOSE(N_STDOUT, ANSI_RED_BOLD "%c" ANSI_ATTR_RESET ANSI_BOLD "%d_%s:" ANSI_ATTR_RESET "  PATH:%s  PARAMS:%s  PROFILE:%s\n", (running_modules[x].module_enabled == 0 ? ' ' : '*'), x, running_modules[x].module_name,
                                                                                                                                    (running_modules[x].module_path == NULL ? "none" : running_modules[x].module_path),
                                                                                                                                    (running_modules[x].module_params == NULL ? "none" : running_modules[x].module_params),
                                                                                                                                    (running_modules[x].modules_profile == NULL ? "none" : running_modules[x].modules_profile));

      for (y=0; y<running_modules[x].module_ifces_cnt; y++) {
         VERBOSE(N_STDOUT,"\t" ANSI_BOLD "IFC%d:" ANSI_ATTR_RESET "  %s;%s;%s;%s\n", y, (running_modules[x].module_ifces[y].ifc_direction == NULL ? "none" : running_modules[x].module_ifces[y].ifc_direction),
                                                                                                (running_modules[x].module_ifces[y].ifc_type == NULL ? "none" : running_modules[x].module_ifces[y].ifc_type),
                                                                                                (running_modules[x].module_ifces[y].ifc_params == NULL ? "none" : running_modules[x].module_ifces[y].ifc_params),
                                                                                                (running_modules[x].module_ifces[y].ifc_note == NULL ? "none" : running_modules[x].module_ifces[y].ifc_note));
      }
   VERBOSE(N_STDOUT, ANSI_BOLD "- - - - - - - - - -\n" ANSI_ATTR_RESET);
   }
}


char **make_module_arguments(const int number_of_module)
{
   unsigned int size_of_atr = DEFAULT_SIZE_OF_BUFFER;
   char * atr = (char *) calloc (size_of_atr, sizeof(char));
   unsigned int x = 0, y = 0;
   int ptr = 0;
   int str_len = 0;
   unsigned int params_counter = 0;
   char ** params = NULL;

   //binary without libtrap interfaces
   if (running_modules[number_of_module].module_ifces_cnt == 0) {
      if (running_modules[number_of_module].module_params == NULL) {
         params = (char **) calloc (2,sizeof(char*));

         str_len = strlen(running_modules[number_of_module].module_name);
         params[0] = (char *) calloc (str_len+1, sizeof(char));   // binary name for exec
         strncpy(params[0],running_modules[number_of_module].module_name, str_len+1);
         params[1] = NULL;
      } else {
         unsigned int size_of_buffer = DEFAULT_SIZE_OF_BUFFER;
         char * buffer = (char *) calloc (size_of_buffer, sizeof(char));
         int num_module_params = 0;
         unsigned int module_params_length = strlen(running_modules[number_of_module].module_params);

         for (x=0; x<module_params_length; x++) {
            if (running_modules[number_of_module].module_params[x] == 32) {
               num_module_params++;
            }
         }
         num_module_params++;

         params = (char **) calloc (2+num_module_params,sizeof(char*));
         str_len = strlen(running_modules[number_of_module].module_name);
         params[0] = (char *) calloc (str_len+1, sizeof(char));   // binary name for exec
         strncpy(params[0],running_modules[number_of_module].module_name, str_len+1);

         params_counter = 1;

         y=0;
         for (x=0; x<module_params_length; x++) {
            if (running_modules[number_of_module].module_params[x] == 32) {
               params[params_counter] = (char *) calloc (strlen(buffer)+1,sizeof(char));
               sprintf(params[params_counter],"%s",buffer);
               params_counter++;
               memset(buffer,0,size_of_buffer);
               y=0;
            } else {
               if (y >= size_of_buffer) {
                  size_of_buffer += size_of_buffer/2;
                  buffer = (char *) realloc (buffer, size_of_buffer*sizeof(char));
                  memset(buffer + y, 0, size_of_buffer/3);
               }
               buffer[y] = running_modules[number_of_module].module_params[x];
               y++;
            }
         }
         params[params_counter] = (char *) calloc (strlen(buffer)+1,sizeof(char));
         sprintf(params[params_counter],"%s",buffer);
         params_counter++;

         params[params_counter] = NULL;
      }

      fprintf(stdout,"%s [INFO] Supervisor - executed command: %s", get_stats_formated_time(), running_modules[number_of_module].module_path);
      fprintf(stderr,"%s [INFO] Supervisor - executed command: %s", get_stats_formated_time(), running_modules[number_of_module].module_path);

      if (params_counter > 0) {
         for (x=1; x<params_counter; x++) {
            fprintf(stdout,"   %s",params[x]);
            fprintf(stderr,"   %s",params[x]);
         }
      }

      fprintf(stdout,"\n");
      fprintf(stderr,"\n");
      return params;
   }

   for (x=0; x<running_modules[number_of_module].module_ifces_cnt; x++) {
      if (running_modules[number_of_module].module_ifces[x].ifc_direction != NULL) {
         if (!strncmp(running_modules[number_of_module].module_ifces[x].ifc_direction, "IN", 2)) {
            if (running_modules[number_of_module].module_ifces[x].ifc_type != NULL) {
               if (!strncmp(running_modules[number_of_module].module_ifces[x].ifc_type, "TCP", 3)) {
                  strncpy(atr + ptr,"t",1);
                  ptr++;
               } else if (!strncmp(running_modules[number_of_module].module_ifces[x].ifc_type, "UNIXSOCKET", 10)) {
                  strncpy(atr + ptr,"u",1);
                  ptr++;
               } else {
                  VERBOSE(N_STDOUT,"%s\n", running_modules[number_of_module].module_ifces[x].ifc_type);
                  VERBOSE(N_STDOUT,"Wrong ifc_type in module %d.\n", number_of_module);
                  return NULL;
               }
            }
         }
      }
   }

   for (x=0; x<running_modules[number_of_module].module_ifces_cnt; x++) {
      if (running_modules[number_of_module].module_ifces[x].ifc_direction != NULL) {
         if (!strncmp(running_modules[number_of_module].module_ifces[x].ifc_direction,"OUT", 3)) {
            if (running_modules[number_of_module].module_ifces[x].ifc_type != NULL) {
               if (!strncmp(running_modules[number_of_module].module_ifces[x].ifc_type, "TCP", 3)) {
                  strncpy(atr + ptr,"t",1);
                  ptr++;
               } else if (!strncmp(running_modules[number_of_module].module_ifces[x].ifc_type, "UNIXSOCKET", 10)) {
                  strncpy(atr + ptr,"u",1);
                  ptr++;
               } else {
                  VERBOSE(N_STDOUT,"%s\n", running_modules[number_of_module].module_ifces[x].ifc_type);
                  VERBOSE(N_STDOUT,"Wrong ifc_type in module %d.\n", number_of_module);
                  return NULL;
               }
            }
         }
      }
   }

   for (x=0; x<running_modules[number_of_module].module_ifces_cnt; x++) {
      if (running_modules[number_of_module].module_ifces[x].ifc_type != NULL) {
         if (!strncmp(running_modules[number_of_module].module_ifces[x].ifc_type,"SERVICE", 7)) {
            strncpy(atr + ptr,"s",1);
            ptr++;
         }
      }
   }

   strncpy(atr + ptr,";",1);
   ptr++;

   for (x=0; x<running_modules[number_of_module].module_ifces_cnt; x++) {
      if (running_modules[number_of_module].module_ifces[x].ifc_direction != NULL) {
         if (!strncmp(running_modules[number_of_module].module_ifces[x].ifc_direction,"IN", 2)) {
            if (running_modules[number_of_module].module_ifces[x].ifc_params != NULL) {
               if ((strlen(atr) + strlen(running_modules[number_of_module].module_ifces[x].ifc_params) + 1) >= (3*size_of_atr)/5) {
                  size_of_atr += strlen(running_modules[number_of_module].module_ifces[x].ifc_params) + (size_of_atr/2);
                  atr = (char *) realloc (atr, size_of_atr*sizeof(char));
                  memset(atr + ptr, 0, size_of_atr - ptr);
               }
               sprintf(atr + ptr,"%s;",running_modules[number_of_module].module_ifces[x].ifc_params);
               ptr += strlen(running_modules[number_of_module].module_ifces[x].ifc_params) + 1;
            }
         }
      }
   }

   for (x=0; x<running_modules[number_of_module].module_ifces_cnt; x++) {
      if (running_modules[number_of_module].module_ifces[x].ifc_direction != NULL) {
         if (!strncmp(running_modules[number_of_module].module_ifces[x].ifc_direction,"OUT", 3)) {
            if (running_modules[number_of_module].module_ifces[x].ifc_params != NULL) {
               if ((strlen(atr) + strlen(running_modules[number_of_module].module_ifces[x].ifc_params) + 1) >= (3*size_of_atr)/5) {
                  size_of_atr += strlen(running_modules[number_of_module].module_ifces[x].ifc_params) + (size_of_atr/2);
                  atr = (char *) realloc (atr, size_of_atr*sizeof(char));
                  memset(atr + ptr, 0, size_of_atr - ptr);
               }
               sprintf(atr + ptr,"%s;",running_modules[number_of_module].module_ifces[x].ifc_params);
               ptr += strlen(running_modules[number_of_module].module_ifces[x].ifc_params) + 1;
            }
         }
      }
   }

   for (x=0; x<running_modules[number_of_module].module_ifces_cnt; x++) {
      if (running_modules[number_of_module].module_ifces[x].ifc_type != NULL) {
         if (!strncmp(running_modules[number_of_module].module_ifces[x].ifc_type,"SERVICE", 7)) {
            if (running_modules[number_of_module].module_ifces[x].ifc_params != NULL) {
               if ((strlen(atr) + strlen(running_modules[number_of_module].module_ifces[x].ifc_params) + 1) >= (3*size_of_atr)/5) {
                  size_of_atr += strlen(running_modules[number_of_module].module_ifces[x].ifc_params) + (size_of_atr/2);
                  atr = (char *) realloc (atr, size_of_atr*sizeof(char));
                  memset(atr + ptr, 0, size_of_atr - ptr);
               }
               sprintf(atr + ptr,"%s;",running_modules[number_of_module].module_ifces[x].ifc_params);
               ptr += strlen(running_modules[number_of_module].module_ifces[x].ifc_params) + 1;
            }
         }
      }
   }
   memset(atr + ptr-1,0,1);

   if (running_modules[number_of_module].module_params == NULL) {
      params = (char **) calloc (4,sizeof(char*));
      str_len = strlen(running_modules[number_of_module].module_name);
      params[0] = (char *) calloc (str_len+1, sizeof(char));   // binary name for exec
      strncpy(params[0],running_modules[number_of_module].module_name, str_len+1);
      str_len = strlen(TRAP_PARAM);
      params[1] = (char *) calloc (str_len+1,sizeof(char));    // libtrap param "-i"
      strncpy(params[1],TRAP_PARAM,str_len+1);
      str_len = strlen(atr);
      params[2] = (char *) calloc (str_len+1,sizeof(char)); // atributes for "-i" param
      strncpy(params[2],atr,str_len+1);

      params[3] = NULL;
   } else {
      unsigned int size_of_buffer = DEFAULT_SIZE_OF_BUFFER;
      char * buffer = (char *) calloc (size_of_buffer, sizeof(char));
      int num_module_params = 0;
      unsigned int module_params_length = strlen(running_modules[number_of_module].module_params);

      for (x=0; x<module_params_length; x++) {
         if (running_modules[number_of_module].module_params[x] == 32 && (x != (module_params_length-1))) {
            num_module_params++;
         }
      }
      num_module_params++;

      params = (char **) calloc (4+num_module_params,sizeof(char*));
      str_len = strlen(running_modules[number_of_module].module_name);
      params[0] = (char *) calloc (str_len+1, sizeof(char));   // binary name for exec
      strncpy(params[0],running_modules[number_of_module].module_name, str_len+1);
      str_len = strlen(TRAP_PARAM);
      params[1] = (char *) calloc (str_len+1,sizeof(char));    // libtrap param "-i"
      strncpy(params[1],TRAP_PARAM,str_len+1);
      str_len = strlen(atr);
      params[2] = (char *) calloc (str_len+1,sizeof(char)); // atributes for "-i" param
      strncpy(params[2],atr,str_len+1);

      params_counter = 3;

      y=0;
      for (x=0; x<module_params_length; x++) {
         if (running_modules[number_of_module].module_params[x] == 32 && (x == (module_params_length-1))) {
            break;
         } else if (running_modules[number_of_module].module_params[x] == 32) {
            params[params_counter] = (char *) calloc (strlen(buffer)+1,sizeof(char));
            sprintf(params[params_counter],"%s",buffer);
            params_counter++;
            memset(buffer,0,size_of_buffer);
            y=0;
         } else {
            if (y >= size_of_buffer) {
               size_of_buffer += size_of_buffer/2;
               buffer = (char *) realloc (buffer, size_of_buffer*sizeof(char));
               memset(buffer + y, 0, size_of_buffer/3);
            }
            buffer[y] = running_modules[number_of_module].module_params[x];
            y++;
         }
      }
      params[params_counter] = (char *) calloc (strlen(buffer)+1,sizeof(char));
      sprintf(params[params_counter],"%s",buffer);
      params_counter++;


      params[params_counter] = NULL;
   }

   fprintf(stdout,"%s [INFO] Supervisor - executed command: %s   %s   %s", get_stats_formated_time(), running_modules[number_of_module].module_path, params[1], params[2]);
   fprintf(stderr,"%s [INFO] Supervisor - executed command: %s   %s   %s", get_stats_formated_time(), running_modules[number_of_module].module_path, params[1], params[2]);

   if (params_counter > 0) {
      for (x=3; x<params_counter; x++) {
         fprintf(stdout,"   %s",params[x]);
         fprintf(stderr,"   %s",params[x]);
      }
   }

   fprintf(stdout,"\n");
   fprintf(stderr,"\n");
   return params;
}

int get_number_from_input_choosing_option ()
{
   int option = 0;
   char * input_p = NULL;

   input_p = get_input_from_stream(input_fd);
   if (input_p == NULL) {
      return RET_ERROR;
   } else if (strlen(input_p) == 1 && sscanf(input_p, "%d", &option) == 1 && option >= 0) {
      free(input_p);
      input_p = NULL;
      return option;
   } else {
      free(input_p);
      input_p = NULL;
      return RET_ERROR;
   }
}

/* Returns count of numbers in input (separated by commas) or -1 */
int get_numbers_from_input_dis_enable_module(int ** array)
{
   int x = 0, module_nums_cnt = 0, is_num = FALSE;
   int * module_nums = NULL;
   char * input_p = NULL;
   int error_input = FALSE;

   module_nums = (int *) calloc (50, sizeof(int));
   input_p = get_input_from_stream(input_fd);

   if (input_p == NULL) {
      error_input = TRUE;
   } else if (strlen(input_p) == 0) {
      VERBOSE(N_STDOUT, ANSI_RED_BOLD "[WARNING] Wrong input - empty string.\n" ANSI_ATTR_RESET);
      error_input = TRUE;
   } else {
      for (x=0; x<strlen(input_p); x++) {
         if (input_p[x] <= '9' && input_p[x] >= '0') {
            is_num = TRUE;
            module_nums[module_nums_cnt] *= 10;
            module_nums[module_nums_cnt] += (input_p[x] - '0');
         } else if (input_p[x] == ',') {
            if (is_num == TRUE) {
               is_num = FALSE;
               module_nums_cnt++;
            } else {
               VERBOSE(N_STDOUT, ANSI_RED_BOLD "[WARNING] Wrong input - comma without a number before it.\n" ANSI_ATTR_RESET);
               error_input = TRUE;
               break;
            }
            if (x == (strlen(input_p) -1)) {
               VERBOSE(N_STDOUT, ANSI_RED_BOLD "[WARNING] Wrong input - comma at the end.\n" ANSI_ATTR_RESET);
               error_input = TRUE;
               break;
            }
         } else {
            VERBOSE(N_STDOUT, ANSI_RED_BOLD "[WARNING] Wrong input - acceptable characters are digits and comma.\n" ANSI_ATTR_RESET);
            error_input = TRUE;
            break;
         }
      }
   }

   if (error_input == TRUE) {
      if (module_nums != NULL) {
         free(module_nums);
         module_nums = NULL;
      }
      if (input_p != NULL) {
         free(input_p);
         input_p = NULL;
      }
      array = NULL;
      return RET_ERROR;
   }

   free(input_p);
   input_p = NULL;
   *array = module_nums;
   return ++module_nums_cnt;
}


int interactive_get_option()
{
   usleep(50000); // Solved bugged output - without this sleep, escape codes in output were not sometimes reseted on time and they were applied also on this menu
   VERBOSE(N_STDOUT, ANSI_CYAN_BOLD "--------OPTIONS--------\n" ANSI_ATTR_RESET);
   VERBOSE(N_STDOUT, ANSI_CYAN "1. START ALL MODULES\n");
   VERBOSE(N_STDOUT, "2. STOP ALL MODULES\n");
   VERBOSE(N_STDOUT, "3. START MODULE\n");
   VERBOSE(N_STDOUT, "4. STOP MODULE\n");
   VERBOSE(N_STDOUT, "5. STARTED MODULES STATUS\n");
   VERBOSE(N_STDOUT, "6. AVAILABLE MODULES\n");
   VERBOSE(N_STDOUT, "7. RELOAD CONFIGURATION\n");
   VERBOSE(N_STDOUT, "8. STOP SUPERVISOR\n" ANSI_ATTR_RESET);
   VERBOSE(N_STDOUT, ANSI_YELLOW_BOLD "[INTERACTIVE] Your choice: " ANSI_ATTR_RESET);

   return get_number_from_input_choosing_option();
}

void init_module_variables(int module_number)
{
   if (running_modules[module_number].module_counters_array == NULL) {
      running_modules[module_number].module_counters_array = (uint64_t *) calloc (3*running_modules[module_number].module_num_out_ifc + running_modules[module_number].module_num_in_ifc,sizeof(uint64_t));
      running_modules[module_number].module_running = TRUE;
   }
   memset(running_modules[module_number].module_counters_array, 0, (running_modules[module_number].module_num_in_ifc + (3*running_modules[module_number].module_num_out_ifc)) * sizeof(uint64_t));
   running_modules[module_number].total_cpu_usage_during_module_startup = get_total_cpu_usage();
   running_modules[module_number].last_period_cpu_usage_kernel_mode = 0;
   running_modules[module_number].last_period_cpu_usage_user_mode = 0;
   running_modules[module_number].last_period_percent_cpu_usage_kernel_mode = 0;
   running_modules[module_number].last_period_percent_cpu_usage_user_mode = 0;
   running_modules[module_number].overall_percent_module_cpu_usage_kernel_mode = 0;
   running_modules[module_number].overall_percent_module_cpu_usage_user_mode = 0;
   running_modules[module_number].module_service_sd = -1;
   running_modules[module_number].module_service_ifc_conn_attempts = 0;
   running_modules[module_number].sent_sigint = FALSE;
}

void re_start_module(const int module_number)
{
   if (running_modules[module_number].module_running == FALSE) {
      VERBOSE(MODULE_EVENT,"%s [START] Starting module %s.\n", get_stats_formated_time(), running_modules[module_number].module_name);
      #ifdef nemea_plugin
         nc_notify(MODULE_EVENT_STARTED,running_modules[module_number].module_name);
      #endif
      if (running_modules[module_number].module_counters_array != NULL) {
         free(running_modules[module_number].module_counters_array);
         running_modules[module_number].module_counters_array = NULL;
      }
      running_modules[module_number].module_counters_array = (uint64_t *) calloc (3*running_modules[module_number].module_num_out_ifc + running_modules[module_number].module_num_in_ifc,sizeof(uint64_t));
      running_modules[module_number].module_running = TRUE;
   } else {
      #ifdef nemea_plugin
         nc_notify(MODULE_EVENT_RESTARTED,running_modules[module_number].module_name);
      #endif
      VERBOSE(MODULE_EVENT,"%s [RESTART] Restarting module %s\n", get_stats_formated_time(), running_modules[module_number].module_name);
   }

   char log_path_stdout[200];
   char log_path_stderr[200];
   memset(log_path_stderr,0,200);
   memset(log_path_stdout,0,200);

   sprintf(log_path_stdout,"%s%s_stdout",logs_path, running_modules[module_number].module_name);
   sprintf(log_path_stderr,"%s%s_stderr",logs_path, running_modules[module_number].module_name);

   init_module_variables(module_number);

   time_t rawtime;
   struct tm * timeinfo;
   time ( &rawtime );
   timeinfo = localtime ( &rawtime );

   fflush(stdout);
   running_modules[module_number].module_pid = fork();
   if (running_modules[module_number].module_pid == 0) {
      int fd_stdout = open(log_path_stdout, O_RDWR | O_CREAT | O_APPEND, PERM_LOGFILE);
      int fd_stderr = open(log_path_stderr, O_RDWR | O_CREAT | O_APPEND, PERM_LOGFILE);
      if (fd_stdout != -1) {
         dup2(fd_stdout,1); //stdout
         close(fd_stdout);
      }
      if (fd_stderr != -1) {
         dup2(fd_stderr,2); //stderr
         close(fd_stderr);
      }
      setsid(); // important for sending SIGINT to supervisor.. modules can't receive the signal too !!!
      fprintf(stdout,"---> %s", asctime (timeinfo));
      fprintf(stderr,"---> %s", asctime (timeinfo));
      if (running_modules[module_number].module_path == NULL) {
         VERBOSE(N_STDOUT,"%s [ERROR] Starting module: module path is missing!\n", get_stats_formated_time());
         running_modules[module_number].module_enabled = FALSE;
      } else {
         char **params = make_module_arguments(module_number);
         fflush(stdout);
         fflush(stderr);
         execvp(running_modules[module_number].module_path, params);
      }
      VERBOSE(MODULE_EVENT,"%s [ERROR] Module execution: could not execute %s binary! (possible reason - wrong module binary path)\n", get_stats_formated_time(), running_modules[module_number].module_name);
      running_modules[module_number].module_enabled = FALSE;
      exit(EXIT_FAILURE);
   } else if (running_modules[module_number].module_pid == -1) {
      running_modules[module_number].module_status = FALSE;
      running_modules[module_number].module_restart_cnt++;
      VERBOSE(N_STDOUT,"%s [ERROR] Fork: could not fork supervisor process!\n", get_stats_formated_time());
   } else {
      running_modules[module_number].module_is_my_child = TRUE;
      running_modules[module_number].module_status = TRUE;
      running_modules[module_number].module_restart_cnt++;
      if (running_modules[module_number].module_restart_cnt == 1) {
         running_modules[module_number].module_restart_timer = 0;
      }
   }
}

// returns TRUE if some module is running, else FALSE
int service_update_module_status()
{
   unsigned int x, some_module_running = FALSE;

   for (x=0; x<loaded_modules_cnt; x++) {
      if (running_modules[x].module_pid > 0) {
         if (kill(running_modules[x].module_pid, 0) == -1) {
            if (errno == EINVAL) {
               VERBOSE(MODULE_EVENT,"%s [STOP] kill -0: ernno EINVAL\n", get_stats_formated_time());
            }
            if (errno == EPERM) {
               VERBOSE(MODULE_EVENT,"%s [STOP] kill -0: errno EPERM\n", get_stats_formated_time());
            }
            if (errno == ESRCH) {
               VERBOSE(MODULE_EVENT,"%s [STOP] kill -0: module %s (PID: %d) is not running !\n", get_stats_formated_time(), running_modules[x].module_name, running_modules[x].module_pid);
            }
            if (running_modules[x].module_service_sd != -1) {
                  close(running_modules[x].module_service_sd);
                  running_modules[x].module_service_sd = -1;
            }
            running_modules[x].module_status = FALSE;
            running_modules[x].module_service_ifc_isconnected = FALSE;
            running_modules[x].module_pid = 0;
         } else {
            running_modules[x].module_status = TRUE;
            some_module_running = TRUE;
         }
      }
   }
   return some_module_running;
}

void service_clean_after_children()
{
   pid_t result;
   unsigned int x;
   int status;

   for (x=0; x<loaded_modules_cnt; x++) {
      if (running_modules[x].module_pid > 0 && running_modules[x].module_is_my_child) {
         result = waitpid(running_modules[x].module_pid , &status, WNOHANG);
         if (result == 0) {
           // Child still alive, nothing to do here!
         } else if (result == -1) {
           // Error
            if (errno == ECHILD) {
               VERBOSE(MODULE_EVENT, "%s [CLEAN] waitpid: module %s (PID: %d) is not my child!\n", get_stats_formated_time(), running_modules[x].module_name, running_modules[x].module_pid);
               running_modules[x].module_is_my_child = FALSE;
            }
         } else {
           // Child exited
            VERBOSE(MODULE_EVENT, "%s [CLEAN] waitpid: module %s (PID: %d) is my child and is not alive anymore!\n", get_stats_formated_time(), running_modules[x].module_name, running_modules[x].module_pid);
         }
      }
   }
}

void supervisor_signal_handler(int catched_signal)
{
   switch (catched_signal) {
   case SIGPIPE:
      break;

   case SIGTERM:
      VERBOSE(N_STDOUT,"%s [SIGNAL HANDLER] SIGTERM catched -> I'm going to terminate my self !\n", get_stats_formated_time());
      supervisor_termination(TRUE, FALSE);
      exit(EXIT_SUCCESS);
      break;

   case SIGINT:
      VERBOSE(N_STDOUT,"%s [SIGNAL HANDLER] SIGINT catched -> I'm going to terminate my self !\n", get_stats_formated_time());
      supervisor_termination(FALSE, TRUE);
      exit(EXIT_SUCCESS);
      break;

   case SIGQUIT:
      VERBOSE(N_STDOUT,"%s [SIGNAL HANDLER] SIGQUIT catched -> I'm going to terminate my self !\n", get_stats_formated_time());
      supervisor_termination(FALSE, TRUE);
      exit(EXIT_SUCCESS);
      break;

   case SIGSEGV:
      VERBOSE(N_STDOUT,"%s [SIGNAL HANDLER] Ouch, SIGSEGV catched -> I'm going to terminate my self !\n", get_stats_formated_time());
      supervisor_termination(FALSE, TRUE);
      exit(EXIT_FAILURE);
      break;
   }
}

int supervisor_initialization(int *argc, char **argv)
{
   unsigned int y = 0;
   int ret_val = 0;

   init_time_info = get_sys_time();
   service_stop_all_modules = FALSE;
   logs_path = NULL;
   config_file = NULL;
   socket_path = NULL;
   verbose_flag = FALSE;
   file_flag = FALSE;
   daemon_flag = FALSE;
   netconf_flag = FALSE;

   input_fd = stdin;
   output_fd = stdout;

   ret_val = parse_arguments(argc, argv);
   if (ret_val) {
      if (file_flag == FALSE) {
         fprintf(stderr,"Wrong format of arguments.\n supervisor [-d|--daemon] -f|--config-file=path [-h|--help] [-v|--verbose] [-L|--logs-path] [-s|--daemon-socket=path]\n");
         return 1;
      }
   } else {
      return 1;
   }

   if (daemon_flag) {
      daemon_init();
   }

   create_output_dir();
   create_output_files_strings();

   VERBOSE(N_STDOUT,"[INIT LOADING CONFIGURATION]\n");
   loaded_modules_cnt = 0;
   running_modules_array_size = RUNNING_MODULES_ARRAY_START_SIZE;
   running_modules = (running_module_t *) calloc (running_modules_array_size,sizeof(running_module_t));
   for (y=0; y<running_modules_array_size; y++) {
      running_modules[y].module_ifces = (interface_t *) calloc(IFCES_ARRAY_START_SIZE, sizeof(interface_t));
      running_modules[y].module_running = FALSE;
      running_modules[y].module_ifces_array_size = IFCES_ARRAY_START_SIZE;
      running_modules[y].module_ifces_cnt = 0;
   }

   pthread_mutex_init(&running_modules_lock,NULL);
   service_thread_continue = TRUE;

   //load configuration
   reload_configuration(RELOAD_INIT_LOAD_CONFIG, NULL);

   VERBOSE(N_STDOUT,"[SERVICE] Starting service thread.\n");
   start_service_thread();

   /************ SIGNAL HANDLING *************/
   /* function prototype to set handler */
   void supervisor_signal_handler(int catched_signal);

   struct sigaction sig_action;
   sig_action.sa_handler = supervisor_signal_handler;
   sig_action.sa_flags = 0;
   sigemptyset(&sig_action.sa_mask);

   if (sigaction(SIGPIPE,&sig_action,NULL) == -1) {
      VERBOSE(N_STDOUT,"%s [ERROR] Sigaction: signal handler won't catch SIGPIPE !\n", get_stats_formated_time());
   }
   if (sigaction(SIGINT,&sig_action,NULL) == -1) {
      VERBOSE(N_STDOUT,"%s [ERROR] Sigaction: signal handler won't catch SIGINT !\n", get_stats_formated_time());
   }
   if (sigaction(SIGTERM,&sig_action,NULL) == -1) {
      VERBOSE(N_STDOUT,"%s [ERROR] Sigaction: signal handler won't catch SIGTERM !\n", get_stats_formated_time());
   }
   if (sigaction(SIGSEGV,&sig_action,NULL) == -1) {
      VERBOSE(N_STDOUT,"%s [ERROR] Sigaction: signal handler won't catch SIGSEGV !\n", get_stats_formated_time());
   }
   if (sigaction(SIGQUIT,&sig_action,NULL) == -1) {
      VERBOSE(N_STDOUT,"%s [ERROR] Sigaction: signal handler won't catch SIGQUIT !\n", get_stats_formated_time());
   }
   /****************************************/

   if (daemon_flag) {
      daemon_mode();
      return 2;
   }

   return 0;
}


void interactive_start_configuration()
{
   pthread_mutex_lock(&running_modules_lock);
   VERBOSE(MODULE_EVENT,"%s [START] Starting configuration...\n", get_stats_formated_time());
   unsigned int x = 0;
   for (x=0; x<loaded_modules_cnt; x++) {
      if (running_modules[x].module_enabled == FALSE) {
         running_modules[x].module_restart_cnt = -1;
         running_modules[x].module_enabled = TRUE;
      }
   }
   pthread_mutex_unlock(&running_modules_lock);
}


void interactive_stop_configuration()
{
   unsigned int x = 0;
   pthread_mutex_lock(&running_modules_lock);
   VERBOSE(MODULE_EVENT,"%s [STOP] Stopping configuration...\n", get_stats_formated_time());
   for (x=0; x<loaded_modules_cnt; x++) {
      if (running_modules[x].module_enabled) {
         running_modules[x].module_enabled = FALSE;
      }
   }
   pthread_mutex_unlock(&running_modules_lock);
}


void interactive_set_module_enabled()
{
   int * modules_to_enable = NULL;
   int x = 0, y = 0, modules_to_enable_cnt = 0;

   pthread_mutex_lock(&running_modules_lock);
   VERBOSE(N_STDOUT, ANSI_YELLOW_BOLD "[INTERACTIVE] Type in module numbers (one number or more separated by comma): " ANSI_ATTR_RESET);
   modules_to_enable_cnt = get_numbers_from_input_dis_enable_module(&modules_to_enable);

   if (modules_to_enable_cnt != RET_ERROR) {
      for (y=0; y<modules_to_enable_cnt; y++) {
         x = modules_to_enable[y];
         if (x>=loaded_modules_cnt || x<0) {
            VERBOSE(N_STDOUT, ANSI_RED_BOLD "[WARNING] Number %d is not valid module number!\n" ANSI_ATTR_RESET, x);
            continue;
         } else if (running_modules[x].module_enabled == TRUE) {
            VERBOSE(N_STDOUT, ANSI_RED_BOLD "[WARNING] Module %s is already enabled.\n" ANSI_ATTR_RESET, running_modules[x].module_name);
         } else {
            running_modules[x].module_enabled = TRUE;
            running_modules[x].module_restart_cnt = -1;
            VERBOSE(MODULE_EVENT, "%s [ENABLED] Module %s set to enabled.\n", get_stats_formated_time(), running_modules[x].module_name);
         }
      }
      free(modules_to_enable);
      modules_to_enable = NULL;
   }

   pthread_mutex_unlock(&running_modules_lock);
}

void service_stop_modules_sigint()
{
   unsigned int x;
   for (x=0; x<loaded_modules_cnt; x++) {
      if (running_modules[x].module_status && running_modules[x].module_enabled == FALSE && running_modules[x].sent_sigint == FALSE) {
         #ifdef nemea_plugin
            nc_notify(MODULE_EVENT_STOPPED,running_modules[x].module_name);
         #endif
         VERBOSE(MODULE_EVENT, "%s [STOP] Stopping module %s... sending SIGINT\n", get_stats_formated_time(), running_modules[x].module_name);
         kill(running_modules[x].module_pid,2);
         running_modules[x].sent_sigint = TRUE;
      }
   }
}

void service_stop_modules_sigkill()
{
   char * dest_port = NULL;
   char buffer[DEFAULT_SIZE_OF_BUFFER];
   unsigned int x, y;
   for (x=0; x<loaded_modules_cnt; x++) {
      if (running_modules[x].module_status && running_modules[x].module_enabled == FALSE && running_modules[x].sent_sigint == TRUE) {
         VERBOSE(MODULE_EVENT, "%s [STOP] Stopping module %s... sending SIGKILL\n", get_stats_formated_time(), running_modules[x].module_name);
         kill(running_modules[x].module_pid,9);
         for (y=0; y<running_modules[x].module_ifces_cnt; y++) {
            if (running_modules[x].module_ifces[y].ifc_type != NULL) {
               memset(buffer, 0, DEFAULT_SIZE_OF_BUFFER);
               if (strcmp(running_modules[x].module_ifces[y].ifc_type, "SERVICE") == 0 || ((strcmp(running_modules[x].module_ifces[y].ifc_type, "UNIXSOCKET") == 0)
                                                                                          && (running_modules[x].module_ifces[y].ifc_direction != NULL)
                                                                                          && (strcmp(running_modules[x].module_ifces[y].ifc_direction, "OUT") == 0))) {
                  if (running_modules[x].module_ifces[y].ifc_params == NULL) {
                     continue;
                  }
                  get_param_by_delimiter(running_modules[x].module_ifces[y].ifc_params, &dest_port, ',');
                  sprintf(buffer,MODULES_UNIXSOCKET_PATH_FILENAME_FORMAT,dest_port);
                  VERBOSE(MODULE_EVENT, "%s [CLEAN] Deleting socket %s - module %s\n", get_stats_formated_time(), buffer, running_modules[x].module_name);
                  unlink(buffer);
                  if (dest_port != NULL) {
                     free(dest_port);
                     dest_port = NULL;
                  }
               }
            }
         }
      }
   }
}

void interactive_stop_module()
{
   int x = 0, y = 0, running_modules_counter = 0, modules_to_stop_cnt = 0;
   int * modules_to_stop = NULL;

   pthread_mutex_lock(&running_modules_lock);
   for (x=0;x<loaded_modules_cnt;x++) {
      if (running_modules[x].module_status) {
         VERBOSE(N_STDOUT,"%d_%s " ANSI_RED_BOLD "running" ANSI_ATTR_RESET " (PID: %d)\n",x, running_modules[x].module_name,running_modules[x].module_pid);
         running_modules_counter++;
      }
   }
   if (running_modules_counter == 0) {
      VERBOSE(N_STDOUT, ANSI_RED_BOLD "[WARNING] All modules are stopped.\n" ANSI_ATTR_RESET);
      pthread_mutex_unlock(&running_modules_lock);
      return;
   }

   VERBOSE(N_STDOUT, ANSI_YELLOW_BOLD "[INTERACTIVE] Type in module numbers (one number or more separated by comma): " ANSI_ATTR_RESET);
   modules_to_stop_cnt = get_numbers_from_input_dis_enable_module(&modules_to_stop);

   if (modules_to_stop_cnt != RET_ERROR) {
      for (y=0; y<modules_to_stop_cnt; y++) {
         x = modules_to_stop[y];
         if (x>=loaded_modules_cnt || x<0) {
            VERBOSE(N_STDOUT, ANSI_RED_BOLD "[WARNING] Number %d is not valid module number!\n" ANSI_ATTR_RESET, x);
            continue;
         } else if (running_modules[x].module_enabled == FALSE) {
            VERBOSE(N_STDOUT, ANSI_RED_BOLD "[WARNING] Module %s is already disabled.\n" ANSI_ATTR_RESET, running_modules[x].module_name);
         } else {
            running_modules[x].module_enabled = FALSE;
            VERBOSE(MODULE_EVENT, "%s [DISABLED] Module %s set to disabled.\n", get_stats_formated_time(), running_modules[x].module_name);
         }
      }
      free(modules_to_stop);
      modules_to_stop = NULL;
   }
   pthread_mutex_unlock(&running_modules_lock);
}


void service_restart_modules()
{
   unsigned int x = 0;
   int max_restarts = 0;
   for (x=0; x<loaded_modules_cnt; x++) {
      if (++running_modules[x].module_restart_timer == 30) {
         running_modules[x].module_restart_timer = 0;
         running_modules[x].module_restart_cnt = 0;
      }

      if (running_modules[x].module_max_restarts_per_minute > -1) {
         max_restarts = running_modules[x].module_max_restarts_per_minute;
      } else {
         max_restarts = max_restarts_per_minute_config;
      }

      if (running_modules[x].module_enabled == TRUE && running_modules[x].module_status == FALSE && (running_modules[x].module_restart_cnt == max_restarts)) {
         VERBOSE(MODULE_EVENT,"%s [RESTART] Module: %s was restarted %d times per minute and it is down again. I set it disabled.\n", get_stats_formated_time(), running_modules[x].module_name, max_restarts);
         running_modules[x].module_enabled = FALSE;
         #ifdef nemea_plugin
            nc_notify(MODULE_EVENT_DISABLED,running_modules[x].module_name);
         #endif
      } else if (running_modules[x].module_status == FALSE && running_modules[x].module_enabled == TRUE) {
         re_start_module(x);
      }
   }
}

void interactive_show_running_modules_status()
{
   unsigned int x = 0, already_printed_modules = 0;
   modules_profile_t * ptr = first_profile_ptr;

   if (loaded_modules_cnt == 0) {
      VERBOSE(N_STDOUT, ANSI_RED_BOLD "[WARNING] No module is loaded.\n" ANSI_ATTR_RESET);
      return;
   }

   while (ptr != NULL) {
      VERBOSE(N_STDOUT, ANSI_BOLD "Profile: %s\n" ANSI_ATTR_RESET, ptr->profile_name);
      for (x=0; x<loaded_modules_cnt; x++) {
         if (running_modules[x].modules_profile != NULL) {
            if (running_modules[x].modules_profile == ptr->profile_name && running_modules[x].module_status == TRUE) {
               VERBOSE(N_STDOUT,"\t%d_%s " ANSI_RED_BOLD "running" ANSI_ATTR_RESET " (PID: %d)\n",x, running_modules[x].module_name,running_modules[x].module_pid);
               already_printed_modules++;
            } else if (running_modules[x].modules_profile == ptr->profile_name) {
               VERBOSE(N_STDOUT,"\t%d_%s " ANSI_RED "stopped" ANSI_ATTR_RESET " (PID: %d)\n",x, running_modules[x].module_name,running_modules[x].module_pid);
               already_printed_modules++;
            }
         }
      }
      ptr = ptr->next;
   }

   if (already_printed_modules < loaded_modules_cnt) {
      VERBOSE(N_STDOUT, ANSI_BOLD "Modules without profile:\n" ANSI_ATTR_RESET);
      for (x=0; x<loaded_modules_cnt; x++) {
         if (running_modules[x].modules_profile == NULL) {
            if (running_modules[x].module_status == TRUE) {
               VERBOSE(N_STDOUT,"\t%d_%s " ANSI_RED_BOLD "running" ANSI_ATTR_RESET " (PID: %d)\n",x, running_modules[x].module_name,running_modules[x].module_pid);
            } else {
               VERBOSE(N_STDOUT,"\t%d_%s " ANSI_RED "stopped" ANSI_ATTR_RESET " (PID: %d)\n",x, running_modules[x].module_name,running_modules[x].module_pid);
            }
         }
      }
   }
}

void free_output_file_strings_and_streams()
{
   if (statistics_file_path != NULL) {
      free(statistics_file_path);
      statistics_file_path = NULL;
   }
   if (module_event_file_path != NULL) {
      free(module_event_file_path);
      module_event_file_path = NULL;
   }
   if (supervisor_debug_log_file_path != NULL) {
      free(supervisor_debug_log_file_path);
      supervisor_debug_log_file_path = NULL;
   }
   if (supervisor_log_file_path != NULL) {
      free(supervisor_log_file_path);
      supervisor_log_file_path = NULL;
   }

   if (supervisor_debug_log_fd != NULL) {
      fclose(supervisor_debug_log_fd);
      supervisor_debug_log_fd = NULL;
   }
   if (supervisor_log_fd != NULL) {
      fclose(supervisor_log_fd);
      supervisor_log_fd = NULL;
   }
   if (statistics_fd != NULL) {
      fclose(statistics_fd);
      statistics_fd = NULL;
   }
   if (module_event_fd != NULL) {
      fclose(module_event_fd);
      module_event_fd = NULL;
   }
}

void free_daemon_internals_variables()
{
   daemon_internals->client_connected = FALSE;
   if (daemon_internals->client_input_stream_fd > 0) {
      close(daemon_internals->client_input_stream_fd);
      daemon_internals->client_input_stream_fd = 0;
   }
   if (daemon_internals->client_input_stream != NULL) {
      fclose(daemon_internals->client_input_stream);
      daemon_internals->client_input_stream = NULL;
   }
   if (daemon_internals->client_output_stream != NULL) {
      fclose(daemon_internals->client_output_stream);
      daemon_internals->client_output_stream = NULL;
   }
   if (daemon_internals->client_sd > 0) {
      close(daemon_internals->client_sd);
      daemon_internals->client_sd = 0;
   }
}

void free_module_and_shift_array(const int module_idx)
{
   int y = 0;

   free_module_on_index(module_idx);
   running_modules[module_idx].module_ifces_cnt = 0;
   running_modules[module_idx].module_num_out_ifc = 0;
   running_modules[module_idx].module_num_in_ifc = 0;
   running_modules[module_idx].module_ifces_array_size = 0;
   for (y=module_idx; y<(loaded_modules_cnt-1); y++) {
   memcpy(&running_modules[y], &running_modules[y+1], sizeof(running_module_t));
   }
   loaded_modules_cnt--;
   memset(&running_modules[loaded_modules_cnt], 0, sizeof(running_module_t));
}

void supervisor_termination(int stop_all_modules, int generate_backup)
{
   unsigned int x, y;

   if (stop_all_modules == TRUE) {
      interactive_stop_configuration();
      service_stop_all_modules = TRUE;
   } else {
      service_stop_all_modules = FALSE;
   }

   VERBOSE(N_STDOUT,"%s [SERVICE] Aborting service thread!\n", get_stats_formated_time());
   service_thread_continue = 0;

   x = pthread_join(service_thread_id, NULL);

   if (x == 0) {
      VERBOSE(N_STDOUT, "%s [SERVICE] pthread_join success: Service thread finished!\n", get_stats_formated_time())
   } else if (((int) x) == -1) {
      if (errno == EINVAL) {
         VERBOSE(N_STDOUT, "%s [ERROR] pthread_join: Not joinable thread!\n", get_stats_formated_time());
      } else if (errno == ESRCH) {
         VERBOSE(N_STDOUT, "%s [ERROR] pthread_join: No thread with this ID found!\n", get_stats_formated_time());
      } else if ( errno == EDEADLK) {
         VERBOSE(N_STDOUT, "%s [ERROR] pthread_join: Deadlock in service thread detected!\n", get_stats_formated_time());
      }
   }

   if (generate_backup == TRUE) {
      generate_backup_config_file();
   } else {
      for (x=0; ((unsigned int) x)<loaded_modules_cnt; x++) {
         if (running_modules[x].module_status == TRUE) {
            VERBOSE(N_STDOUT, "%s [WARNING] Some modules are still running, gonna generate backup anyway!\n", get_stats_formated_time());
            generate_backup_config_file();
            break;
         }
      }
   }

   for (x=0; ((unsigned int) x)<loaded_modules_cnt;x++) {
      if (running_modules[x].module_counters_array != NULL) {
         free(running_modules[x].module_counters_array);
         running_modules[x].module_counters_array = NULL;
      }
   }
   for (x=0;x<running_modules_array_size;x++) {
      for (y=0; y<running_modules[x].module_ifces_cnt; y++) {
         if (running_modules[x].module_ifces[y].ifc_note != NULL) {
            free(running_modules[x].module_ifces[y].ifc_note);
            running_modules[x].module_ifces[y].ifc_note = NULL;
         }
         if (running_modules[x].module_ifces[y].ifc_type != NULL) {
            free(running_modules[x].module_ifces[y].ifc_type);
            running_modules[x].module_ifces[y].ifc_type = NULL;
         }
         if (running_modules[x].module_ifces[y].ifc_direction != NULL) {
            free(running_modules[x].module_ifces[y].ifc_direction);
            running_modules[x].module_ifces[y].ifc_direction = NULL;
         }
         if (running_modules[x].module_ifces[y].ifc_params != NULL) {
            free(running_modules[x].module_ifces[y].ifc_params);
            running_modules[x].module_ifces[y].ifc_params = NULL;
         }
      }
      if (running_modules[x].module_ifces != NULL) {
         free(running_modules[x].module_ifces);
         running_modules[x].module_ifces = NULL;
      }
      if (running_modules[x].module_path != NULL) {
         free(running_modules[x].module_path);
         running_modules[x].module_path = NULL;
      }
      if (running_modules[x].module_name != NULL) {
         free(running_modules[x].module_name);
         running_modules[x].module_name = NULL;
      }
      if (running_modules[x].module_params != NULL) {
         free(running_modules[x].module_params);
         running_modules[x].module_params = NULL;
      }
   }
   if (running_modules != NULL) {
      free(running_modules);
      running_modules = NULL;
   }

   if (config_file != NULL) {
      free(config_file);
      config_file = NULL;
   }
   if (logs_path != NULL) {
      free(logs_path);
      logs_path = NULL;
   }

   modules_profile_t * ptr = first_profile_ptr;
   modules_profile_t * p = NULL;
   while (ptr != NULL) {
      p = ptr;
      ptr = ptr->next;
      if (p->profile_name != NULL) {
         free(p->profile_name);
      }
      if (p != NULL) {
         free(p);
      }
   }

   if (daemon_flag && (daemon_internals != NULL)) {
      free_daemon_internals_variables();
      if (daemon_internals->daemon_sd > 0) {
         close(daemon_internals->daemon_sd);
         daemon_internals->daemon_sd = 0;
      }
      if (daemon_internals != NULL) {
         free(daemon_internals);
         daemon_internals = NULL;
      }
      unlink(socket_path);
   }

   free_output_file_strings_and_streams();
}

char *get_param_by_delimiter(const char *source, char **dest, const char delimiter)
{
   char *param_end = NULL;
   unsigned int param_size = 0;

   if (source == NULL) {
      return NULL;
   }

   param_end = strchr(source, delimiter);
   if (param_end == NULL) {
      /* no delimiter found, copy the whole source */
      *dest = strdup(source);
      return NULL;
   }

   param_size = param_end - source;
   *dest = (char *) calloc(1, param_size + 1);
   if (*dest == NULL) {
      return (NULL);
   }
   strncpy(*dest, source, param_size);
   return param_end + 1;
}

long int get_total_cpu_usage()
{
   long int new_total_cpu_usage = 0;
   FILE * proc_stat_fd = fopen("/proc/stat","r");
   int x = 0, num = 0;

   if (proc_stat_fd == NULL) {
      return -1;
   } else if (fscanf(proc_stat_fd,"cpu") != 0) {
      fclose(proc_stat_fd);
      return -1;
   }

   for (x=0; x<10; x++) {
      if (!fscanf(proc_stat_fd,"%d",&num)) {
         continue;
      }
      new_total_cpu_usage += num;
   }

   fclose(proc_stat_fd);
   return new_total_cpu_usage;
}

void update_module_cpu_usage(long int * last_total_cpu_usage)
{
   int utime = 0, stime = 0;
   unsigned int x = 0;
   FILE * proc_stat_fd = NULL;
   char path[20];
   memset(path,0,20);
   long int new_total_cpu_usage = get_total_cpu_usage();
   long int difference_total = new_total_cpu_usage - *last_total_cpu_usage;

   *last_total_cpu_usage = new_total_cpu_usage;

   for (x=0; x<loaded_modules_cnt; x++) {
      if (running_modules[x].module_status) {
         sprintf(path,"/proc/%d/stat",running_modules[x].module_pid);
         proc_stat_fd = fopen(path,"r");
         if (!fscanf(proc_stat_fd,"%*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %d %d", &utime , &stime)) {
            continue;
         }
         if (running_modules[x].total_cpu_usage_during_module_startup != -1) {
            running_modules[x].overall_percent_module_cpu_usage_kernel_mode = 100 * ((float)stime/(float)(new_total_cpu_usage - running_modules[x].total_cpu_usage_during_module_startup));
            running_modules[x].overall_percent_module_cpu_usage_user_mode = 100 * ((float)utime/(float)(new_total_cpu_usage - running_modules[x].total_cpu_usage_during_module_startup));
         } else {
            running_modules[x].overall_percent_module_cpu_usage_kernel_mode = 0;
            running_modules[x].overall_percent_module_cpu_usage_user_mode = 0;
         }
         running_modules[x].last_period_percent_cpu_usage_kernel_mode = 100 * (stime - running_modules[x].last_period_cpu_usage_kernel_mode)/difference_total;
         running_modules[x].last_period_percent_cpu_usage_user_mode = 100 * (utime - running_modules[x].last_period_cpu_usage_user_mode)/difference_total;
         running_modules[x].last_period_cpu_usage_kernel_mode = stime;
         running_modules[x].last_period_cpu_usage_user_mode = utime;
         fclose(proc_stat_fd);
      }
   }
}

int service_get_data(int sd, int running_module_number)
{
   int sizeof_recv = (running_modules[running_module_number].module_num_in_ifc + 3*running_modules[running_module_number].module_num_out_ifc) * sizeof(uint64_t);
   int total_receved = 0;
   int last_receved = 0;
   char * data_pointer = (char *) running_modules[running_module_number].module_counters_array;

   while (total_receved < sizeof_recv) {
      last_receved = recv(sd, data_pointer + total_receved, sizeof_recv - total_receved, MSG_DONTWAIT);
      if (last_receved == 0) {
         VERBOSE(STATISTICS,"! Modules service thread closed its socket, im done !\n");
         return 0;
      } else if (last_receved == -1) {
         if (errno == EAGAIN  || errno == EWOULDBLOCK) {
            return 0;
         }
         VERBOSE(MODULE_EVENT,"[SERVICE] Error while receiving from module %d_%s !\n", running_module_number, running_modules[running_module_number].module_name);
         return 0;
      }
      total_receved += last_receved;
   }
   return 1;
}

void connect_to_module_service_ifc(int module, int num_ifc)
{
   char * dest_port;
   int sockfd;
   union tcpip_socket_addr addr;

   // Increase counter of connection attempts to the service interface
   running_modules[module].module_service_ifc_conn_attempts++;

   if (running_modules[module].module_ifces[num_ifc].ifc_params == NULL) {
      running_modules[module].module_service_ifc_isconnected = FALSE;
      return;
   }
   get_param_by_delimiter(running_modules[module].module_ifces[num_ifc].ifc_params, &dest_port, ',');
   VERBOSE(MODULE_EVENT,"%s [SERVICE] Connecting to module %s on port %s...\n", get_stats_formated_time(), running_modules[module].module_name, dest_port);

   memset(&addr, 0, sizeof(addr));

   addr.unix_addr.sun_family = AF_UNIX;
   snprintf(addr.unix_addr.sun_path, sizeof(addr.unix_addr.sun_path) - 1, MODULES_UNIXSOCKET_PATH_FILENAME_FORMAT, dest_port);
   sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (sockfd == -1) {
      VERBOSE(MODULE_EVENT,"%s [SERVICE] Error while opening socket for connection with module %s.\n", get_stats_formated_time(), running_modules[module].module_name);
      running_modules[module].module_service_ifc_isconnected = FALSE;
      if (dest_port != NULL) {
         free(dest_port);
         dest_port = NULL;
      }
      return;
   }
   if (connect(sockfd, (struct sockaddr *) &addr.unix_addr, sizeof(addr.unix_addr)) == -1) {
      VERBOSE(MODULE_EVENT,"%s [SERVICE] Error while connecting to module %s on port %s\n", get_stats_formated_time(), running_modules[module].module_name, dest_port);
      running_modules[module].module_service_ifc_isconnected = FALSE;
      if (dest_port != NULL) {
         free(dest_port);
         dest_port = NULL;
      }
      close(sockfd);
      return;
   }
   running_modules[module].module_service_sd = sockfd;
   running_modules[module].module_service_ifc_isconnected = TRUE;
   VERBOSE(MODULE_EVENT,"%s [SERVICE] Connected to module %s.\n", get_stats_formated_time(), running_modules[module].module_name);
   if (dest_port != NULL) {
      free(dest_port);
      dest_port = NULL;
   }
}

void print_statistics(struct tm * timeinfo)
{
   unsigned int x = 0, y = 0;
   VERBOSE(STATISTICS,"------> %s", asctime(timeinfo));
   for (x=0;x<loaded_modules_cnt;x++) {
      if (running_modules[x].module_status) {
         VERBOSE(STATISTICS,"NAME:  %s; PID: %d; | ",
            running_modules[x].module_name,
            running_modules[x].module_pid);
         if (running_modules[x].module_has_service_ifc && running_modules[x].module_service_ifc_isconnected) {
            VERBOSE(STATISTICS,"CNT_RM:  ");
            for (y=0; y<running_modules[x].module_num_in_ifc; y++) {
               VERBOSE(STATISTICS,"%"PRIu64"  ", running_modules[x].module_counters_array[y]);
            }
            VERBOSE(STATISTICS,"CNT_SM:  ");
            for (y=0; y<running_modules[x].module_num_out_ifc; y++) {
               VERBOSE(STATISTICS,"%"PRIu64"  ", running_modules[x].module_counters_array[y + running_modules[x].module_num_in_ifc]);
            }
            VERBOSE(STATISTICS,"CNT_SB:  ");
            for (y=0; y<running_modules[x].module_num_out_ifc; y++) {
               VERBOSE(STATISTICS,"%"PRIu64"  ", running_modules[x].module_counters_array[y + running_modules[x].module_num_in_ifc + running_modules[x].module_num_out_ifc]);
            }
            VERBOSE(STATISTICS,"CNT_AF:  ");
            for (y=0; y<running_modules[x].module_num_out_ifc; y++) {
               VERBOSE(STATISTICS,"%"PRIu64"  ", running_modules[x].module_counters_array[y + running_modules[x].module_num_in_ifc + 2*running_modules[x].module_num_out_ifc]);
            }
         }
         VERBOSE(STATISTICS,"\n");
      }
   }
}

void print_statistics_legend()
{
   VERBOSE(STATISTICS,"Legend CPU stats:\n"
                        "\tO_S - overall kernel mode percentage (overall system)\n"
                        "\tO_U - overall user mode percentage (overall user)\n"
                        "\tP_S - periodic kernel mode percentage (periodic system)\n"
                        "\tP_U - periodic user mode percentage (periodic user)\n"
                        "Legend messages stats:\n"
                        "\tCNT_RM - counter of received messages of one trap interface (INPUT IFC)\n"
                        "\tCNT_SM - counter of sent messages of one trap interface (OUTPUT IFC)\n"
                        "\tCNT_SB - send buffer counter of one trap interface (OUTPUT IFC)\n"
                        "\tCNT_AF - autoflush counter of one trap interface (OUTPUT IFC)\n");
}

void *service_thread_routine(void *arg __attribute__ ((unused)))
{
   int some_module_running = FALSE;
   int sizeof_intptr = 4;
   unsigned int x,y;

   time_t rawtime;
   struct tm * timeinfo;

   while (TRUE) {
      pthread_mutex_lock(&running_modules_lock);
      time(&rawtime);
      timeinfo = localtime(&rawtime);

      some_module_running = service_update_module_status();
      if (service_thread_continue == FALSE) {
         if (service_stop_all_modules == FALSE) {
            VERBOSE(N_STDOUT, "%s [WARNING] I let modules continue running!\n", get_stats_formated_time());
            break;
         } else if (some_module_running == FALSE) {
            VERBOSE(N_STDOUT, "%s [WARNING] I stopped all modules!\n", get_stats_formated_time());
            break;
         }
      }
      service_restart_modules();
      service_stop_modules_sigint();

      pthread_mutex_unlock(&running_modules_lock);
      sleep(1);
      pthread_mutex_lock(&running_modules_lock);

      service_clean_after_children();
      some_module_running = service_update_module_status();
      service_stop_modules_sigkill();
      service_clean_after_children();

      for (y=0; y<loaded_modules_cnt; y++) {
         if (running_modules[y].module_served_by_service_thread == FALSE) {
            if (running_modules[y].remove_module == TRUE) {
               if (running_modules[y].module_status == FALSE) {
                  VERBOSE(N_STDOUT, "[WARNING] %s is gonna be deleted right now, see ya!\n", running_modules[y].module_name);
                  free_module_and_shift_array(y);
               }
            } else if (running_modules[y].init_module) {
               if (running_modules[y].module_status == FALSE) {
                  VERBOSE(N_STDOUT, "[WARNING] %s was stopped and is gonna be started again\n", running_modules[y].module_name);
                  running_modules[y].module_enabled = TRUE;
                  running_modules[y].module_restart_cnt = -1;
                  running_modules[y].init_module = FALSE;
                  running_modules[y].module_served_by_service_thread = TRUE;
               }
            } else {
               running_modules[y].module_served_by_service_thread = TRUE;
            }
         }
      }

      some_module_running = service_update_module_status();
      int request[1];
      request[0] = 1;
      for (x=0;x<loaded_modules_cnt;x++) {
         if (running_modules[x].module_has_service_ifc == TRUE && running_modules[x].module_status == TRUE) {
            if (running_modules[x].module_service_ifc_isconnected == FALSE && (running_modules[x].module_service_ifc_conn_attempts < SERVICE_IFC_CONN_ATTEMPTS_LIMIT)) {
               y=0;
               while (1) {
                  if (running_modules[x].module_ifces[y].ifc_type != NULL) {
                     if ((strncmp(running_modules[x].module_ifces[y].ifc_type, "SERVICE", 7) == 0)) {
                        break;
                     }
                  }
                  y++;
               }
               if (running_modules[x].module_service_sd != -1) {
                  close(running_modules[x].module_service_sd);
                  running_modules[x].module_service_sd = -1;
               }
               connect_to_module_service_ifc(x,y);
               if (running_modules[x].module_service_ifc_conn_attempts >= SERVICE_IFC_CONN_ATTEMPTS_LIMIT) {
                  VERBOSE(MODULE_EVENT,"%s [SERVICE] Connection attempts to service interface of module %s reached 3, enough trying!\n", get_stats_formated_time(), running_modules[x].module_name);
               }
            }
            if (running_modules[x].module_service_ifc_isconnected == TRUE) {
               if (send(running_modules[x].module_service_sd,(void *) request, sizeof_intptr, 0) == -1) {
                  VERBOSE(MODULE_EVENT,"[SERVICE] Error while sending request to module %d_%s.\n",x,running_modules[x].module_name);
                  running_modules[x].module_service_ifc_isconnected = FALSE;
               }
            }
         }
      }
      some_module_running = service_update_module_status();
      for (x=0;x<loaded_modules_cnt;x++) {
         if (running_modules[x].module_has_service_ifc == TRUE && running_modules[x].module_status == TRUE) {
            if (running_modules[x].module_service_ifc_isconnected == TRUE) {
               if (!(service_get_data(running_modules[x].module_service_sd, x))) {
                  continue;
               }
            }
         }
      }

      pthread_mutex_unlock(&running_modules_lock);

      if ((verbose_flag == TRUE) && (some_module_running == TRUE)) {
         print_statistics(timeinfo);
      }

      if (service_thread_continue == TRUE) {
         sleep(1);
      }
   }

   for (x=0;x<loaded_modules_cnt;x++) {
      if ((running_modules[x].module_has_service_ifc == TRUE) && (running_modules[x].module_service_ifc_isconnected == TRUE)) {
         VERBOSE(MODULE_EVENT,"%s [SERVICE] Disconnecting from module %s\n", get_stats_formated_time(), running_modules[x].module_name);
         if (running_modules[x].module_service_sd != -1) {
            close(running_modules[x].module_service_sd);
         }
      }
   }

   pthread_exit(EXIT_SUCCESS);
}

char * make_formated_statistics()
{
   unsigned int size_of_buffer = 5*DEFAULT_SIZE_OF_BUFFER;
   char * buffer = (char *) calloc (size_of_buffer, sizeof(char));
   unsigned int x, y, counter = 0;
   int ptr = 0;

   for (x=0; x<loaded_modules_cnt; x++) {
      if(running_modules[x].module_status) {
         counter = 0;
         for (y=0; y<running_modules[x].module_ifces_cnt; y++) {
            if(running_modules[x].module_ifces[y].ifc_direction != NULL) {
               if(strcmp(running_modules[x].module_ifces[y].ifc_direction, "IN") == 0) {
                  ptr += sprintf(buffer + ptr, "%s,in,%d,%"PRIu64"\n", running_modules[x].module_name, counter, running_modules[x].module_counters_array[counter]);
                  counter++;
                  if (strlen(buffer) >= (3*size_of_buffer)/5) {
                     size_of_buffer += size_of_buffer/2;
                     buffer = (char *) realloc (buffer, size_of_buffer * sizeof(char));
                     memset(buffer + ptr, 0, size_of_buffer - ptr);
                  }
               }
            }
         }
         counter = 0;
         for (y=0; y<running_modules[x].module_ifces_cnt; y++) {
            if(running_modules[x].module_ifces[y].ifc_direction != NULL) {
               if(strcmp(running_modules[x].module_ifces[y].ifc_direction, "OUT") == 0) {
                  ptr += sprintf(buffer + ptr, "%s,out,%d,%"PRIu64",%"PRIu64",%"PRIu64"\n", running_modules[x].module_name, counter, running_modules[x].module_counters_array[counter + running_modules[x].module_num_in_ifc],
                                                                           running_modules[x].module_counters_array[counter + running_modules[x].module_num_in_ifc + running_modules[x].module_num_out_ifc],
                                                                           running_modules[x].module_counters_array[counter + running_modules[x].module_num_in_ifc + 2*running_modules[x].module_num_out_ifc]);
                  counter++;
                  if (strlen(buffer) >= (3*size_of_buffer)/5) {
                     size_of_buffer += size_of_buffer/2;
                     buffer = (char *) realloc (buffer, size_of_buffer * sizeof(char));
                     memset(buffer + ptr, 0, size_of_buffer - ptr);
                  }
               }
            }
         }
      }
   }

   for (x=0; x<loaded_modules_cnt; x++) {
      if(running_modules[x].module_status) {
         ptr += sprintf(buffer + ptr, "%s,cpu,%d,%d\n", running_modules[x].module_name, running_modules[x].last_period_percent_cpu_usage_kernel_mode, running_modules[x].last_period_percent_cpu_usage_user_mode);
         if (strlen(buffer) >= (3*size_of_buffer)/5) {
            size_of_buffer += size_of_buffer/2;
            buffer = (char *) realloc (buffer, size_of_buffer * sizeof(char));
            memset(buffer + ptr, 0, size_of_buffer - ptr);
         }
      }
   }

   return buffer;
}

void start_service_thread()
{
   pthread_attr_t attr;
   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

   pthread_create(&service_thread_id,  &attr, service_thread_routine, NULL);
}

int parse_arguments(int *argc, char **argv)
{
   char c;
   while (1) {
      int this_option_optind = optind ? optind : 1;
      int option_index = 0;
      static struct option long_options[] = {
         {"daemon", no_argument, 0, 'd'},
         {"config-file",  required_argument,    0, 'f'},
         {"help", no_argument,           0,  'h' },
         {"verbose",  no_argument,       0,  'v' },
         {"daemon-socket",  required_argument,  0, 's'},
         {"logs-path",  required_argument,  0, 'L'},
         {0, 0, 0, 0}
      };

      c = getopt_long(*argc, argv, "df:hvs:L:", long_options, &option_index);
      if (c == -1) {
         break;
      }

      switch (c) {
      case 'h':
         printf("Usage: supervisor [-d|--daemon] -f|--config-file=path [-h|--help] [-v|--verbose] [-L|--logs-path] [-s|--daemon-socket=path]\n");
         return FALSE;
      case 's':
         socket_path = optarg;
         break;
      case 'v':
         verbose_flag = TRUE;
         break;
      case 'f':
         config_file = strdup(optarg);
         file_flag = TRUE;
         break;
      case 'd':
         daemon_flag = TRUE;
         break;
      case 'L':
         logs_path = strdup(optarg);
         break;
      }
   }

   if (socket_path == NULL) {
      /* socket_path was not set by user, use default value. */
      socket_path = DEFAULT_DAEMON_UNIXSOCKET_PATH_FILENAME;
   }
   if (config_file == NULL) {
      fprintf(stderr, "Missing required config file (-f|--config-file).\n");
      return FALSE;
   }
   if (strstr(config_file, ".xml") == NULL) {
      if (config_file != NULL) {
         free(config_file);
         config_file = NULL;
      }
      fprintf(stderr, "File does not have expected .xml extension.\n");
      return FALSE;
   }
   return TRUE;
}

int daemon_init()
{
   // create daemon
   pid_t process_id = 0;
   pid_t sid = 0;

   fflush(stdout);
   process_id = fork();
   if (process_id < 0)  {
      VERBOSE(N_STDOUT,"%s [ERROR] Fork: could not initialize daemon process!\n", get_stats_formated_time());
      exit(EXIT_FAILURE);
   } else if (process_id > 0) {
      if (config_file != NULL) {
         free(config_file);
         config_file = NULL;
      }
      if (logs_path != NULL) {
         free(logs_path);
         logs_path = NULL;
      }
      VERBOSE(N_STDOUT,"%s [INFO] PID of daemon process: %d.\n", get_stats_formated_time(), process_id);
      exit(EXIT_SUCCESS);
   }

   umask(0);
   sid = setsid();
   if (sid < 0) {
      VERBOSE(N_STDOUT,"[ERROR] Setsid: calling process is process group leader!\n");
      exit(EXIT_FAILURE);
   }

   // allocate daemon_internals
   daemon_internals = (daemon_internals_t *) calloc (1, sizeof(daemon_internals_t));
   if (daemon_internals == NULL) {
      fprintf(stderr, "%s [ERROR] Could not allocate dameon_internals, cannot proceed without it!\n", get_stats_formated_time());
      exit(EXIT_FAILURE);
   }

   // create socket
   union tcpip_socket_addr addr;
   memset(&addr, 0, sizeof(addr));
   addr.unix_addr.sun_family = AF_UNIX;
   snprintf(addr.unix_addr.sun_path, sizeof(addr.unix_addr.sun_path) - 1, "%s", socket_path);

   /* if socket file exists, it could be hard to create new socket and bind */
   unlink(socket_path); /* error when file does not exist is not a problem */
   daemon_internals->daemon_sd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (bind(daemon_internals->daemon_sd, (struct sockaddr *) &addr.unix_addr, sizeof(addr.unix_addr)) != -1) {
      chmod(socket_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
   } else {
      VERBOSE(N_STDOUT,"%s [ERROR] Bind: could not bind the daemon socket!\n", get_stats_formated_time());
      exit(EXIT_FAILURE);
   }

   if (listen(daemon_internals->daemon_sd, 0) == -1) {
      VERBOSE(N_STDOUT,"%s [ERROR] Listen: could not listen on the daemon socket!\n", get_stats_formated_time());
      exit(EXIT_FAILURE);
   }

   return 0;
}


int daemon_get_client()
{
   int error, ret_val;
   socklen_t len;
   struct sockaddr_storage remoteaddr; // client address
   socklen_t addrlen;
   int newclient;
   addrlen = sizeof remoteaddr;

   // handle new connection
   while(1) {
      newclient = accept(daemon_internals->daemon_sd, (struct sockaddr *)&remoteaddr, &addrlen);
      if (newclient == -1) {
         VERBOSE(N_STDOUT,"%s [ERROR] Accept: could not accept a new client!\n", get_stats_formated_time());
         return newclient;
      }

      error = 0;
      len = sizeof (error);
      ret_val = getsockopt (newclient, SOL_SOCKET, SO_ERROR, &error, &len );
      if (ret_val == 0) {
         VERBOSE(N_STDOUT, "Socket is up\n");
         break;
      }
   }
   VERBOSE(N_STDOUT,"%s [WARNING] New client has connected!\n", get_stats_formated_time());
   return newclient;
}


int daemon_get_code_from_client()
{
   int bytes_to_read = 0; // value can be also -1 <=> ioctl error
   int ret_val = 0;
   int request = -1;
   char * buffer = NULL;
   fd_set read_fds;
   struct timeval tv;

   while (1) {
         FD_ZERO(&read_fds);
         FD_SET(daemon_internals->client_input_stream_fd, &read_fds);

         tv.tv_sec = 2;
         tv.tv_usec = 0;

         ret_val = select(daemon_internals->client_input_stream_fd+1, &read_fds, NULL, NULL, &tv);
         if (ret_val == -1) {
            // select error, return -1 and wait for new client
            return -1;
         } else if (ret_val != 0) {
            if (FD_ISSET(daemon_internals->client_input_stream_fd, &read_fds)) {
               ioctl(daemon_internals->client_input_stream_fd, FIONREAD, &bytes_to_read);
               if (bytes_to_read == 0 || bytes_to_read == -1) {
                  // client has disconnected, return -2 and wait for new client
                  return -2;
               }

               buffer = get_input_from_stream(input_fd);
               if (buffer == NULL) {
                  // problem with input, return -1 and wait for new client
                  return -1;
               }
               if (sscanf(buffer, "%d", &request) != 1) {
                  // wrong format of code, return -1 and wait for new client
                  free(buffer);
                  return -1;
               }
               free(buffer);

               switch (request) {
               case DAEMON_CONFIG_MODE_CODE:
                  return DAEMON_CONFIG_MODE_CODE;

               case DAEMON_RELOAD_MODE_CODE:
                  return DAEMON_RELOAD_MODE_CODE;

               case DAEMON_STATS_MODE_CODE:
                  return DAEMON_STATS_MODE_CODE;

               default:
                  // unknown code, return -1 and wait for new client
                  return -1;
               }
            }
         } else {
            // timeout instead of mode-code, return -3 and wait for new client
            return -3;
         }
      }
}

void send_options_to_client()
{
   usleep(50000); // Solved bugged output - without this sleep, escape codes in output were not sometimes reseted on time and they were applied also on this menu
   VERBOSE(N_STDOUT, ANSI_CYAN_BOLD "--------OPTIONS--------\n" ANSI_ATTR_RESET);
   VERBOSE(N_STDOUT, ANSI_CYAN "1. START ALL MODULES\n");
   VERBOSE(N_STDOUT, "2. STOP ALL MODULES\n");
   VERBOSE(N_STDOUT, "3. START MODULE\n");
   VERBOSE(N_STDOUT, "4. STOP MODULE\n");
   VERBOSE(N_STDOUT, "5. STARTED MODULES STATUS\n");
   VERBOSE(N_STDOUT, "6. AVAILABLE MODULES\n");
   VERBOSE(N_STDOUT, "7. RELOAD CONFIGURATION\n");
   VERBOSE(N_STDOUT, "-- Type \"Cquit\" to exit client --\n");
   VERBOSE(N_STDOUT, "-- Type \"Dstop\" to stop daemon --\n" ANSI_ATTR_RESET);
   VERBOSE(N_STDOUT, ANSI_YELLOW_BOLD "[INTERACTIVE] Your choice: " ANSI_ATTR_RESET);
}


void daemon_mode()
{
   long int last_total_cpu_usage = get_total_cpu_usage();
   int bytes_to_read = 0; // value can be also -1 <=> ioctl error
   int ret_val = 0, nine_cnt = 0;
   int request = -1;
   fd_set read_fds;
   struct timeval tv;

   daemon_internals->daemon_terminated = FALSE;
   daemon_internals->client_connected = FALSE;

   while (daemon_internals->daemon_terminated == FALSE) {

      // get new client
      daemon_internals->client_sd = daemon_get_client();
      if (daemon_internals->client_sd == -1) {
         daemon_internals->client_connected = FALSE;
         continue;
      }

      // open input stream on client' s socket
      daemon_internals->client_input_stream = fdopen(daemon_internals->client_sd, "r");
      if (daemon_internals->client_input_stream == NULL) {
         VERBOSE(N_STDOUT,"%s [ERROR] Fdopen: could not open client input stream!\n", get_stats_formated_time());
         free_daemon_internals_variables();
         continue;
      }

      // open output stream on client' s socket
      daemon_internals->client_output_stream = fdopen(daemon_internals->client_sd, "w");
      if (daemon_internals->client_output_stream == NULL) {
         VERBOSE(N_STDOUT,"%s [ERROR] Fdopen: could not open client output stream!\n", get_stats_formated_time());
         free_daemon_internals_variables();
         continue;
      }

      // get file descriptor of input stream on client' s socket
      daemon_internals->client_input_stream_fd = fileno(daemon_internals->client_input_stream);
      if (daemon_internals->client_input_stream_fd < 0) {
         VERBOSE(N_STDOUT,"%s [ERROR] Fileno: could not get client input stream descriptor!\n", get_stats_formated_time());
         free_daemon_internals_variables();
         continue;
      }

      // I/O streams are opened and client is connected
      daemon_internals->client_connected = TRUE;
      input_fd = daemon_internals->client_input_stream;
      output_fd = daemon_internals->client_output_stream;

      // get code from client according to operation he wants to perform
      switch (daemon_get_code_from_client()) {
      case -3: // timeout
         input_fd = stdin;
         output_fd = supervisor_log_fd;
         VERBOSE(N_STDOUT, "[ERROR] Timeout, client has not sent mode-code -> gonna wait for new client\n");
         free_daemon_internals_variables();
         continue;

      case -2: // client has disconnected
         input_fd = stdin;
         output_fd = supervisor_log_fd;
         VERBOSE(N_STDOUT, "[ERROR] Client has disconnected -> gonna wait for new client\n");
         free_daemon_internals_variables();
         continue;

      case -1: // another error while receiving mode-code from client
         input_fd = stdin;
         output_fd = supervisor_log_fd;
         VERBOSE(N_STDOUT, "[ERROR] Error while waiting for a mode-code from client -> gonna wait for new client\n");
         free_daemon_internals_variables();
         continue;

      case DAEMON_CONFIG_MODE_CODE: // normal client configure mode -> continue to options loop
         send_options_to_client();
         break;

      case DAEMON_RELOAD_MODE_CODE: // just reload configuration and wait for new client
         input_fd = stdin;
         output_fd = supervisor_log_fd;
         VERBOSE(N_STDOUT, "%s [RELOAD] Got request from client to reload...\n", get_stats_formated_time());
         free_daemon_internals_variables();
         reload_configuration(RELOAD_DEFAULT_CONFIG_FILE, NULL);
         continue;

      case DAEMON_STATS_MODE_CODE: { // send stats to current client and wait for new one
         update_module_cpu_usage(&last_total_cpu_usage);
         char * stats_buffer = make_formated_statistics();
         int buffer_len = strlen(stats_buffer);
         char stats_buffer2[buffer_len+1];
         memset(stats_buffer2,0,buffer_len+1);
         strncpy(stats_buffer2, stats_buffer, buffer_len+1);
         VERBOSE(N_STDOUT,"%s", stats_buffer2);
         if (stats_buffer != NULL) {
            free(stats_buffer);
            stats_buffer = NULL;
         }
         input_fd = stdin;
         output_fd = supervisor_log_fd;
         VERBOSE(N_STDOUT, "%s [WARNING] Stats sent to client!\n", get_stats_formated_time());
         free_daemon_internals_variables();
         continue;
      }

      default: // just in case of unknown return value.. clean up and wait for new client
         input_fd = stdin;
         output_fd = supervisor_log_fd;
         free_daemon_internals_variables();
         continue;
      }

      while (daemon_internals->client_connected) {
         request = -1;
         FD_ZERO(&read_fds);
         FD_SET(daemon_internals->client_input_stream_fd, &read_fds);

         tv.tv_sec = 1;
         tv.tv_usec = 0;

         ret_val = select(daemon_internals->client_input_stream_fd+1, &read_fds, NULL, NULL, &tv);
         if (ret_val == -1) {
            VERBOSE(N_STDOUT,"%s [ERROR] Select: error\n", get_stats_formated_time());
            free_daemon_internals_variables();
            input_fd = stdin;
            output_fd = supervisor_log_fd;
            break;
         } else if (ret_val != 0) {
            if (FD_ISSET(daemon_internals->client_input_stream_fd, &read_fds)) {
               ioctl(daemon_internals->client_input_stream_fd, FIONREAD, &bytes_to_read);
               if (bytes_to_read == 0 || bytes_to_read == -1) {
                  input_fd = stdin;
                  output_fd = supervisor_log_fd;
                  VERBOSE(N_STDOUT, "%s [WARNING] Client has disconnected!\n", get_stats_formated_time());
                  free_daemon_internals_variables();
                  break;
               }

               request = get_number_from_input_choosing_option();

               switch (request) {
               case 1:
                  interactive_start_configuration();
                  break;
               case 2:
                  interactive_stop_configuration();
                  break;
               case 3:
                  interactive_set_module_enabled();
                  break;
               case 4:
                  interactive_stop_module();
                  break;
               case 5:
                  interactive_show_running_modules_status();
                  break;
               case 6:
                  interactive_show_available_modules();
                  break;
               case 7:
                  reload_configuration(RELOAD_INTERACTIVE, NULL);
                  break;
               case 9:
                  nine_cnt++;
                  if (nine_cnt == 3) {
                     daemon_internals->client_connected = FALSE;
                     daemon_internals->daemon_terminated = TRUE;
                  }
                  break;
               default:
                  VERBOSE(N_STDOUT, ANSI_RED_BOLD "[WARNING] Wrong input!\n" ANSI_ATTR_RESET);
                  break;
               }
               if (nine_cnt == 0 && !(daemon_internals->daemon_terminated) && daemon_internals->client_connected) {
                  send_options_to_client();
               }
            }
         } else {
            if (nine_cnt > 0) {
               VERBOSE(N_STDOUT, ANSI_RED_BOLD "[WARNING] Wrong input!\n" ANSI_ATTR_RESET);
               nine_cnt = 0;
               send_options_to_client();
            }
         }
      }
   }

   supervisor_termination(FALSE, FALSE);
   return;
}

int find_loaded_module(char * name)
{
   unsigned int x;
   for (x=0; x<loaded_modules_cnt; x++) {
      if (strcmp(running_modules[x].module_name, name) == 0) {
         return x;
      }
   }
   return -1;
}

void free_module_on_index(int index)
{
   unsigned int y;

   if (running_modules[index].module_counters_array != NULL) {
      free(running_modules[index].module_counters_array);
      running_modules[index].module_counters_array = NULL;
   }

   for (y=0; y<running_modules[index].module_ifces_cnt; y++) {
         if (running_modules[index].module_ifces[y].ifc_note != NULL) {
            free(running_modules[index].module_ifces[y].ifc_note);
            running_modules[index].module_ifces[y].ifc_note = NULL;
         }
         if (running_modules[index].module_ifces[y].ifc_type != NULL) {
            free(running_modules[index].module_ifces[y].ifc_type);
            running_modules[index].module_ifces[y].ifc_type = NULL;
         }
         if (running_modules[index].module_ifces[y].ifc_direction != NULL) {
            free(running_modules[index].module_ifces[y].ifc_direction);
            running_modules[index].module_ifces[y].ifc_direction = NULL;
         }
         if (running_modules[index].module_ifces[y].ifc_params != NULL) {
            free(running_modules[index].module_ifces[y].ifc_params);
            running_modules[index].module_ifces[y].ifc_params = NULL;
         }
      }
      if (running_modules[index].module_ifces != NULL) {
         free(running_modules[index].module_ifces);
         running_modules[index].module_ifces = NULL;
      }
      if (running_modules[index].module_path != NULL) {
         free(running_modules[index].module_path);
         running_modules[index].module_path = NULL;
      }
      if (running_modules[index].module_name != NULL) {
         free(running_modules[index].module_name);
         running_modules[index].module_name = NULL;
      }
      if (running_modules[index].module_params != NULL) {
         free(running_modules[index].module_params);
         running_modules[index].module_params = NULL;
      }
}

void free_module_interfaces_on_index(int index)
{
   unsigned int y;
   for (y=0; y<running_modules[index].module_ifces_cnt; y++) {
      if (running_modules[index].module_ifces[y].ifc_note != NULL) {
         free(running_modules[index].module_ifces[y].ifc_note);
         running_modules[index].module_ifces[y].ifc_note = NULL;
      }
      if (running_modules[index].module_ifces[y].ifc_type != NULL) {
         free(running_modules[index].module_ifces[y].ifc_type);
         running_modules[index].module_ifces[y].ifc_type = NULL;
      }
      if (running_modules[index].module_ifces[y].ifc_direction != NULL) {
         free(running_modules[index].module_ifces[y].ifc_direction);
         running_modules[index].module_ifces[y].ifc_direction = NULL;
      }
      if (running_modules[index].module_ifces[y].ifc_params != NULL) {
         free(running_modules[index].module_ifces[y].ifc_params);
         running_modules[index].module_ifces[y].ifc_params = NULL;
      }
   }
}

void reload_process_supervisor_element(reload_config_vars_t ** config_vars)
{
   xmlChar * key = NULL;
   int x = 0, number = 0;

   while ((*config_vars)->module_elem != NULL) {
      if (!xmlStrcmp((*config_vars)->module_elem->name, BAD_CAST "verbose")) {
         // Process supervisor's element "verbose"
         key = xmlNodeListGetString((*config_vars)->doc_tree_ptr, (*config_vars)->module_elem->xmlChildrenNode, 1);
         if (key != NULL) {
            if (xmlStrcmp(key, BAD_CAST "true") == 0) {
               verbose_flag = TRUE;
            } else if (xmlStrcmp(key, BAD_CAST "false") == 0) {
               verbose_flag = FALSE;
            }
         }
      } else if (!xmlStrcmp((*config_vars)->module_elem->name, BAD_CAST "module-restarts")) {
         // Process supervisor's element "module-restarts"
         key = xmlNodeListGetString((*config_vars)->doc_tree_ptr, (*config_vars)->module_elem->xmlChildrenNode, 1);
         if (key != NULL) {
            x = 0;
            if ((sscanf((const char *) key,"%d",&number) == 1) && (number >= 0)) {
               x = (unsigned int) number;
               max_restarts_per_minute_config = x;
            }
         }
      } else if (!xmlStrcmp((*config_vars)->module_elem->name, BAD_CAST "logs-directory")) {
         // Process supervisor's element "logs-directory"
         key = xmlNodeListGetString((*config_vars)->doc_tree_ptr, (*config_vars)->module_elem->xmlChildrenNode, 1);
         if (key != NULL) {
            if (xmlStrcmp(key, BAD_CAST logs_path) != 0) {
               if (logs_path != NULL) {
                  free(logs_path);
                  logs_path = NULL;
               }
               logs_path = (char *) xmlStrdup(key);
               create_output_dir();
               create_output_files_strings();
            }
         }
      }
      if (key != NULL) {
         xmlFree(key);
         key = NULL;
      }
      (*config_vars)->module_elem = (*config_vars)->module_elem->next;
   }
   return;
}

void reload_process_module_atribute(reload_config_vars_t ** config_vars, char ** module_ifc_atr)
{
   int str_len = 0;
   xmlChar * key = NULL;

   key = xmlNodeListGetString((*config_vars)->doc_tree_ptr, (*config_vars)->module_atr_elem->xmlChildrenNode, 1);
   if ((*config_vars)->module_modifying) {
      if (*module_ifc_atr != NULL && key != NULL) {
         if (xmlStrcmp(key, BAD_CAST *module_ifc_atr) != 0) {
            VERBOSE(N_STDOUT, "[WARNING] %s's attribute \"%s\" has been changed (%s -> %s), gonna update it.\n",
               running_modules[(*config_vars)->current_module_idx].module_name, (char *)(*config_vars)->module_atr_elem->name,
               *module_ifc_atr, (char *)key);
            running_modules[(*config_vars)->current_module_idx].module_modified_by_reload = TRUE;
            if (*module_ifc_atr != NULL) {
               free(*module_ifc_atr);
               *module_ifc_atr = NULL;
            }
            str_len = strlen((char *) key);
            *module_ifc_atr = (char *) xmlStrdup(key);
         }
      } else if (*module_ifc_atr == NULL && key == NULL) {
         // new one and old one NULL -> OK
      } else if (*module_ifc_atr == NULL) {
         VERBOSE(N_STDOUT, "[WARNING] %s's attribute \"%s\" should be empty, gonna update it.\n",
               running_modules[(*config_vars)->current_module_idx].module_name, (char *)(*config_vars)->module_atr_elem->name);
         running_modules[(*config_vars)->current_module_idx].module_modified_by_reload = TRUE;
         str_len = strlen((char *) key);
         *module_ifc_atr = (char *) xmlStrdup(key);
      } else if (key == NULL) {
         VERBOSE(N_STDOUT, "[WARNING] %s's attribute \"%s\" shouldn't be empty, gonna update it.\n",
               running_modules[(*config_vars)->current_module_idx].module_name, (char *)(*config_vars)->module_atr_elem->name);
         running_modules[(*config_vars)->current_module_idx].module_modified_by_reload = TRUE;
         if (*module_ifc_atr != NULL) {
            free(*module_ifc_atr);
            *module_ifc_atr = NULL;
         }
      }
   } else {
      if (key == NULL) {
         *module_ifc_atr = NULL;
      } else {
         str_len = strlen((char *) key);
         *module_ifc_atr = (char *) xmlStrdup(key);
      }
   }

   if (key != NULL) {
      xmlFree(key);
      key = NULL;
   }

   return;
}

int reload_process_module_interface_atribute(reload_config_vars_t ** config_vars, char ** module_ifc_atr)
{
   int str_len = 0;
   xmlChar * key = NULL;

   key = xmlNodeListGetString((*config_vars)->doc_tree_ptr, (*config_vars)->ifc_atr_elem->xmlChildrenNode, 1);
   if ((*config_vars)->module_modifying) {
      if (*module_ifc_atr != NULL && key != NULL) {
         if (xmlStrcmp(key, BAD_CAST *module_ifc_atr) != 0) {
            VERBOSE(N_STDOUT, "[WARNING] %s's interface attribute \"%s\" has been changed (%s -> %s), gonna update module's interfaces.\n",
               running_modules[(*config_vars)->current_module_idx].module_name, (*config_vars)->ifc_atr_elem->name,
               *module_ifc_atr, (char *)key);
            running_modules[(*config_vars)->current_module_idx].module_modified_by_reload = TRUE;
            free_module_interfaces_on_index((*config_vars)->current_module_idx);
            // ifc_cnt = -1;
            (*config_vars)->ifc_elem = (*config_vars)->module_atr_elem->xmlChildrenNode;
            running_modules[(*config_vars)->current_module_idx].module_ifces_cnt = -1;
            running_modules[(*config_vars)->current_module_idx].module_num_out_ifc = 0;
            running_modules[(*config_vars)->current_module_idx].module_num_in_ifc = 0;
            (*config_vars)->module_modifying = FALSE;

            if (key != NULL) {
               xmlFree(key);
               key = NULL;
            }
            return -1;
         }

         if (key != NULL) {
            xmlFree(key);
            key = NULL;
         }
      } else if (*module_ifc_atr == NULL && key == NULL) {
         // new one and old one NULL -> OK
      } else {
         if (key == NULL) {
            VERBOSE(N_STDOUT, "[WARNING] %s's interface attribute \"%s\" shouldn't be empty, gonna update module's interfaces.\n",
               running_modules[(*config_vars)->current_module_idx].module_name, (*config_vars)->ifc_atr_elem->name);
         } else {
            VERBOSE(N_STDOUT, "[WARNING] %s's interface attribute \"%s\" should be empty, gonna update module's interfaces.\n",
               running_modules[(*config_vars)->current_module_idx].module_name, (*config_vars)->ifc_atr_elem->name);
         }
         running_modules[(*config_vars)->current_module_idx].module_modified_by_reload = TRUE;
         free_module_interfaces_on_index((*config_vars)->current_module_idx);
         // ifc_cnt = -1;
         (*config_vars)->ifc_elem = (*config_vars)->module_atr_elem->xmlChildrenNode;
         running_modules[(*config_vars)->current_module_idx].module_ifces_cnt = -1;
         running_modules[(*config_vars)->current_module_idx].module_num_out_ifc = 0;
         running_modules[(*config_vars)->current_module_idx].module_num_in_ifc = 0;
         (*config_vars)->module_modifying = FALSE;

         if (key != NULL) {
            xmlFree(key);
            key = NULL;
         }
         return -1;
      }
   } else {
      if (key == NULL) {
         *module_ifc_atr = NULL;
      } else {
         str_len = strlen((char *) key);
         *module_ifc_atr = (char *) calloc (str_len+1, sizeof(char));
         strncpy(*module_ifc_atr , (char *) key, str_len+1);

         if (key != NULL) {
            xmlFree(key);
            key = NULL;
         }
      }
   }

   return 0;
}

void reload_check_modules_interfaces_count(reload_config_vars_t  ** config_vars)
{
   int original_module_ifc_cnt = running_modules[(*config_vars)->current_module_idx].module_ifces_cnt;
   int new_module_ifc_cnt = 0;

   while ((*config_vars)->ifc_elem != NULL) {
      if (!xmlStrcmp((*config_vars)->ifc_elem->name,BAD_CAST "interface")) {
         new_module_ifc_cnt++;
      }
      (*config_vars)->ifc_elem = (*config_vars)->ifc_elem->next;
   }

   (*config_vars)->ifc_elem = (*config_vars)->module_atr_elem->xmlChildrenNode;

   if (original_module_ifc_cnt != new_module_ifc_cnt) {
      running_modules[(*config_vars)->current_module_idx].module_modified_by_reload = TRUE;
      free_module_interfaces_on_index((*config_vars)->current_module_idx);
      running_modules[(*config_vars)->current_module_idx].module_ifces_cnt = 0;
      running_modules[(*config_vars)->current_module_idx].module_num_out_ifc = 0;
      running_modules[(*config_vars)->current_module_idx].module_num_in_ifc = 0;
      (*config_vars)->module_modifying = FALSE;
      VERBOSE(N_STDOUT,"[WARNING] Reloading module \"%s\" - original interface cnt:%d, actual interface cnt:%d -> gonna update module's interfaces.\n",
                                             running_modules[(*config_vars)->current_module_idx].module_name, original_module_ifc_cnt, new_module_ifc_cnt);
   }

   return;
}

int reload_find_and_check_module_basic_elements(reload_config_vars_t ** config_vars)
{
   int move_to_next_module = FALSE;
   int ret_val = 0;
   int last_module = FALSE;
   xmlChar * key = NULL;
   int basic_elements[2], name_elem_idx = 0, path_elem_idx = 1;
   memset(basic_elements, 0, 2*sizeof(int));

   while ((*config_vars)->module_atr_elem != NULL) {
      if ((!xmlStrcmp((*config_vars)->module_atr_elem->name, BAD_CAST "name"))) {
         key = xmlNodeListGetString((*config_vars)->doc_tree_ptr, (*config_vars)->module_atr_elem->xmlChildrenNode, 1);
         if (key == NULL) {
            basic_elements[name_elem_idx] = -1;
            move_to_next_module = TRUE;
         } else {
            basic_elements[name_elem_idx]++;
            if (basic_elements[name_elem_idx] == 1) {
               ret_val = find_loaded_module((char *) key);
               if (ret_val == -1) { // Module with this name was not found - gonna insert a new module
                  (*config_vars)->current_module_idx = loaded_modules_cnt;
                  (*config_vars)->module_modifying = FALSE;
               } else { // Found already loaded module with same name -> check it's values
                  (*config_vars)->current_module_idx = ret_val;
                  (*config_vars)->module_modifying = TRUE;
               }
            } else {
               move_to_next_module = TRUE;
            }
         }
      } else if ((!xmlStrcmp((*config_vars)->module_atr_elem->name,BAD_CAST "path"))) {
         key = xmlNodeListGetString((*config_vars)->doc_tree_ptr, (*config_vars)->module_atr_elem->xmlChildrenNode, 1);
         if (key == NULL) {
            basic_elements[path_elem_idx] = -1;
            move_to_next_module = TRUE;
         } else {
            basic_elements[path_elem_idx]++;
            if (basic_elements[path_elem_idx] > 1) {
               move_to_next_module = TRUE;
            }
         }
      }

      if (key != NULL) {
         xmlFree(key);
         key = NULL;
      }

      // If there is no more children of module element and path or name elements were not found, move to next module
      if ((*config_vars)->module_atr_elem->next == NULL && (basic_elements[name_elem_idx] == 0 || basic_elements[path_elem_idx] == 0)) {
         move_to_next_module = TRUE;
      }

      if (move_to_next_module) {
         if (basic_elements[name_elem_idx] > 1) {
            VERBOSE(N_STDOUT, "[WARNING] Reloading error - found more \"name\" elements in module -> moving to next module.\n");
         } else if (basic_elements[name_elem_idx] == 0) {
            VERBOSE(N_STDOUT, "[WARNING] Reloading error - didn't find \"name\" element in module -> moving to next module.\n");
         } else if (basic_elements[name_elem_idx] == -1) {
            VERBOSE(N_STDOUT, "[WARNING] Reloading error - found empty \"name\" element in module -> moving to next module.\n");
         } else if (basic_elements[path_elem_idx] > 1) {
            VERBOSE(N_STDOUT, "[WARNING] Reloading error - found more \"path\" elements in module -> moving to next module.\n");
         } else if (basic_elements[path_elem_idx] == 0) {
            VERBOSE(N_STDOUT, "[WARNING] Reloading error - didn't find \"path\" element in module -> moving to next module.\n");
         } else if (basic_elements[path_elem_idx] == -1) {
            VERBOSE(N_STDOUT, "[WARNING] Reloading error - found empty \"path\" element in module -> moving to next module.\n");
         }

         (*config_vars)->module_elem = (*config_vars)->module_elem->next;
         if ((*config_vars)->module_elem == NULL) {
            last_module = TRUE;
            break;
         } else {
            (*config_vars)->module_elem = (*config_vars)->module_elem->next;
            (*config_vars)->current_module_idx = -1;
            memset(basic_elements,0,2*sizeof(int));
            (*config_vars)->module_atr_elem = (*config_vars)->module_elem->xmlChildrenNode;
         }
         move_to_next_module = FALSE;
         continue;
      }

      (*config_vars)->module_atr_elem = (*config_vars)->module_atr_elem->next;
   }

   if (last_module) {
      return -1;
   }

   if ((basic_elements[0] != 1) || (basic_elements[1] != 1)) {
      return -1;
   }

   return 0;
}

int reload_find_and_check_modules_profile_basic_elements(reload_config_vars_t ** config_vars)
{
   int new_profile_enabled = FALSE;
   char * new_profile_name = NULL;
   xmlChar * key = NULL;
   int basic_elements[2], name_elem_idx = 0, enabled_elem_idx = 1;
   memset(basic_elements, 0, 2*sizeof(int));

   while ((*config_vars)->module_elem != NULL) {
      if (!xmlStrcmp((*config_vars)->module_elem->name,BAD_CAST "name")) {
         // Process modules element "name"
         key = xmlNodeListGetString((*config_vars)->doc_tree_ptr, (*config_vars)->module_elem->xmlChildrenNode, 1);
         if (key != NULL) {
            basic_elements[name_elem_idx]++;
            if (basic_elements[name_elem_idx] == 1) {
               new_profile_name = (char *) xmlStrdup(key);
            } else { // Found more than one modules element "name" -> invalid profile
               break;
            }
         } else { // Modules element "name" is empty -> invalid profile
            basic_elements[name_elem_idx] = -1;
            break;
         }
      } else if (!xmlStrcmp((*config_vars)->module_elem->name,BAD_CAST "enabled")) {
         // Process modules element "enabled"
         key = xmlNodeListGetString((*config_vars)->doc_tree_ptr, (*config_vars)->module_elem->xmlChildrenNode, 1);
         if (key != NULL) {
            basic_elements[enabled_elem_idx]++;
            if (basic_elements[enabled_elem_idx] == 1) {
               if (xmlStrcmp(key, BAD_CAST "true") == 0) {
                  new_profile_enabled = TRUE;
               } else {
                  new_profile_enabled = FALSE;
               }
            } else { // Found more than one modules element "enabled" -> invalid profile
               break;
            }
         } else { // Modules element "enabled" is empty -> invalid profile
            basic_elements[enabled_elem_idx] = -1;
            break;
         }
      }

      if (key != NULL) {
         xmlFree(key);
         key = NULL;
      }
      (*config_vars)->module_elem = (*config_vars)->module_elem->next;
   }

   if (key != NULL) {
      xmlFree(key);
      key = NULL;
   }

   if (basic_elements[name_elem_idx] != 1 || basic_elements[enabled_elem_idx] != 1) {
      // Invalid profile
      if (basic_elements[name_elem_idx] > 1) { // Found more "name" elements
         VERBOSE(N_STDOUT, "[WARNING] Reloading error - found more \"name\" elements in modules profile.\n");
      } else if (basic_elements[name_elem_idx] == -1) { // Found empty "name" element
         VERBOSE(N_STDOUT, "[WARNING] Reloading error - found empty \"name\" element in modules profile.\n");
      } else if (basic_elements[enabled_elem_idx] > 1) { // Found more "enabled" elements
         VERBOSE(N_STDOUT, "[WARNING] Reloading error - found more \"enabled\" elements in modules profile.\n");
      } else if (basic_elements[enabled_elem_idx] == -1) { // Found empty "enabled" element
         VERBOSE(N_STDOUT, "[WARNING] Reloading error - found empty \"enabled\" element in modules profile.\n");
      }
      if (new_profile_name != NULL) {
         free(new_profile_name);
         new_profile_name = NULL;
      }
      return -1;
   } else { // Valid profile -> allocate it
      if (first_profile_ptr == NULL) {
         first_profile_ptr = (modules_profile_t *) calloc (1, sizeof(modules_profile_t));
         first_profile_ptr->profile_name = new_profile_name;
         first_profile_ptr->profile_enabled = new_profile_enabled;
         first_profile_ptr->next = NULL;
         actual_profile_ptr = first_profile_ptr;
      } else {
         modules_profile_t * ptr = (modules_profile_t *) calloc (1, sizeof(modules_profile_t));
         ptr->profile_name = new_profile_name;
         ptr->profile_enabled = new_profile_enabled;
         ptr->next = NULL;
         actual_profile_ptr->next = ptr;
         actual_profile_ptr = ptr;
      }
   }
   return 0;
}

void reload_count_module_interfaces(reload_config_vars_t ** config_vars)
{
   int x = 0;

   running_modules[(*config_vars)->current_module_idx].module_has_service_ifc = FALSE;
   running_modules[(*config_vars)->current_module_idx].module_num_in_ifc = 0;
   running_modules[(*config_vars)->current_module_idx].module_num_in_ifc = 0;

   for (x=0; x<running_modules[(*config_vars)->current_module_idx].module_ifces_cnt; x++) {
      if (running_modules[(*config_vars)->current_module_idx].module_ifces[x].ifc_direction != NULL) {
         if (strncmp(running_modules[(*config_vars)->current_module_idx].module_ifces[x].ifc_direction, "IN", 2) == 0) {
            running_modules[(*config_vars)->current_module_idx].module_num_in_ifc++;
         } else if (strncmp(running_modules[(*config_vars)->current_module_idx].module_ifces[x].ifc_direction, "OUT", 3) == 0) {
            running_modules[(*config_vars)->current_module_idx].module_num_out_ifc++;
         }
      }
      if (running_modules[(*config_vars)->current_module_idx].module_ifces[x].ifc_type != NULL) {
         if (strncmp(running_modules[(*config_vars)->current_module_idx].module_ifces[x].ifc_type, "SERVICE", 7) == 0) {
            running_modules[(*config_vars)->current_module_idx].module_has_service_ifc = TRUE;
         }
      }
   }
   return;
}

void reload_check_module_allocated_interfaces(reload_config_vars_t ** config_vars, int ifc_cnt)
{
   int origin_size = 0;

   if (running_modules[(*config_vars)->current_module_idx].module_ifces_array_size == 0) {
      VERBOSE(N_STDOUT, "[WARNING] Reload - allocating \"%s\" interfaces memory.\n", running_modules[(*config_vars)->current_module_idx].module_name);
      running_modules[(*config_vars)->current_module_idx].module_ifces = (interface_t *) calloc(IFCES_ARRAY_START_SIZE, sizeof(interface_t));
      running_modules[(*config_vars)->current_module_idx].module_ifces_array_size = IFCES_ARRAY_START_SIZE;
      running_modules[(*config_vars)->current_module_idx].module_ifces_cnt = 0;
   } else if (ifc_cnt == running_modules[(*config_vars)->current_module_idx].module_ifces_array_size) {
      VERBOSE(N_STDOUT, "[WARNING] Reload - reallocating \"%s\" interfaces memory.\n", running_modules[(*config_vars)->current_module_idx].module_name);
      origin_size = running_modules[(*config_vars)->current_module_idx].module_ifces_array_size;
      running_modules[(*config_vars)->current_module_idx].module_ifces_array_size += running_modules[(*config_vars)->current_module_idx].module_ifces_array_size/2;
      running_modules[(*config_vars)->current_module_idx].module_ifces = (interface_t *) realloc (running_modules[(*config_vars)->current_module_idx].module_ifces, (running_modules[(*config_vars)->current_module_idx].module_ifces_array_size) * sizeof(interface_t));
      memset(running_modules[(*config_vars)->current_module_idx].module_ifces + origin_size,0,(origin_size/2)*sizeof(interface_t));
   }
}

void reload_check_running_modules_allocated_memory()
{
   int origin_size = 0, x = 0;

   if (loaded_modules_cnt == running_modules_array_size) {
      VERBOSE(N_STDOUT, "[WARNING] Reload - reallocating running_modules memory.\n");
      origin_size = running_modules_array_size;
      running_modules_array_size += running_modules_array_size/2;
      running_modules = (running_module_t * ) realloc (running_modules, (running_modules_array_size)*sizeof(running_module_t));
      memset(running_modules + origin_size,0,(origin_size/2)*sizeof(running_module_t));

      for (x=loaded_modules_cnt; x<running_modules_array_size; x++) {
         running_modules[x].module_ifces = (interface_t *) calloc (IFCES_ARRAY_START_SIZE, sizeof(interface_t));
         running_modules[x].module_running = FALSE;
         running_modules[x].module_ifces_array_size = IFCES_ARRAY_START_SIZE;
         running_modules[x].module_ifces_cnt = 0;
      }
   }
}

void reload_resolve_module_enabled(reload_config_vars_t ** config_vars, const int modules_got_profile)
{
   xmlChar * key = NULL;
   int config_module_enabled = FALSE;
   int profile_and_module_enabled_anded = FALSE;

   key = xmlNodeListGetString((*config_vars)->doc_tree_ptr, (*config_vars)->module_atr_elem->xmlChildrenNode, 1);
   if (key == NULL) {
      running_modules[(*config_vars)->current_module_idx].module_enabled = FALSE;
      return;
   } else {
      if (xmlStrncmp(key, BAD_CAST "true", xmlStrlen(key)) == 0) {
         config_module_enabled = TRUE;
      } else if (xmlStrncmp(key, BAD_CAST "false", xmlStrlen(key)) == 0){
         config_module_enabled = FALSE;
      }
      xmlFree(key);
      key = NULL;
   }

   if (modules_got_profile == TRUE) {
      profile_and_module_enabled_anded = actual_profile_ptr->profile_enabled & config_module_enabled;
      if (profile_and_module_enabled_anded == TRUE && running_modules[(*config_vars)->current_module_idx].module_enabled == FALSE) {
         running_modules[(*config_vars)->current_module_idx].module_restart_cnt = -1;
      }
      // If current module is new in configuration, just save the anded enabled flag and return
      if ((*config_vars)->module_modifying == FALSE) {
         running_modules[(*config_vars)->current_module_idx].module_enabled = profile_and_module_enabled_anded;
      } else if (profile_and_module_enabled_anded != running_modules[(*config_vars)->current_module_idx].module_enabled) {
         VERBOSE(N_STDOUT, "[WARNING] %s enabled flag has been modified: %s -> %s.\n",
            running_modules[(*config_vars)->current_module_idx].module_name,
            (running_modules[(*config_vars)->current_module_idx].module_enabled == TRUE ? "enabled" : "disabled"),
            (profile_and_module_enabled_anded == TRUE ? "enabled" : "disabled"));
         running_modules[(*config_vars)->current_module_idx].module_enabled = profile_and_module_enabled_anded;
      }
   } else {
      if (config_module_enabled == TRUE && running_modules[(*config_vars)->current_module_idx].module_enabled == FALSE) {
         running_modules[(*config_vars)->current_module_idx].module_restart_cnt = -1;
      }
      // If current module is new in configuration, just save the enabled flag and return
      if ((*config_vars)->module_modifying == FALSE) {
         running_modules[(*config_vars)->current_module_idx].module_enabled = config_module_enabled;
      } else if (config_module_enabled != running_modules[(*config_vars)->current_module_idx].module_enabled) {
         VERBOSE(N_STDOUT, "[WARNING] %s enabled flag has been modified: %s -> %s.\n",
            running_modules[(*config_vars)->current_module_idx].module_name,
            (running_modules[(*config_vars)->current_module_idx].module_enabled == TRUE ? "enabled" : "disabled"),
            (config_module_enabled == TRUE ? "enabled" : "disabled"));
         running_modules[(*config_vars)->current_module_idx].module_enabled = config_module_enabled;
      }
   }
   return;
}

int reload_configuration(const int choice, xmlNodePtr node)
{
   pthread_mutex_lock(&running_modules_lock);

   int modules_got_profile;
   unsigned int x = 0, y = 0;
   int number = 0;
   int ret_val = 0;
   unsigned int original_loaded_modules_cnt = loaded_modules_cnt;
   int already_loaded_modules_found[loaded_modules_cnt];
   memset(already_loaded_modules_found, 0, loaded_modules_cnt*sizeof(int));
   int str_len = 0;
   char * buffer = NULL;
   int ifc_cnt = 0;
   reload_config_vars_t * config_vars = (reload_config_vars_t *) calloc (1, sizeof(reload_config_vars_t));
   xmlChar * key = NULL;

   switch (choice) {
      case RELOAD_INIT_LOAD_CONFIG: {
            char file_name[100];
            memset(file_name,0,100);
            if (socket_path == NULL) {
               sprintf(file_name, "%s%s.xml", logs_path, DEFAULT_BACKUP_FILE_PREFIX); // TODO backup file path, what if logs dir path is changed after restart.. wont find backup file
            } else {
               char socket_path_without_s[100];
               memset(socket_path_without_s,0,100);
               int socket_path_without_s_length = 0;
               for (x=0; x<strlen(socket_path); x++) {
                  if (socket_path[x] != '/') {
                     socket_path_without_s[socket_path_without_s_length] = socket_path[x];
                     socket_path_without_s_length++;
                  }
               }
               sprintf(file_name, "%s%s_%s.xml", logs_path, DEFAULT_BACKUP_FILE_PREFIX, socket_path_without_s);
            }
            if (access(file_name, F_OK) != -1) {
               config_vars->doc_tree_ptr = xmlParseFile(file_name);
               unlink(file_name); // delete backup file after parsing, it wont be needed anymore
               VERBOSE(N_STDOUT, "%s [WARNING] I found backup file with my socket on path \"%s\" and I'm gonna load it!\n", get_stats_formated_time(), file_name);
            } else {
               if (errno == EACCES) {
                  VERBOSE(N_STDOUT, "%s [WARNING] I don't have permissions to access backup file path \"%s\", I'm gonna load default config file!\n", get_stats_formated_time(), file_name);
               } else if (errno == ENOENT) {
                  VERBOSE(N_STDOUT, "%s [WARNING] Backup file with path \"%s\" does not exist, I'm gonna load default config file!\n", get_stats_formated_time(), file_name);
               }
               config_vars->doc_tree_ptr = xmlParseFile(config_file);
            }
            if (config_vars->doc_tree_ptr == NULL) {
               fprintf(stderr,"Document not parsed successfully. \n");
               pthread_mutex_unlock(&running_modules_lock);
               free(config_vars);
               return FALSE;
            }
            config_vars->current_node = xmlDocGetRootElement(config_vars->doc_tree_ptr);
         }
         break;

      case RELOAD_DEFAULT_CONFIG_FILE:
         config_vars->doc_tree_ptr = xmlParseFile(config_file);
         if (config_vars->doc_tree_ptr == NULL) {
            fprintf(stderr,"Document not parsed successfully. \n");
            pthread_mutex_unlock(&running_modules_lock);
            free(config_vars);
            return FALSE;
         }
         config_vars->current_node = xmlDocGetRootElement(config_vars->doc_tree_ptr);
         break;

      case RELOAD_CALLBACK_ROOT_ELEM:
         config_vars->current_node = node;
         break;

      case RELOAD_INTERACTIVE: {
         /*Get the name of a new config file */
         VERBOSE(N_STDOUT, ANSI_YELLOW_BOLD "[INTERACTIVE] Type in a name of the xml file to be loaded including \".xml\"; ("  "to reload same config file"  " with path \"%s\" type \"default\" or " "to cancel reloading" " type \"cancel\"): " ANSI_ATTR_RESET, config_file);
         buffer = get_input_from_stream(input_fd);
         if (buffer == NULL) {
            fprintf(stderr,"[ERROR] Input error.\n");
            pthread_mutex_unlock(&running_modules_lock);
            free(config_vars);
            return FALSE;
         } else if (strlen(buffer) == 0) {
            VERBOSE(N_STDOUT, ANSI_RED_BOLD "[WARNING] Wrong input - empty string.\n" ANSI_ATTR_RESET);
            free(buffer);
            pthread_mutex_unlock(&running_modules_lock);
            free(config_vars);
            return FALSE;
         } else if (strcmp(buffer, "cancel") == 0) {
            VERBOSE(N_STDOUT, ANSI_RED_BOLD "[WARNING] Reload configuration canceled.\n" ANSI_ATTR_RESET);
            free(buffer);
            pthread_mutex_unlock(&running_modules_lock);
            free(config_vars);
            return FALSE;
         } else if (strcmp(buffer, "default") == 0) {
            config_vars->doc_tree_ptr = xmlParseFile(config_file);
         } else {
            config_vars->doc_tree_ptr = xmlParseFile(buffer);
         }
         free(buffer);
         if (config_vars->doc_tree_ptr == NULL) {
            fprintf(stderr,"Document not parsed successfully. \n");
            xmlCleanupParser();
            pthread_mutex_unlock(&running_modules_lock);
            free(config_vars);
            return FALSE;
         }
         config_vars->current_node = xmlDocGetRootElement(config_vars->doc_tree_ptr);
         break;
      }

      default:
         xmlCleanupParser();
         pthread_mutex_unlock(&running_modules_lock);
         free(config_vars);
         return FALSE;
   }

   if (config_vars->current_node == NULL) {
      VERBOSE(DEBUG,"[WARNING] Reload error - empty document.\n");
      // do not free libnetconf xml structures or parsers data!!!
      if (choice != RELOAD_CALLBACK_ROOT_ELEM) {
         xmlFreeDoc(config_vars->doc_tree_ptr);
         xmlCleanupParser();
      }
      pthread_mutex_unlock(&running_modules_lock);
      free(config_vars);
      return FALSE;
   }

   if (xmlStrcmp(config_vars->current_node->name, BAD_CAST "nemea-supervisor")) {
      VERBOSE(DEBUG,"[WARNING] Reload error - document of the wrong type, root node != nemea-supervisor.\n");
      // do not free libnetconf xml structures or parsers data!!!
      if (choice != RELOAD_CALLBACK_ROOT_ELEM) {
         xmlFreeDoc(config_vars->doc_tree_ptr);
         xmlCleanupParser();
      }
      pthread_mutex_unlock(&running_modules_lock);
      free(config_vars);
      return FALSE;
   }

   if (config_vars->current_node->xmlChildrenNode == NULL) {
      VERBOSE(DEBUG,"[WARNING] Reload error - no child of nemea-supervisor element found.\n");
      // do not free libnetconf xml structures or parsers data!!!
      if (choice != RELOAD_CALLBACK_ROOT_ELEM) {
         xmlFreeDoc(config_vars->doc_tree_ptr);
         xmlCleanupParser();
      }
      pthread_mutex_unlock(&running_modules_lock);
      free(config_vars);
      return FALSE;
   }

   // Print XML configuration to supervisor debug log
   VERBOSE(DEBUG, "\n\n%s [DEBUG] Request to reload this configuration --->\n\n", get_stats_formated_time());
   print_xmlDoc_to_stream(config_vars->current_node->doc, supervisor_debug_log_fd);

   config_vars->current_node = config_vars->current_node->xmlChildrenNode;

   /*****************/
   for (x=0; x<running_modules_array_size; x++) {
      running_modules[x].module_modified_by_reload = FALSE;
      running_modules[x].modules_profile = NULL;
      running_modules[x].module_max_restarts_per_minute = -1;
      running_modules[x].module_is_my_child = TRUE;
      running_modules[x].init_module = FALSE;
      running_modules[x].remove_module = FALSE;
   }

   modules_profile_t * ptr = first_profile_ptr;
   modules_profile_t * p = NULL;
   while (ptr != NULL) {
      p = ptr;
      ptr = ptr->next;
      if (p->profile_name != NULL) {
         free(p->profile_name);
      }
      if (p != NULL) {
         free(p);
      }
   }
   first_profile_ptr = NULL;
   actual_profile_ptr = NULL;

   /*****************/
   VERBOSE(N_STDOUT,"- - -\n[RELOAD] Processing new configuration...\n[RELOAD] Progress of reloading with reports about every event is logged in \"supervisor_debug_log\" file.\n");

   while (config_vars->current_node != NULL) {
      if (!xmlStrcmp(config_vars->current_node->name, BAD_CAST "supervisor")) {
         // Process root's element "supervisor"
         config_vars->module_elem = config_vars->current_node->xmlChildrenNode;
         reload_process_supervisor_element(&config_vars);
      } else if (!xmlStrcmp(config_vars->current_node->name, BAD_CAST "modules")) {
         // Process root's element "modules"
         modules_got_profile = FALSE;
         config_vars->module_elem = config_vars->current_node->xmlChildrenNode;
         config_vars->module_atr_elem = NULL, config_vars->ifc_elem = NULL, config_vars->ifc_atr_elem = NULL;
         ifc_cnt = 0;

         /* if return value equals -1, modules element doesn't have one valid name and enabled element -> it's children (module elements) won't have profile
         *  return value 0 means success -> modules children will have a profile
         */
         if (reload_find_and_check_modules_profile_basic_elements(&config_vars) == 0) {
            VERBOSE(N_STDOUT, "[WARNING] Found valid modules profile with name \"%s\" set to %s.\n", actual_profile_ptr->profile_name, (actual_profile_ptr->profile_enabled == TRUE ? "enabled" : "disabled"));
            modules_got_profile = TRUE;
         } else {
            VERBOSE(N_STDOUT, "[WARNING] Did not find valid modules profile.\n");
            modules_got_profile = FALSE;
         }

         config_vars->module_elem = config_vars->current_node->xmlChildrenNode;

         while (config_vars->module_elem != NULL) {
            if (!xmlStrcmp(config_vars->module_elem->name, BAD_CAST "module")) {
               // Process modules element "module"
               config_vars->current_module_idx = -1;

               // Check and reallocate (if needed) running_modules memory
               reload_check_running_modules_allocated_memory();

               config_vars->module_atr_elem = config_vars->module_elem->xmlChildrenNode;

               /* if return value equals 1, there is no more valid module elements -> break the module parsing loop
               *  return value 0 is success -> parse the module attributes
               */
               if (reload_find_and_check_module_basic_elements(&config_vars) == -1) {
                  VERBOSE(N_STDOUT, "[WARNING] Reloading error - last module is invalid, breaking the loop.\n");
                  break;
               } else {
                  if (config_vars->current_module_idx != loaded_modules_cnt) {
                     already_loaded_modules_found[config_vars->current_module_idx] = TRUE;
                  }
               }

               // Get module's PID from "module" element if it exists
               if (choice == RELOAD_INIT_LOAD_CONFIG) {
                  key = xmlGetProp(config_vars->module_elem, BAD_CAST "module_pid");
                  if (key != NULL) {
                     running_modules[config_vars->current_module_idx].module_pid = atoi((char *) key);
                     xmlFree(key);
                     key = NULL;
                  }
               }

               config_vars->module_atr_elem = config_vars->module_elem->xmlChildrenNode;
               while (config_vars->module_atr_elem != NULL) {
                  if ((!xmlStrcmp(config_vars->module_atr_elem->name,BAD_CAST "enabled"))) {
                     // Process module's "enabled" attribute
                     reload_resolve_module_enabled(&config_vars, modules_got_profile);
                  } else if (!xmlStrcmp(config_vars->module_atr_elem->name, BAD_CAST "module-restarts")) {
                     // Process module's "module-restarts" attribute
                     key = xmlNodeListGetString(config_vars->doc_tree_ptr, config_vars->module_atr_elem->xmlChildrenNode, 1);
                     if (key != NULL) {
                        x = 0;
                        if ((sscanf((char *) key,"%d",&number) == 1) && (number >= 0)) {
                           x = (unsigned int) number;
                           running_modules[config_vars->current_module_idx].module_max_restarts_per_minute = x;
                        }
                        xmlFree(key);
                        key = NULL;
                     }
                  } else if ((!xmlStrcmp(config_vars->module_atr_elem->name,BAD_CAST "params"))) {
                     // Process module's "parameters" attribute
                     reload_process_module_atribute(&config_vars, &running_modules[config_vars->current_module_idx].module_params);
                  } else if ((!xmlStrcmp(config_vars->module_atr_elem->name,BAD_CAST "name"))) {
                     // Process module's "name" attribute
                     if (config_vars->module_modifying == FALSE) {
                        key = xmlNodeListGetString(config_vars->doc_tree_ptr, config_vars->module_atr_elem->xmlChildrenNode, 1);
                        if (key != NULL) {
                           str_len = strlen((char *) key);
                           running_modules[config_vars->current_module_idx].module_name = (char *) xmlStrdup(key);
                           xmlFree(key);
                           key = NULL;
                        }
                     }
                  } else if ((!xmlStrcmp(config_vars->module_atr_elem->name,BAD_CAST "path"))) {
                     // Process module's "path" attribute
                     reload_process_module_atribute(&config_vars, &running_modules[config_vars->current_module_idx].module_path);
                  } else if ((!xmlStrcmp(config_vars->module_atr_elem->name,BAD_CAST "trapinterfaces"))) {
                     // Process module's "trapinterfaces" element
                     ifc_cnt=0;
                     config_vars->ifc_elem = config_vars->module_atr_elem->xmlChildrenNode;

                     // If the parsed module has been already in configuration, check it's interfaces count -> it original count equals actual, it's ok, otherwise interfaces will be updated.
                     if (config_vars->module_modifying) {
                        reload_check_modules_interfaces_count(&config_vars);
                     }

                     while (config_vars->ifc_elem != NULL) {
                        if (!xmlStrcmp(config_vars->ifc_elem->name,BAD_CAST "interface")) {
                           config_vars->ifc_atr_elem = config_vars->ifc_elem->xmlChildrenNode;

                           // Check and reallocate (if needed) module's interfaces array
                           reload_check_module_allocated_interfaces(&config_vars, ifc_cnt);

                           while (config_vars->ifc_atr_elem != NULL) {
                              if ((!xmlStrcmp(config_vars->ifc_atr_elem->name,BAD_CAST "note"))) {
                                 // Process module's interface "note" attribute
                                 if (reload_process_module_interface_atribute(&config_vars, &running_modules[config_vars->current_module_idx].module_ifces[ifc_cnt].ifc_note) == -1) {
                                    ifc_cnt = -1;
                                    break;
                                 }
                              } else if ((!xmlStrcmp(config_vars->ifc_atr_elem->name,BAD_CAST "type"))) {
                                 // Process module's interface "type" attribute
                                 if (reload_process_module_interface_atribute(&config_vars, &running_modules[config_vars->current_module_idx].module_ifces[ifc_cnt].ifc_type) == -1) {
                                    ifc_cnt = -1;
                                    break;
                                 }
                              } else if ((!xmlStrcmp(config_vars->ifc_atr_elem->name,BAD_CAST "direction"))) {
                                 // Process module's interface "direction" attribute
                                 if (reload_process_module_interface_atribute(&config_vars, &running_modules[config_vars->current_module_idx].module_ifces[ifc_cnt].ifc_direction) == -1) {
                                    ifc_cnt = -1;
                                    break;
                                 }
                              } else if ((!xmlStrcmp(config_vars->ifc_atr_elem->name,BAD_CAST "params"))) {
                                 // Process module's interface "parameters" attribute
                                 if (reload_process_module_interface_atribute(&config_vars, &running_modules[config_vars->current_module_idx].module_ifces[ifc_cnt].ifc_params) == -1) {
                                    ifc_cnt = -1;
                                    break;
                                 }
                              }
                              config_vars->ifc_atr_elem=config_vars->ifc_atr_elem->next;
                           }

                           ifc_cnt++;
                           if (config_vars->module_modifying == FALSE) {
                              running_modules[config_vars->current_module_idx].module_ifces_cnt++;
                           }
                        }
                        config_vars->ifc_elem = config_vars->ifc_elem->next;
                     }
                  }

                  config_vars->module_atr_elem = config_vars->module_atr_elem->next;
               }

               // If the parsed module is new or it's interfaces were updated, count it's input and output interfaces
               if (config_vars->module_modifying == FALSE) {
                  reload_count_module_interfaces(&config_vars);
               }

               // If the parsed module is new (it was inserted to next empty position), increment counters
               if (config_vars->current_module_idx == loaded_modules_cnt) {
                  loaded_modules_cnt++;
                  config_vars->inserted_modules++;
               }

               // If actual modules element has a valid profile, save it's name in parsed module structure
               if (modules_got_profile == TRUE) {
                  running_modules[config_vars->current_module_idx].modules_profile = actual_profile_ptr->profile_name;
               }
            }
            config_vars->module_elem = config_vars->module_elem->next;
         }
      }
      config_vars->current_node = config_vars->current_node->next;
   }

   // do not free libnetconf xml structures or parsers data!!!
   if (choice != RELOAD_CALLBACK_ROOT_ELEM) {
      xmlFreeDoc(config_vars->doc_tree_ptr);
      xmlCleanupParser();
   }

   // Stop and remove missing modules from loaded configuration (modules deleted by user)
   for (x=0; x<original_loaded_modules_cnt; x++) {
      if (already_loaded_modules_found[x] == FALSE) {
         VERBOSE(N_STDOUT, "[WARNING] %s was not found in new configuration, it will be stopped and removed.\n", running_modules[x].module_name);
         running_modules[x].module_enabled = FALSE;
         running_modules[x].remove_module = TRUE;
         config_vars->removed_modules++;
      }
   }

   for (x=0; x<loaded_modules_cnt; x++) {
      running_modules[x].module_served_by_service_thread = FALSE;
      // Count modified modules
      if (running_modules[x].module_modified_by_reload == TRUE) {
         config_vars->modified_modules++;
         // If they were or are running, restart them with new configuration and initialize their variables
         if (running_modules[x].module_running == TRUE) {
            running_modules[x].module_running = FALSE;
            if (running_modules[x].module_enabled == TRUE) {
               VERBOSE(N_STDOUT, "[WARNING] %s was modified by reload and it has been running -> it will be restarted with new configuration.\n", running_modules[x].module_name);
               running_modules[x].module_enabled = FALSE;
               running_modules[x].init_module = TRUE;
            }
         }
      }
   }

   // If module's PID > 0, initialize it's variables, because it's already running, so there won't be re_start function call
   if (choice == RELOAD_INIT_LOAD_CONFIG) {
      for (x=0; x<loaded_modules_cnt; x++) {
         if (running_modules[x].module_pid > 0) {
            init_module_variables(x);
         }
      }
   }

   // Print reload statistics
   VERBOSE(N_STDOUT,"Inserted modules:\t%d\n", config_vars->inserted_modules);
   VERBOSE(N_STDOUT,"Removed modules:\t%d\n", config_vars->removed_modules);
   VERBOSE(N_STDOUT,"Modified modules:\t%d\n", config_vars->modified_modules);
   VERBOSE(N_STDOUT,"Unmodified modules:\t%d\n", original_loaded_modules_cnt - config_vars->modified_modules - config_vars->removed_modules);

   pthread_mutex_unlock(&running_modules_lock);
   free(config_vars);
   return TRUE;
}

void generate_backup_config_file()
{
   modules_profile_t * ptr = first_profile_ptr;
   unsigned int x, y, backuped_modules = 0;
   char buffer[20];
   const char * templ = "<?xml version=\"1.0\"?><nemea-supervisor xmlns=\"urn:cesnet:tmc:nemea:1.0\"></nemea-supervisor>";
   xmlDocPtr document_ptr = NULL;
   xmlNodePtr root_elem = NULL, modules = NULL, module = NULL, trapinterfaces = NULL, interface = NULL;

   document_ptr = xmlParseMemory(templ, strlen(templ));
   if (document_ptr == NULL) {
      return;
   }
   root_elem = xmlDocGetRootElement(document_ptr);
   xmlNewProp (root_elem, BAD_CAST "lock", NULL);
   if (daemon_flag) {
      xmlNewProp (root_elem, BAD_CAST "daemon", BAD_CAST "true");
      xmlNewProp (root_elem, BAD_CAST "socket_path", BAD_CAST socket_path);
   } else {
      xmlNewProp (root_elem, BAD_CAST "daemon", BAD_CAST "false");
      xmlNewProp (root_elem, BAD_CAST "socket_path", BAD_CAST NULL);
   }

   modules = xmlNewChild(root_elem, NULL, BAD_CAST "supervisor", BAD_CAST NULL);
   if (verbose_flag) {
      xmlNewChild(modules, NULL, BAD_CAST "verbose", BAD_CAST "true");
   } else {
      xmlNewChild(modules, NULL, BAD_CAST "verbose", BAD_CAST "false");
   }
   memset(buffer,0,20);
   sprintf(buffer, "%d", max_restarts_per_minute_config);
   xmlNewChild(modules, NULL, BAD_CAST "module-restarts", BAD_CAST buffer);
   xmlNewChild(modules, NULL, BAD_CAST "logs-directory", BAD_CAST logs_path);

   if (xmlAddChild(root_elem, modules) == NULL) {
      xmlFree(modules);
   }

   // backup modules with profile name
   while (ptr != NULL) {
      if (ptr->profile_name != NULL) {
         modules = xmlNewChild(root_elem, NULL, BAD_CAST "modules", NULL);
         xmlNewChild(modules, NULL, BAD_CAST "name", BAD_CAST ptr->profile_name);
         if (ptr->profile_enabled) {
            xmlNewChild(modules, NULL, BAD_CAST "enabled", BAD_CAST "true");
         } else {
            xmlNewChild(modules, NULL, BAD_CAST "enabled", BAD_CAST "false");
         }
         for (x=0; x<loaded_modules_cnt; x++) {
            if (running_modules[x].modules_profile != NULL) {
               if (strcmp(running_modules[x].modules_profile, ptr->profile_name) == 0) {
                  module = xmlNewChild(modules, NULL, BAD_CAST "module", NULL);

                  memset(buffer,0,20);
                  sprintf(buffer, "%d", running_modules[x].module_pid);
                  xmlNewProp (module, BAD_CAST "module_pid", BAD_CAST buffer);

                  xmlNewChild(module, NULL, BAD_CAST "name", BAD_CAST running_modules[x].module_name);
                  xmlNewChild(module, NULL, BAD_CAST "path", BAD_CAST running_modules[x].module_path);
                  xmlNewChild(module, NULL, BAD_CAST "params", BAD_CAST running_modules[x].module_params);
                  if (running_modules[x].module_enabled) {
                     xmlNewChild(module, NULL, BAD_CAST "enabled", BAD_CAST "true");
                  } else {
                     xmlNewChild(module, NULL, BAD_CAST "enabled", BAD_CAST "false");
                  }
                  trapinterfaces = xmlNewChild(module, NULL, BAD_CAST "trapinterfaces", NULL);

                  for (y=0; y<running_modules[x].module_ifces_cnt; y++) {
                     interface = xmlNewChild(trapinterfaces, NULL, BAD_CAST "interface", NULL);
                     xmlNewChild(interface, NULL, BAD_CAST "note", BAD_CAST running_modules[x].module_ifces[y].ifc_note);
                     xmlNewChild(interface, NULL, BAD_CAST "params", BAD_CAST running_modules[x].module_ifces[y].ifc_params);
                     xmlNewChild(interface, NULL, BAD_CAST "direction", BAD_CAST running_modules[x].module_ifces[y].ifc_direction);
                     xmlNewChild(interface, NULL, BAD_CAST "type", BAD_CAST running_modules[x].module_ifces[y].ifc_type);

                     if (xmlAddChild(trapinterfaces, interface) == NULL) {
                        xmlFree(interface);
                     }
                  }


                  if (xmlAddChild(modules, module) == NULL) {
                     xmlFree(module);
                  }
                  backuped_modules++;
               }
            }
         }

         if (xmlAddChild(root_elem, modules) == NULL) {
            xmlFree(modules);
         }
      }
      ptr = ptr->next;
   }

   //backup modules without profile name
   if (backuped_modules < loaded_modules_cnt) {
      modules = xmlNewChild(root_elem, NULL, BAD_CAST "modules", NULL);
      for (x=0; x<loaded_modules_cnt; x++) {
         if (running_modules[x].modules_profile == NULL) {
            module = xmlNewChild(modules, NULL, BAD_CAST "module", NULL);

            memset(buffer,0,20);
            sprintf(buffer, "%d", running_modules[x].module_pid);
            xmlNewProp (module, "module_pid", buffer);

            xmlNewChild(module, NULL, BAD_CAST "name", running_modules[x].module_name);
            xmlNewChild(module, NULL, BAD_CAST "path", running_modules[x].module_path);
            xmlNewChild(module, NULL, BAD_CAST "params", running_modules[x].module_params);
            if (running_modules[x].module_enabled) {
               xmlNewChild(module, NULL, BAD_CAST "enabled", "true");
            } else {
               xmlNewChild(module, NULL, BAD_CAST "enabled", "false");
            }
            trapinterfaces = xmlNewChild(module, NULL, BAD_CAST "trapinterfaces", NULL);

            for (y=0; y<running_modules[x].module_ifces_cnt; y++) {
               interface = xmlNewChild(trapinterfaces, NULL, BAD_CAST "interface", NULL);
               xmlNewChild(interface, NULL, BAD_CAST "note", running_modules[x].module_ifces[y].ifc_note);
               xmlNewChild(interface, NULL, BAD_CAST "params", running_modules[x].module_ifces[y].ifc_params);
               xmlNewChild(interface, NULL, BAD_CAST "direction", running_modules[x].module_ifces[y].ifc_direction);
               xmlNewChild(interface, NULL, BAD_CAST "type", running_modules[x].module_ifces[y].ifc_type);

               if (xmlAddChild(trapinterfaces, interface) == NULL) {
                  xmlFree(interface);
               }
            }

            if (xmlAddChild(modules, module) == NULL) {
               xmlFree(module);
            }
         }
      }

      if (xmlAddChild(root_elem, modules) == NULL) {
         xmlFree(modules);
      }
   }

   char file_name[100];
   memset(file_name,0,100);
   if (socket_path == NULL) {
      sprintf(file_name, "%s%s.xml", logs_path, DEFAULT_BACKUP_FILE_PREFIX); // TODO backup file path, what if logs dir path is changed after restart.. wont find backup file
   } else {
      char socket_path_without_s[100];
      memset(socket_path_without_s,0,100);
      int socket_path_without_s_length = 0;
      for (x=0; x<strlen(socket_path); x++) {
         if (socket_path[x] != '/') {
            socket_path_without_s[socket_path_without_s_length] = socket_path[x];
            socket_path_without_s_length++;
         }
      }
      sprintf(file_name, "%s%s_%s.xml", logs_path, DEFAULT_BACKUP_FILE_PREFIX, socket_path_without_s);
   }

   FILE * file_fd = fopen(file_name,"w");

   if (file_fd != NULL) {
      if (xmlDocFormatDump(file_fd, document_ptr, 1) == -1) {
         VERBOSE(N_STDOUT, "%s [ERROR] Could not save backup file!\n", get_stats_formated_time());
      } else {
         VERBOSE(N_STDOUT, "%s [WARNING] Phew, backup file saved !!\n", get_stats_formated_time());
      }
      fclose(file_fd);
   } else {
      VERBOSE(N_STDOUT, "%s [ERROR] Could not open backup file!\n", get_stats_formated_time());
   }

   xmlFreeDoc(document_ptr);
   xmlCleanupParser();
}

#ifdef nemea_plugin
int nc_supervisor_initialization()
{
   unsigned int y = 0;

   init_time_info = get_sys_time();
   service_stop_all_modules = FALSE;
   logs_path = NULL;
   config_file = NULL;
   socket_path = DEFAULT_DAEMON_UNIXSOCKET_PATH_FILENAME;
   verbose_flag = FALSE;
   file_flag = FALSE;
   daemon_flag = FALSE;
   netconf_flag = TRUE;
   graph_first_node = NULL;
   graph_last_node = NULL;

   input_fd = stdin;
   output_fd = stdout;

   create_output_dir();
   create_output_files_strings();

   VERBOSE(N_STDOUT,"[INIT LOADING CONFIGURATION]\n");
   loaded_modules_cnt = 0;
   running_modules_array_size = RUNNING_MODULES_ARRAY_START_SIZE;
   running_modules = (running_module_t *) calloc (running_modules_array_size,sizeof(running_module_t));
   for (y=0;y<running_modules_array_size;y++) {
      running_modules[y].module_ifces = (interface_t *) calloc(IFCES_ARRAY_START_SIZE, sizeof(interface_t));
      running_modules[y].module_running = FALSE;
      running_modules[y].module_ifces_array_size = IFCES_ARRAY_START_SIZE;
      running_modules[y].module_ifces_cnt = 0;
   }

   pthread_mutex_init(&running_modules_lock,NULL);
   service_thread_continue = TRUE;

   VERBOSE(N_STDOUT,"[SERVICE] Starting service thread.\n");
   start_service_thread();

   return 0;
}

xmlDocPtr nc_get_state_data()
{
   modules_profile_t * ptr = first_profile_ptr;
   unsigned int x = 0, y = 0, in_ifc_cnt = 0, out_ifc_cnt = 0, modules_with_profile = 0;
   char buffer[DEFAULT_SIZE_OF_BUFFER];
   const char * template = "<?xml version=\"1.0\"?><nemea-supervisor xmlns=\"urn:cesnet:tmc:nemea:1.0\"></nemea-supervisor>";
   xmlDocPtr doc_tree_ptr = NULL;
   xmlNodePtr root_elem = NULL, modules_elem = NULL, module_elem = NULL, trapinterfaces_elem = NULL, interface_elem = NULL;

   if (loaded_modules_cnt > 0) {
      doc_tree_ptr = xmlParseMemory(template, strlen(template));
      if (doc_tree_ptr == NULL) {
         return NULL;
      }
      root_elem = xmlDocGetRootElement(doc_tree_ptr);
      if (root_elem == NULL) {
         return NULL;
      }

      // get state data about modules with a profile
      while (ptr != NULL) {
         if (ptr->profile_name != NULL) {
            modules_elem = xmlNewChild(root_elem, NULL, BAD_CAST "modules", NULL);
            xmlNewChild(modules_elem, NULL, BAD_CAST "name", BAD_CAST ptr->profile_name);
            if (ptr->profile_enabled) {
               xmlNewChild(modules_elem, NULL, BAD_CAST "enabled", BAD_CAST "true");
            } else {
               xmlNewChild(modules_elem, NULL, BAD_CAST "enabled", BAD_CAST "false");
            }

            for (x = 0; x < loaded_modules_cnt; x++) {
               if (running_modules[x].modules_profile == NULL) {
                  continue;
               }
               if (strcmp(running_modules[x].modules_profile, ptr->profile_name) != 0) {
                  continue;
               }
               memset(buffer,0,DEFAULT_SIZE_OF_BUFFER);
               module_elem = xmlNewChild(modules_elem, NULL, BAD_CAST "module", NULL);
               xmlNewChild(module_elem, NULL, BAD_CAST "name", BAD_CAST running_modules[x].module_name);

               if (running_modules[x].module_status == TRUE) {
                  xmlNewChild(module_elem, NULL, BAD_CAST "running", BAD_CAST "true");
               } else {
                  xmlNewChild(module_elem, NULL, BAD_CAST "running", BAD_CAST "false");
               }

               if (running_modules[x].module_restart_cnt < 0) {
                  snprintf(buffer, DEFAULT_SIZE_OF_BUFFER, "%d",0);
                  xmlNewChild(module_elem, NULL, BAD_CAST "restart-counter", BAD_CAST buffer);
               } else {
                  snprintf(buffer, DEFAULT_SIZE_OF_BUFFER, "%d",running_modules[x].module_restart_cnt);
                  xmlNewChild(module_elem, NULL, BAD_CAST "restart-counter", BAD_CAST buffer);
               }

               if (running_modules[x].module_has_service_ifc && running_modules[x].module_status) {
                  in_ifc_cnt = 0;
                  out_ifc_cnt = 0;
                  trapinterfaces_elem = xmlNewChild(module_elem, NULL, BAD_CAST "trapinterfaces", NULL);
                  for (y=0; y<running_modules[x].module_ifces_cnt; y++) {
                     if (running_modules[x].module_ifces[y].ifc_direction != NULL) {
                        interface_elem = xmlNewChild(trapinterfaces_elem, NULL, BAD_CAST "interface", NULL);
                        xmlNewChild(interface_elem, NULL, BAD_CAST "type", BAD_CAST running_modules[x].module_ifces[y].ifc_type);
                        xmlNewChild(interface_elem, NULL, BAD_CAST "direction", BAD_CAST running_modules[x].module_ifces[y].ifc_direction);
                        xmlNewChild(interface_elem, NULL, BAD_CAST "params", BAD_CAST running_modules[x].module_ifces[y].ifc_params);
                        if (strcmp(running_modules[x].module_ifces[y].ifc_direction, "IN") == 0) {
                           memset(buffer,0,DEFAULT_SIZE_OF_BUFFER);
                           snprintf(buffer, DEFAULT_SIZE_OF_BUFFER, "%"PRIu64,running_modules[x].module_counters_array[in_ifc_cnt]);
                           xmlNewChild(interface_elem, NULL, BAD_CAST "recv-msg-cnt", BAD_CAST buffer);
                           in_ifc_cnt++;
                        } else if (xmlStrcmp(BAD_CAST running_modules[x].module_ifces[y].ifc_direction, BAD_CAST "OUT") == 0) {
                           memset(buffer,0,DEFAULT_SIZE_OF_BUFFER);
                           snprintf(buffer, DEFAULT_SIZE_OF_BUFFER, "%"PRIu64,running_modules[x].module_counters_array[running_modules[x].module_num_in_ifc + out_ifc_cnt]);
                           xmlNewChild(interface_elem, NULL, BAD_CAST "sent-msg-cnt", BAD_CAST buffer);
                           memset(buffer,0,DEFAULT_SIZE_OF_BUFFER);
                           snprintf(buffer, DEFAULT_SIZE_OF_BUFFER, "%"PRIu64,running_modules[x].module_counters_array[running_modules[x].module_num_in_ifc + running_modules[x].module_num_out_ifc + out_ifc_cnt]);
                           xmlNewChild(interface_elem, NULL, BAD_CAST "sent-buffer-cnt", BAD_CAST buffer);
                           memset(buffer,0,DEFAULT_SIZE_OF_BUFFER);
                           snprintf(buffer, DEFAULT_SIZE_OF_BUFFER, "%"PRIu64,running_modules[x].module_counters_array[running_modules[x].module_num_in_ifc + 2*running_modules[x].module_num_out_ifc + out_ifc_cnt]);
                           xmlNewChild(interface_elem, NULL, BAD_CAST "autoflush-cnt", BAD_CAST buffer);
                           out_ifc_cnt++;
                        }
                     }
                  }
               }

               /* TODO check and free */
               if (xmlAddChild(modules_elem, module_elem) == NULL) {
                  xmlFree(module_elem);
               }
               modules_with_profile++;
            }

            if (xmlAddChild(root_elem, modules_elem) == NULL) {
               xmlFree(modules_elem);
            }
         }
         ptr = ptr->next;
      }


      //get state data about modules without profile
      if (modules_with_profile < loaded_modules_cnt) {
         modules_elem = xmlNewChild(root_elem, NULL, BAD_CAST "modules", NULL);
         for (x = 0; x < loaded_modules_cnt; x++) {
               if (running_modules[x].modules_profile != NULL) {
                  continue;
               }
               memset(buffer,0,DEFAULT_SIZE_OF_BUFFER);
               module_elem = xmlNewChild(modules_elem, NULL, BAD_CAST "module", NULL);
               xmlNewChild(module_elem, NULL, BAD_CAST "name", BAD_CAST running_modules[x].module_name);

               if (running_modules[x].module_status == TRUE) {
                  xmlNewChild(module_elem, NULL, BAD_CAST "running", BAD_CAST "true");
               } else {
                  xmlNewChild(module_elem, NULL, BAD_CAST "running", BAD_CAST "false");
               }

               if (running_modules[x].module_restart_cnt < 0) {
                  snprintf(buffer, DEFAULT_SIZE_OF_BUFFER, "%d",0);
                  xmlNewChild(module_elem, NULL, BAD_CAST "restart-counter", BAD_CAST buffer);
               } else {
                  snprintf(buffer, DEFAULT_SIZE_OF_BUFFER, "%d",running_modules[x].module_restart_cnt);
                  xmlNewChild(module_elem, NULL, BAD_CAST "restart-counter", BAD_CAST buffer);
               }

               if (running_modules[x].module_has_service_ifc && running_modules[x].module_status) {
                  in_ifc_cnt = 0;
                  out_ifc_cnt = 0;
                  trapinterfaces_elem = xmlNewChild(module_elem, NULL, BAD_CAST "trapinterfaces", NULL);
                  for (y=0; y<running_modules[x].module_ifces_cnt; y++) {
                     if (running_modules[x].module_ifces[y].ifc_direction != NULL) {
                        interface_elem = xmlNewChild(trapinterfaces_elem, NULL, BAD_CAST "interface", NULL);
                        xmlNewChild(interface_elem, NULL, BAD_CAST "type", BAD_CAST running_modules[x].module_ifces[y].ifc_type);
                        xmlNewChild(interface_elem, NULL, BAD_CAST "direction", BAD_CAST running_modules[x].module_ifces[y].ifc_direction);
                        xmlNewChild(interface_elem, NULL, BAD_CAST "params", BAD_CAST running_modules[x].module_ifces[y].ifc_params);
                        if (strcmp(running_modules[x].module_ifces[y].ifc_direction, "IN") == 0) {
                           memset(buffer,0,DEFAULT_SIZE_OF_BUFFER);
                           snprintf(buffer, DEFAULT_SIZE_OF_BUFFER, "%"PRIu64,running_modules[x].module_counters_array[in_ifc_cnt]);
                           xmlNewChild(interface_elem, NULL, BAD_CAST "recv-msg-cnt", BAD_CAST buffer);
                           in_ifc_cnt++;
                        } else if (xmlStrcmp(BAD_CAST running_modules[x].module_ifces[y].ifc_direction, BAD_CAST "OUT") == 0) {
                           memset(buffer,0,DEFAULT_SIZE_OF_BUFFER);
                           snprintf(buffer, DEFAULT_SIZE_OF_BUFFER, "%"PRIu64,running_modules[x].module_counters_array[running_modules[x].module_num_in_ifc + out_ifc_cnt]);
                           xmlNewChild(interface_elem, NULL, BAD_CAST "sent-msg-cnt", BAD_CAST buffer);
                           memset(buffer,0,DEFAULT_SIZE_OF_BUFFER);
                           snprintf(buffer, DEFAULT_SIZE_OF_BUFFER, "%"PRIu64,running_modules[x].module_counters_array[running_modules[x].module_num_in_ifc + running_modules[x].module_num_out_ifc + out_ifc_cnt]);
                           xmlNewChild(interface_elem, NULL, BAD_CAST "sent-buffer-cnt", BAD_CAST buffer);
                           memset(buffer,0,DEFAULT_SIZE_OF_BUFFER);
                           snprintf(buffer, DEFAULT_SIZE_OF_BUFFER, "%"PRIu64,running_modules[x].module_counters_array[running_modules[x].module_num_in_ifc + 2*running_modules[x].module_num_out_ifc + out_ifc_cnt]);
                           xmlNewChild(interface_elem, NULL, BAD_CAST "autoflush-cnt", BAD_CAST buffer);
                           out_ifc_cnt++;
                        }
                     }
                  }
               }

               /* TODO check and free */
               if (xmlAddChild(modules_elem, module_elem) == NULL) {
                  xmlFree(module_elem);
               }
            }

            if (xmlAddChild(root_elem, modules_elem) == NULL) {
               xmlFree(modules_elem);
            }
         }


   }

   return doc_tree_ptr;
}
#endif