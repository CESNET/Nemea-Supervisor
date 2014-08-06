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

#include "supervisor.h"
#include "graph.h"
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
#define MAX_RESTARTS_PER_MINUTE  3  ///< Maximum number of module restarts per minute
#define DEFAULT_SIZE_OF_BUFFER   100

#define DAEMON_STOP_CODE 951753
#define DAEMON_CONFIG_MODE_CODE 789123
#define DAEMON_STATS_MODE_CODE 456987

#define MODULES_UNIXSOCKET_PATH_FILENAME_FORMAT   "/tmp/trap-localhost-%s.sock" ///< Modules output interfaces socket, to which connects service thread.
#define DEFAULT_DAEMON_UNIXSOCKET_PATH_FILENAME  "/tmp/supervisor_daemon.sock"  ///<  Daemon socket.

#define NC_DEFAULT_LOGSDIR_PATH "/tmp/supervisor_logs/"

/*******GLOBAL VARIABLES*******/
running_module_t *   running_modules = NULL;  ///< Information about running modules

int         running_modules_array_size = 0;  ///< Current size of running_modules array.
int         loaded_modules_cnt = 0; ///< Current number of loaded modules.

pthread_mutex_t running_modules_lock; ///< mutex for locking counters
int         service_thread_continue = FALSE; ///< condition variable of main loop of the service_thread

graph_node_t *    graph_first_node = NULL; ///< First node of graph nodes list.
graph_node_t *    graph_last_node = NULL; ///< Last node of graph nodes list.

pthread_t   service_thread_id; ///< Service thread identificator.

// supervisor flags
int      help_flag = FALSE;        // -h 
int      file_flag = FALSE;        // -f "file" arguments
int      verbose_flag = FALSE;     // -v messages and cpu_usage stats
int      daemon_flag = FALSE;      // --daemon
int      netconf_flag = FALSE;
char *   config_file = NULL;
char *   socket_path = NULL;
char *   logs_path = NULL;

char * statistics_file_path;
char * module_event_file_path;
char * supervisor_log_file_path;
char * graph_picture_file_path;
char * graph_code_file_path;

/**************************************/

union tcpip_socket_addr {
   struct addrinfo tcpip_addr; ///< used for TCPIP socket
   struct sockaddr_un unix_addr; ///< used for path of UNIX socket
};

long int get_total_cpu_usage();
void start_service_thread();
int parse_arguments(int *argc, char **argv);
void print_help();
void daemon_mode();
char *get_param_by_delimiter(const char *source, char **dest, const char delimiter);

void create_output_dir()
{
   char * buffer = NULL;
   struct stat st = {0};

   if (netconf_flag) { // netconf mode
      logs_path = (char *) calloc (strlen(NC_DEFAULT_LOGSDIR_PATH)+1, sizeof(char));
      strcpy(logs_path, NC_DEFAULT_LOGSDIR_PATH);
   } else { // interactive mode or daemon mode
      if (logs_path == NULL) {
         if ((buffer = getenv("HOME")) != 0) {
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

      if (stat(logs_path, &st) == -1) {
         mkdir(logs_path, PERM_LOGSDIR);
      }
   }
}

void create_output_files_strings()
{
   statistics_file_path = (char *) calloc (strlen(logs_path)+strlen("supervisor_log_statistics")+1, sizeof(char));
   sprintf(statistics_file_path, "%ssupervisor_log_statistics", logs_path);
   module_event_file_path = (char *) calloc (strlen(logs_path)+strlen("supervisor_log_module_event")+1, sizeof(char));
   sprintf(module_event_file_path, "%ssupervisor_log_module_event", logs_path);
   graph_picture_file_path = (char *) calloc (strlen(logs_path)+strlen("graph_picture.png")+1, sizeof(char));
   sprintf(graph_picture_file_path, "%sgraph_picture.png", logs_path);
   graph_code_file_path = (char *) calloc (strlen(logs_path)+strlen("graph_code")+1, sizeof(char));
   sprintf(graph_code_file_path, "%sgraph_code", logs_path);

   statistics_fd = fopen(statistics_file_path, "w");
   module_event_fd = fopen(module_event_file_path, "w");

   if (netconf_flag || daemon_flag) {
      supervisor_log_file_path = (char *) calloc (strlen(logs_path)+strlen("supervisor_log")+1, sizeof(char));
      sprintf(supervisor_log_file_path, "%ssupervisor_log", logs_path);

      int fd_fd = open (supervisor_log_file_path, O_RDWR | O_CREAT | O_APPEND, PERM_LOGFILE);
      dup2(fd_fd, 1);
      dup2(fd_fd, 2);
      close(fd_fd);
   }
}

/**if choice TRUE -> parse file, else parse buffer*/
int load_configuration(const int choice, const char * buffer)
{
   int needed_tags[2];
   int last_module = FALSE;
   int str_len = 0;
   xmlChar * key = NULL;
   int x = 0, y = 0;
   xmlDocPtr xml_tree = NULL;
   xmlNodePtr current_node = NULL;

   if (choice) {
      xml_tree = xmlParseFile(buffer);
   } else {
      xml_tree = xmlParseMemory(buffer, strlen(buffer));
   }

   if (xml_tree == NULL) {
      fprintf(stderr,"Document not parsed successfully. \n");
      return FALSE;
   }

   current_node = xmlDocGetRootElement(xml_tree);

   if (current_node == NULL) {
      fprintf(stderr,"empty document\n");
      xmlFreeDoc(xml_tree);
      return FALSE;
   }

   if (xmlStrcmp(current_node->name, BAD_CAST "nemea-supervisor")) {
      fprintf(stderr,"document of the wrong type, root node != nemea-supervisor\n");
      xmlFreeDoc(xml_tree);
      return FALSE;
   }

   if (current_node->xmlChildrenNode == NULL) {
      fprintf(stderr,"no child of nemea-supervisor tag found\n");
      xmlFreeDoc(xml_tree);
      return FALSE;
   }

   current_node = current_node->xmlChildrenNode;
   while (1) {
      if (!xmlStrcmp(current_node->name, BAD_CAST "modules")) {
         break;
      }
      current_node = current_node-> next;
      if (current_node == NULL) {
         fprintf(stderr,"Tag \"modules\" wasnt found.\n");
         xmlFreeDoc(xml_tree);
         return FALSE;
      }
   }

   /*****************/

   xmlNodePtr module_ptr = current_node->xmlChildrenNode;
   xmlNodePtr module_atr = NULL, ifc_ptr = NULL, ifc_atr = NULL;
   int ifc_cnt = 0;

   while (module_ptr != NULL) {
      if (!xmlStrcmp(module_ptr->name,BAD_CAST "module"))   {
         memset(needed_tags,0,2*sizeof(int));
         //check allocated memory, if we dont have enough -> realloc
         if (loaded_modules_cnt == running_modules_array_size) {
            VERBOSE(N_STDOUT,"REALLOCING MODULES ARRAY --->\n");
            int origin_size = running_modules_array_size;
            running_modules_array_size += running_modules_array_size/2;
            running_modules = (running_module_t * ) realloc (running_modules, (running_modules_array_size)*sizeof(running_module_t));
            memset(running_modules + origin_size,0,(origin_size/2)*sizeof(running_module_t));
            for (y=loaded_modules_cnt; y<running_modules_array_size; y++) {
               running_modules[y].module_ifces = (interface_t *) calloc (IFCES_ARRAY_START_SIZE, sizeof(interface_t));
               running_modules[y].module_running = FALSE;
               running_modules[y].module_ifces_array_size = IFCES_ARRAY_START_SIZE;
               running_modules[y].module_ifces_cnt = 0;
            }
            graph_node_t * node_ptr = graph_first_node;
            x = 0;
            while (node_ptr != NULL) {
               node_ptr->module_data = (void *) &running_modules[x];
               node_ptr = node_ptr->next_node;
               x++;
            }
         }
         module_atr = module_ptr->xmlChildrenNode;

         while (module_atr != NULL) {
            if ((!xmlStrcmp(module_atr->name,BAD_CAST "name"))) {
               key = xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1);
               if (key == NULL) {
                  module_ptr = module_ptr->next;
                  module_ptr = module_ptr->next;
                  if (module_ptr == NULL) {
                     last_module = TRUE;
                     break;
                  } else {
                     memset(needed_tags,0,2*sizeof(int));
                     module_atr = module_ptr->xmlChildrenNode;
                     continue;
                  }
               } else {
                  needed_tags[0]++;
               }
            } else if ((!xmlStrcmp(module_atr->name,BAD_CAST "path"))) {
               key = xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1);
               if (key == NULL) {
                  module_ptr = module_ptr->next;
                  module_ptr = module_ptr->next;
                  if (module_ptr == NULL) {
                     last_module = TRUE;
                     break;
                  } else {
                     memset(needed_tags,0,2*sizeof(int));
                     module_atr = module_ptr->xmlChildrenNode;
                     continue;
                  }
               } else {
                  needed_tags[1]++;
               }
            }

            if (key != NULL) {
               xmlFree(key);
               key = NULL;
            }
            module_atr = module_atr->next;
         }

         if (last_module) {
            break;
         }
         if (key != NULL) {
            xmlFree(key);
            key = NULL;
         }
         if ((needed_tags[0] != 1) || (needed_tags[1] != 1)) {
            module_ptr = module_ptr->next;
            module_ptr = module_ptr->next;
            continue;
         }

         module_atr = module_ptr->xmlChildrenNode;
         while (module_atr != NULL) {
            if ((!xmlStrcmp(module_atr->name,BAD_CAST "enabled"))) {
               key = xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1);
               if (key == NULL) {
                  running_modules[loaded_modules_cnt].module_enabled = FALSE;
               } else {
                  if (strncmp(key, "true", strlen(key)) == 0) {
                     running_modules[loaded_modules_cnt].module_enabled = TRUE;
                  } else {
                     running_modules[loaded_modules_cnt].module_enabled = FALSE;
                  }
                  if (key != NULL) {
                     xmlFree(key);
                     key = NULL;
                  }
               }
            } else if ((!xmlStrcmp(module_atr->name,BAD_CAST "params"))) {
               key = xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1);
               if (key == NULL) {
                  running_modules[loaded_modules_cnt].module_params = NULL;
               } else {
                  str_len = strlen((char *) key);
                  running_modules[loaded_modules_cnt].module_params = (char *) calloc (str_len+1, sizeof(char));
                  strncpy(running_modules[loaded_modules_cnt].module_params, (char *) key, str_len+1);
                  if (key != NULL) {
                     xmlFree(key);
                     key = NULL;
                  }
               }
            } else if ((!xmlStrcmp(module_atr->name,BAD_CAST "name"))) {
               key = xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1);
               if (key == NULL) {
                  running_modules[loaded_modules_cnt].module_name = NULL;
               } else {
                  str_len = strlen((char *) key);
                  running_modules[loaded_modules_cnt].module_name = (char *) calloc (str_len+1, sizeof(char));
                  strncpy(running_modules[loaded_modules_cnt].module_name, (char *) key, str_len+1);
                  if (key != NULL) {
                     xmlFree(key);
                     key = NULL;
                  }
               }
            } else if ((!xmlStrcmp(module_atr->name,BAD_CAST "path"))) {
               key = xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1);
               if (key == NULL) {
                  running_modules[loaded_modules_cnt].module_path = NULL;
               } else {
                  str_len = strlen((char *) key);
                  running_modules[loaded_modules_cnt].module_path = (char *) calloc (str_len+1, sizeof(char));
                  strncpy(running_modules[loaded_modules_cnt].module_path, (char *) key, str_len+1);
                  if (key != NULL) {
                     xmlFree(key);
                     key = NULL;
                  }
               }
            } else if ((!xmlStrcmp(module_atr->name,BAD_CAST "trapinterfaces"))) {
               ifc_cnt=0;
               ifc_ptr = module_atr->xmlChildrenNode;

               while (ifc_ptr != NULL) {
                  if (!xmlStrcmp(ifc_ptr->name,BAD_CAST "interface")) {
                     ifc_atr = ifc_ptr->xmlChildrenNode;
                     if (ifc_cnt == running_modules[loaded_modules_cnt].module_ifces_array_size) {
                        VERBOSE(N_STDOUT,"REALLOCING MODULE INTERFACES ARRAY --->\n");
                        int origin_size = running_modules[loaded_modules_cnt].module_ifces_array_size;
                        running_modules[loaded_modules_cnt].module_ifces_array_size += running_modules[loaded_modules_cnt].module_ifces_array_size/2;
                        running_modules[loaded_modules_cnt].module_ifces = (interface_t *) realloc (running_modules[loaded_modules_cnt].module_ifces, (running_modules[loaded_modules_cnt].module_ifces_array_size) * sizeof(interface_t));
                        memset(running_modules[loaded_modules_cnt].module_ifces + origin_size,0,(origin_size/2)*sizeof(interface_t));
                     }

                     while (ifc_atr != NULL) {
                        if ((!xmlStrcmp(ifc_atr->name,BAD_CAST "note"))) {
                           key =xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1);
                           if (key == NULL) {
                              running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_note = NULL;
                           } else {
                              str_len = strlen((char *) key);
                              running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_note = (char *) calloc (str_len+1, sizeof(char));
                              strncpy(running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_note , (char *) key, str_len+1);
                              if (key != NULL) {
                                 xmlFree(key);
                                 key = NULL;
                              }
                           }
                        } else if ((!xmlStrcmp(ifc_atr->name,BAD_CAST "type"))) {
                           key =xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1);
                           if (key == NULL) {
                              running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_type = NULL;
                           } else {
                              str_len = strlen((char *) key);
                              running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_type = (char *) calloc (str_len+1, sizeof(char));
                              strncpy(running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_type, (char *) key, str_len+1);
                              if (key != NULL) {
                                 xmlFree(key);
                                 key = NULL;
                              }
                           }
                        } else if ((!xmlStrcmp(ifc_atr->name,BAD_CAST "direction"))) {
                           key =xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1);
                           if (key == NULL) {
                              running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_direction= NULL;
                           } else {
                              str_len = strlen((char *) key);
                              running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_direction = (char *) calloc (str_len+1, sizeof(char));
                              strncpy(running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_direction, (char *) key, str_len+1);
                              if (key != NULL) {
                                 xmlFree(key);
                                 key = NULL;
                              }
                           }
                        } else if ((!xmlStrcmp(ifc_atr->name,BAD_CAST "params"))) {
                           key =xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1);
                           if (key == NULL) {
                              running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_params = NULL;
                           } else {
                              str_len = strlen((char *) key);
                              running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_params = (char *) calloc (str_len+1+10, sizeof(char));
                              strncpy(running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_params, (char *) key, str_len+1);
                              if (key != NULL) {
                                 xmlFree(key);
                                 key = NULL;
                              }
                           }
                        }
                        ifc_atr=ifc_atr->next;
                     }

                     ifc_cnt++;
                     running_modules[loaded_modules_cnt].module_ifces_cnt++;
                  }
                  ifc_ptr = ifc_ptr->next;
               }
            }

            module_atr = module_atr->next;
         }
         for (x=0; x<running_modules[loaded_modules_cnt].module_ifces_cnt; x++) {
            if (running_modules[loaded_modules_cnt].module_ifces[x].ifc_direction != NULL) {
               if (strncmp(running_modules[loaded_modules_cnt].module_ifces[x].ifc_direction, "IN", 2) == 0) {
                  running_modules[loaded_modules_cnt].module_num_in_ifc++;
               } else if (strncmp(running_modules[loaded_modules_cnt].module_ifces[x].ifc_direction, "OUT", 3) == 0) {
                  running_modules[loaded_modules_cnt].module_num_out_ifc++;
               }
            }
         }
         loaded_modules_cnt++;
      }
      module_ptr = module_ptr->next;
   }

   xmlFreeDoc(xml_tree);
   xmlCleanupParser();
   return TRUE;
}


void interactive_show_available_modules ()
{
   int x,y = 0;
   VERBOSE(N_STDOUT,"---PRINTING CONFIGURATION---\n");

   for (x=0; x < loaded_modules_cnt; x++) {
      VERBOSE(N_STDOUT,"%c%d_%s:  PATH:%s  PARAMS:%s\n", (running_modules[x].module_enabled == 0 ? ' ' : '*'), x, running_modules[x].module_name, running_modules[x].module_path, running_modules[x].module_params);
      
      for (y=0; y<running_modules[x].module_ifces_cnt; y++) {
         VERBOSE(N_STDOUT,"\tIFC%d:  %s;%s;%s;%s\n", y, running_modules[x].module_ifces[y].ifc_direction, running_modules[x].module_ifces[y].ifc_type, running_modules[x].module_ifces[y].ifc_params, running_modules[x].module_ifces[y].ifc_note);
      }
   }
}


char **make_module_arguments(const int number_of_module)
{
   int size_of_atr = DEFAULT_SIZE_OF_BUFFER;
   char * atr = (char *) calloc (size_of_atr, sizeof(char));
   int x = 0, y = 0;
   int ptr = 0;
   int str_len = 0;
   int params_counter = 0;
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
         int size_of_buffer = DEFAULT_SIZE_OF_BUFFER;
         char * buffer = (char *) calloc (size_of_buffer, sizeof(char));
         int num_module_params = 0;
         int module_params_length = strlen(running_modules[number_of_module].module_params);
         
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

      fprintf(stdout,"Supervisor - executed command: %s", running_modules[number_of_module].module_path);
      fprintf(stderr,"Supervisor - executed command: %s", running_modules[number_of_module].module_path);
      
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
      int size_of_buffer = DEFAULT_SIZE_OF_BUFFER;
      char * buffer = (char *) calloc (size_of_buffer, sizeof(char));
      int num_module_params = 0;
      int module_params_length = strlen(running_modules[number_of_module].module_params);
      
      for (x=0; x<module_params_length; x++) {
         if (running_modules[number_of_module].module_params[x] == 32) {
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
   
   fprintf(stdout,"Supervisor - executed command: %s   %s   %s", running_modules[number_of_module].module_path, params[1], params[2]);
   fprintf(stderr,"Supervisor - executed command: %s   %s   %s", running_modules[number_of_module].module_path, params[1], params[2]);
   
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


int get_number_from_input()
{
   char buffer[1000];
   memset(buffer,0,1000);
   int ret_val = 0;

   if (fscanf(input_fd, "%s", buffer) == 0) {
      return -1;
   }
   
   if (strlen(buffer) > 2) {
      return -1;
   } else if ((strlen(buffer) == 2) && ((buffer[0] > '9' || buffer[0] < '1') || (buffer[1] > '9' || buffer[1] < '0'))) {
      return -1;
   } else if (!sscanf(buffer,"%d",&ret_val)) {
      return -1;
   }
   
   return ret_val;
}


int interactive_get_option()
{
   VERBOSE(N_STDOUT,"--------OPTIONS--------\n");
   VERBOSE(N_STDOUT,"1. START ALL MODULES\n");
   VERBOSE(N_STDOUT,"2. STOP ALL MODULES\n");
   VERBOSE(N_STDOUT,"3. START MODULE\n");
   VERBOSE(N_STDOUT,"4. STOP MODULE\n");
   VERBOSE(N_STDOUT,"5. STARTED MODULES STATUS\n");
   VERBOSE(N_STDOUT,"6. AVAILABLE MODULES\n");
   VERBOSE(N_STDOUT,"7. SHOW GRAPH\n");
   VERBOSE(N_STDOUT,"8. RELOAD CONFIGURATION\n");
   VERBOSE(N_STDOUT,"9. STOP SUPERVISOR\n");

   return get_number_from_input();
}


void re_start_module(const int module_number)
{
   if (running_modules[module_number].module_running == FALSE) {
      VERBOSE(MODULE_EVENT,"Starting module %d_%s.\n", module_number, running_modules[module_number].module_name);
      running_modules[module_number].module_counters_array = (int *) calloc (3*running_modules[module_number].module_num_out_ifc + running_modules[module_number].module_num_in_ifc,sizeof(int));
      running_modules[module_number].module_running = TRUE;
   } else {
      VERBOSE(MODULE_EVENT,"Restarting module %d_%s\n", module_number, running_modules[module_number].module_name);
   }

   char log_path_stdout[200];
   char log_path_stderr[200];
   memset(log_path_stderr,0,200);
   memset(log_path_stdout,0,200);

   sprintf(log_path_stdout,"%s%d_%s_stdout",logs_path, module_number, running_modules[module_number].module_name);
   sprintf(log_path_stderr,"%s%d_%s_stderr",logs_path, module_number, running_modules[module_number].module_name);

   memset(running_modules[module_number].module_counters_array, 0, (running_modules[module_number].module_num_in_ifc + (3*running_modules[module_number].module_num_out_ifc)) * sizeof(int));
   running_modules[module_number].total_cpu_usage_during_module_startup = get_total_cpu_usage();
   running_modules[module_number].last_period_cpu_usage_kernel_mode = 0;
   running_modules[module_number].last_period_cpu_usage_user_mode = 0;
   running_modules[module_number].last_period_percent_cpu_usage_kernel_mode = 0;
   running_modules[module_number].last_period_percent_cpu_usage_user_mode = 0;
   running_modules[module_number].overall_percent_module_cpu_usage_kernel_mode = 0;
   running_modules[module_number].overall_percent_module_cpu_usage_user_mode = 0;
   running_modules[module_number].module_service_sd = -1;
   running_modules[module_number].sent_sigint = FALSE;

   time_t rawtime;
   struct tm * timeinfo;
   time ( &rawtime );
   timeinfo = localtime ( &rawtime );

   fflush(stdout);
   running_modules[module_number].module_pid = fork();
   if (running_modules[module_number].module_pid == 0) {
      int fd_stdout = open(log_path_stdout, O_RDWR | O_CREAT | O_APPEND, PERM_LOGFILE);
      int fd_stderr = open(log_path_stderr, O_RDWR | O_CREAT | O_APPEND, PERM_LOGFILE);
      dup2(fd_stdout,1); //stdout
      dup2(fd_stderr,2); //stderr
      close(fd_stdout);
      close(fd_stderr);
      fprintf(stdout,"---> %s", asctime (timeinfo));
      fprintf(stderr,"---> %s", asctime (timeinfo));
      if (running_modules[module_number].module_path == NULL) {
         VERBOSE(N_STDOUT,"Module path == NULL.\n");
         running_modules[module_number].module_enabled = FALSE;
      } else {
         char **params = make_module_arguments(running_modules[module_number].module_number);
         fflush(stdout);
         fflush(stderr);
         execvp(running_modules[module_number].module_path, params);
      }
      VERBOSE(N_STDOUT,"Error while executing module binary.\n");
      running_modules[module_number].module_enabled = FALSE;
      exit(1);
   } else if (running_modules[module_number].module_pid == -1) {
      running_modules[module_number].module_status = FALSE;
      running_modules[module_number].module_restart_cnt++;
      VERBOSE(N_STDOUT,"Err Fork()\n");
   } else {
      running_modules[module_number].module_status = TRUE;
      running_modules[module_number].module_restart_cnt++;
      if (running_modules[module_number].module_restart_cnt == 1) {
         running_modules[module_number].module_restart_timer = 0;
      }
   }
}

void service_update_module_status()
{
   int x;
   int status;
   pid_t result;

   for (x=0; x<loaded_modules_cnt; x++) {
      if (running_modules[x].module_pid > 0) {
         result = waitpid(running_modules[x].module_pid , &status, WNOHANG);
         if (result == 0) {
           // Child still alive
         } else if (result == -1) {
           // Error
            if (running_modules[x].module_service_sd != -1) {
               close(running_modules[x].module_service_sd);
               running_modules[x].module_service_sd = -1;
            }
            running_modules[x].module_status = FALSE;
            running_modules[x].module_service_ifc_isconnected = FALSE;
         } else {
           // Child exited
            if (running_modules[x].module_service_sd != -1) {
               close(running_modules[x].module_service_sd);
               running_modules[x].module_service_sd = -1;
            }
            running_modules[x].module_status = FALSE;
            running_modules[x].module_service_ifc_isconnected = FALSE;
         }
      }
   }
}

void sigpipe_handler(int sig)
{
   if (sig == SIGPIPE) {
      VERBOSE(MODULE_EVENT,"SIGPIPE catched..\n");
   }
}

int supervisor_initialization(int *argc, char **argv)
{
   int y = 0, ret_val = 0;

   logs_path = NULL;
   config_file = NULL;
   socket_path = NULL;
   verbose_flag = FALSE;
   help_flag = FALSE;
   file_flag = FALSE;
   daemon_flag = FALSE;
   netconf_flag = FALSE;
   graph_first_node = NULL;
   graph_last_node = NULL;

   input_fd = stdin;
   output_fd = stdout;
   
   ret_val = parse_arguments(argc, argv);
   if (ret_val) {
      if (help_flag) {
         print_help();
         return 1;
      } else if (file_flag == FALSE) {
         fprintf(stderr,"Wrong format of arguments.\n %s [-h] [-v] [--show-cpuusage] -f config_file.xml\n", argv[0]);
         return 1;
      }
   } else {
      return 1;
   }

   int daemon_arg;
   if (daemon_flag) {
      daemon_init(&daemon_arg);
   }

   create_output_dir();
   create_output_files_strings();

   VERBOSE(N_STDOUT,"--- LOADING CONFIGURATION ---\n");
   loaded_modules_cnt = 0;
   running_modules_array_size = RUNNING_MODULES_ARRAY_START_SIZE;
   running_modules = (running_module_t *) calloc (running_modules_array_size,sizeof(running_module_t));
   for (y=0;y<running_modules_array_size;y++) {
      running_modules[y].module_ifces = (interface_t *) calloc(IFCES_ARRAY_START_SIZE, sizeof(interface_t));
      running_modules[y].module_running = FALSE;
      running_modules[y].module_ifces_array_size = IFCES_ARRAY_START_SIZE;
      running_modules[y].module_ifces_cnt = 0;
   }

   //load configuration
   load_configuration(TRUE,config_file);

   pthread_mutex_init(&running_modules_lock,NULL);
   service_thread_continue = TRUE;

   VERBOSE(N_STDOUT,"-- Starting service thread --\n");
   start_service_thread();

   /* function prototype to set handler */
   void sigpipe_handler(int sig);
   struct sigaction sa;

   sa.sa_handler = sigpipe_handler;
   sa.sa_flags = 0;
   sigemptyset(&sa.sa_mask);

   if (sigaction(SIGPIPE,&sa,NULL) == -1) {
      VERBOSE(N_STDOUT,"ERROR sigaction !!\n");
   }

   if (daemon_flag) {
      daemon_mode(&daemon_arg);
      return 2;
   }

   return 0;
}


void interactive_start_configuration()
{
   pthread_mutex_lock(&running_modules_lock);
   VERBOSE(MODULE_EVENT,"Starting configuration...\n");
   int x = 0;
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
   int x = 0;
   pthread_mutex_lock(&running_modules_lock);
   VERBOSE(MODULE_EVENT,"Stopping configuration...\n");
   for (x=0; x<loaded_modules_cnt; x++) {
      if (running_modules[x].module_enabled) {
         running_modules[x].module_enabled = FALSE;
      }
   }
   pthread_mutex_unlock(&running_modules_lock);
}


void interactive_set_module_enabled()
{
   pthread_mutex_lock(&running_modules_lock);
   VERBOSE(N_STDOUT,"Type in module number\n");
   int x = get_number_from_input();

   if (x>=loaded_modules_cnt || x<0) {
      VERBOSE(N_STDOUT,"Wrong input, type in 0 - %d.\n", loaded_modules_cnt-1);
      pthread_mutex_unlock(&running_modules_lock);
      return;
   }

   if (running_modules[x].module_enabled) {
      VERBOSE(N_STDOUT,"Module %d%s is already enabled.\n", x, running_modules[x].module_name);
   } else {
      running_modules[x].module_enabled = TRUE;
      running_modules[x].module_restart_cnt = -1;
   }
   pthread_mutex_unlock(&running_modules_lock);
}

void service_stop_modules_sigint()
{
   int x;
   for (x=0; x<loaded_modules_cnt; x++) {
      if (running_modules[x].module_status && running_modules[x].module_enabled == FALSE && running_modules[x].sent_sigint == FALSE) {
         VERBOSE(MODULE_EVENT, "Stopping module %d_%s... sending SIGINT\n", x, running_modules[x].module_name);
         kill(running_modules[x].module_pid,2);
         running_modules[x].sent_sigint = TRUE;
      }
   }
}

void service_stop_modules_sigkill()
{
   char * dest_port = NULL;
   char buffer[DEFAULT_SIZE_OF_BUFFER];
   int x, y;
   for (x=0; x<loaded_modules_cnt; x++) {
      if (running_modules[x].module_status && running_modules[x].module_enabled == FALSE && running_modules[x].sent_sigint == TRUE) {
         VERBOSE(MODULE_EVENT, "Stopping module %d_%s... sending SIGKILL\n", x, running_modules[x].module_name);
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
                  VERBOSE(MODULE_EVENT, "Deleting socket %s... module %d_%s\n", buffer, x, running_modules[x].module_name);
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
   int x = 0, running_modules_counter = 0;
   
   pthread_mutex_lock(&running_modules_lock);
   for (x=0;x<loaded_modules_cnt;x++) {
      if (running_modules[x].module_status) {
         VERBOSE(N_STDOUT,"%d_%s running (PID: %d)\n",x, running_modules[x].module_name,running_modules[x].module_pid);
         running_modules_counter++;
      }
   }
   if (running_modules_counter == 0) {
      VERBOSE(N_STDOUT,"All modules are stopped.\n");
      pthread_mutex_unlock(&running_modules_lock);
      return;
   }
   VERBOSE(N_STDOUT,"Type in number of module to kill:\n");
   x = get_number_from_input();
   if (x>=loaded_modules_cnt || x<0) {
      VERBOSE(N_STDOUT,"Wrong input, type in 0 - %d.\n", loaded_modules_cnt-1);
      pthread_mutex_unlock(&running_modules_lock);
      return;
   } else {
      running_modules[x].module_enabled = FALSE;
   }
   pthread_mutex_unlock(&running_modules_lock);
}


void service_restart_modules()
{
   int x = 0;
   for (x=0; x<loaded_modules_cnt; x++) {
      if (++running_modules[x].module_restart_timer == 30) {
         running_modules[x].module_restart_timer = 0;
         running_modules[x].module_restart_cnt = 0;
      }
      if (running_modules[x].module_enabled == TRUE && running_modules[x].module_status == FALSE && (running_modules[x].module_restart_cnt == MAX_RESTARTS_PER_MINUTE)) {
         VERBOSE(N_STDOUT,"Module: %d_%s was restarted %d times per minute and it is down again. I set it disabled.\n",x, running_modules[x].module_name, MAX_RESTARTS_PER_MINUTE);
         running_modules[x].module_enabled = FALSE;
      } else if (running_modules[x].module_status == FALSE && running_modules[x].module_enabled == TRUE) {
         re_start_module(x);
      }
   }
}

void interactive_show_running_modules_status()
{
   int x = 0;
   if (loaded_modules_cnt == 0) {
      VERBOSE(N_STDOUT,"No module is loaded.\n");
      return;
   }
   for (x=0; x<loaded_modules_cnt; x++) {
      if (running_modules[x].module_status == TRUE) {
         VERBOSE(N_STDOUT,"%d_%s running (PID: %d)\n",x, running_modules[x].module_name,running_modules[x].module_pid);
      } else {
         VERBOSE(N_STDOUT,"%d_%s stopped (PID: %d)\n",x, running_modules[x].module_name,running_modules[x].module_pid);
      }
   }
}


void supervisor_termination()
{
   int x, y;
   VERBOSE(N_STDOUT,"-- Stopping modules --\n");
   interactive_stop_configuration();
   VERBOSE(N_STDOUT,"-- Aborting service thread --\n");
   sleep(2);
   service_thread_continue = 0;
   sleep(2);
   for (x=0;x<loaded_modules_cnt;x++) {
      if (running_modules[x].module_running && running_modules[x].module_counters_array != NULL) {
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

   destroy_graph(graph_first_node);

   if (config_file != NULL) {
      free(config_file);
      config_file = NULL;
   }
   if (logs_path != NULL) {
      free(logs_path);
      logs_path = NULL;
   }

   if (statistics_file_path != NULL) {
      free(statistics_file_path);
      statistics_file_path = NULL;
   }
   if (module_event_file_path != NULL) {
      free(module_event_file_path);
      module_event_file_path = NULL;
   }
   if (supervisor_log_file_path != NULL) {
      free(supervisor_log_file_path);
      supervisor_log_file_path = NULL;
   }
   if (graph_picture_file_path != NULL) {
      free(graph_picture_file_path);
      graph_picture_file_path = NULL;
   }
   if (graph_code_file_path != NULL) {
      free(graph_code_file_path);
      graph_code_file_path = NULL;
   }

   fclose(statistics_fd);
   fclose(module_event_fd);
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
   int utime = 0, stime = 0, x = 0;
   FILE * proc_stat_fd = NULL;
   char path[20];
   memset(path,0,20);
   long int new_total_cpu_usage = get_total_cpu_usage();
   long int difference_total = new_total_cpu_usage - *last_total_cpu_usage;

   *last_total_cpu_usage = new_total_cpu_usage;

   for (x=0;x<loaded_modules_cnt;x++) {
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

void generate_periodic_picture()
{
   if (graph_first_node == NULL) {
      return;
   }
   generate_graph_code(graph_first_node);
   generate_picture();
}

void interactive_show_graph()
{
   if (graph_first_node == NULL) {
      VERBOSE(N_STDOUT,"No module with service ifc running.\n");
      return;
   }
   show_picture();
}

int service_get_data(int sd, int running_module_number)
{
   int sizeof_recv = (running_modules[running_module_number].module_num_in_ifc + 3*running_modules[running_module_number].module_num_out_ifc) * sizeof(int);
   int total_receved = 0;
   int last_receved = 0;
   char * data_pointer = (char *) running_modules[running_module_number].module_counters_array;

   while (total_receved < sizeof_recv) {
      last_receved = recv(sd, data_pointer + total_receved, sizeof_recv - total_receved, 0);
      if (last_receved == 0) {
         VERBOSE(STATISTICS,"! Modules service thread closed its socket, im done !\n");
         return 0;
      } else if (last_receved == -1) {
         VERBOSE(STATISTICS,"! Error while recving from module %d_%s !\n", running_module_number, running_modules[running_module_number].module_name);
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

   if (running_modules[module].module_ifces[num_ifc].ifc_params == NULL) {
      running_modules[module].module_service_ifc_isconnected = FALSE;
      return;
   }
   get_param_by_delimiter(running_modules[module].module_ifces[num_ifc].ifc_params, &dest_port, ',');
   VERBOSE(MODULE_EVENT,"-- Connecting to module %d_%s on port %s\n",module,running_modules[module].module_name, dest_port);

   memset(&addr, 0, sizeof(addr));

   addr.unix_addr.sun_family = AF_UNIX;
   snprintf(addr.unix_addr.sun_path, sizeof(addr.unix_addr.sun_path) - 1, MODULES_UNIXSOCKET_PATH_FILENAME_FORMAT, dest_port);
   sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (sockfd == -1) {
      VERBOSE(MODULE_EVENT,"Error while opening socket.\n");
      running_modules[module].module_service_ifc_isconnected = FALSE;
      if (dest_port != NULL) {
         free(dest_port);
         dest_port = NULL;
      }
      return;
   }
   if (connect(sockfd, (struct sockaddr *) &addr.unix_addr, sizeof(addr.unix_addr)) == -1) {
      VERBOSE(MODULE_EVENT,"Error while connecting to module %d_%s on port %s\n",module,running_modules[module].module_name, dest_port);
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
   if (dest_port != NULL) {
      free(dest_port);
      dest_port = NULL;
   }
}

void print_statistics_and_cpu_usage(struct tm * timeinfo)
{
   int x = 0, y = 0;
   VERBOSE(STATISTICS,"------> %s", asctime(timeinfo));
   for (x=0;x<loaded_modules_cnt;x++) {
      if (running_modules[x].module_status) {
         VERBOSE(STATISTICS,"NAME:  %s; PID: %d; | CPU: O_S %d%%; O_U %d%%; P_S %d%%; P_U %d%%; | ",
            running_modules[x].module_name,
            running_modules[x].module_pid,
            running_modules[x].overall_percent_module_cpu_usage_kernel_mode,
            running_modules[x].overall_percent_module_cpu_usage_user_mode,
            running_modules[x].last_period_percent_cpu_usage_kernel_mode,
            running_modules[x].last_period_percent_cpu_usage_user_mode);
         if (running_modules[x].module_has_service_ifc && running_modules[x].module_service_ifc_isconnected) {
            VERBOSE(STATISTICS,"CNT_RM:  ");
            for (y=0; y<running_modules[x].module_num_in_ifc; y++) {
               VERBOSE(STATISTICS,"%d  ", running_modules[x].module_counters_array[y]);
            }
            VERBOSE(STATISTICS,"CNT_SM:  ");
            for (y=0; y<running_modules[x].module_num_out_ifc; y++) {
               VERBOSE(STATISTICS,"%d  ", running_modules[x].module_counters_array[y + running_modules[x].module_num_in_ifc]);
            }
            VERBOSE(STATISTICS,"CNT_SB:  ");
            for (y=0; y<running_modules[x].module_num_out_ifc; y++) {
               VERBOSE(STATISTICS,"%d  ", running_modules[x].module_counters_array[y + running_modules[x].module_num_in_ifc + running_modules[x].module_num_out_ifc]);
            }
            VERBOSE(STATISTICS,"CNT_AF:  ");
            for (y=0; y<running_modules[x].module_num_out_ifc; y++) {
               VERBOSE(STATISTICS,"%d  ", running_modules[x].module_counters_array[y + running_modules[x].module_num_in_ifc + 2*running_modules[x].module_num_out_ifc]);
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

void *service_thread_routine(void* arg)
{
   long int last_total_cpu_usage = 0;
   int sizeof_intptr = 4;
   int x,y;

   time_t rawtime;
   struct tm * timeinfo;

   if (verbose_flag) {
      print_statistics_legend();
   }

   while (service_thread_continue == TRUE) {
      pthread_mutex_lock(&running_modules_lock);
      time(&rawtime);
      timeinfo = localtime(&rawtime);

      service_update_module_status();
      service_restart_modules();
      service_stop_modules_sigint();

      pthread_mutex_unlock(&running_modules_lock);
      sleep(1);
      pthread_mutex_lock(&running_modules_lock);

      service_update_module_status();
      service_stop_modules_sigkill();

      for (y=0; y<loaded_modules_cnt; y++) {
         if (running_modules[y].module_served_by_service_thread == FALSE) {
            running_modules[y].module_number = y;
            for (x=0; x<running_modules[y].module_ifces_cnt; x++) {
               if (running_modules[y].module_ifces[x].ifc_type != NULL) {
                  if ((strncmp(running_modules[y].module_ifces[x].ifc_type, "SERVICE", 7) == 0)) {
                     running_modules[y].module_has_service_ifc = TRUE;
                     break;
                  }
               }
            }
            graph_node_t * new_node = add_graph_node (graph_first_node, graph_last_node, (void *) &running_modules[y]);

            if (graph_first_node == NULL) {
               graph_first_node = new_node;
               graph_last_node = new_node;
               graph_first_node->next_node = NULL;
            } else {
               graph_last_node->next_node = new_node;
               graph_last_node = new_node;
               graph_last_node->next_node = NULL;
            }
            running_modules[y].module_served_by_service_thread = TRUE;

            if (y==loaded_modules_cnt-1) {
               check_port_duplicates(graph_first_node);
            }
         }
      }
      
      int request[1];
      memset(request,0, sizeof(int));
      request[0] = 1;
      for (x=0;x<loaded_modules_cnt;x++) {
         if (running_modules[x].module_has_service_ifc == TRUE && running_modules[x].module_status == TRUE) {
            if (running_modules[x].module_service_ifc_isconnected == FALSE) {
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
               }
               connect_to_module_service_ifc(x,y);
            }
            if (running_modules[x].module_service_ifc_isconnected == TRUE) {
               if (send(running_modules[x].module_service_sd,(void *) request, sizeof_intptr, 0) == -1) {
                  VERBOSE(STATISTICS,"------> %s", asctime(timeinfo));
                  VERBOSE(STATISTICS,"Error while sending request to module %d_%s.\n",x,running_modules[x].module_name);
                  running_modules[x].module_service_ifc_isconnected = FALSE;
               }
            }
         }
      }
      service_update_module_status();
      for (x=0;x<loaded_modules_cnt;x++) {
         if (running_modules[x].module_has_service_ifc == TRUE && running_modules[x].module_status == TRUE) {
            if (running_modules[x].module_service_ifc_isconnected == TRUE) {
               if (!(service_get_data(running_modules[x].module_service_sd, x))) {
                  continue;
               }
            }
         }
      }
      update_module_cpu_usage(&last_total_cpu_usage);
      update_graph_values(graph_first_node);
      generate_periodic_picture();

      pthread_mutex_unlock(&running_modules_lock);

      if (verbose_flag) {
         print_statistics_and_cpu_usage(timeinfo);
      }

      usleep(500000);
   }

   for (x=0;x<loaded_modules_cnt;x++) {
      if ((running_modules[x].module_has_service_ifc == TRUE) && (running_modules[x].module_service_ifc_isconnected == TRUE)) {
         VERBOSE(MODULE_EVENT,"------> %s", asctime(timeinfo));
         VERBOSE(MODULE_EVENT,"-- disconnecting from module %d_%s\n",x, running_modules[x].module_name);
         if (running_modules[x].module_service_sd != -1) {
            close(running_modules[x].module_service_sd);
         }
      }
   }

   pthread_exit(NULL);
}

char * make_formated_statistics()
{
   int size_of_buffer = 5*DEFAULT_SIZE_OF_BUFFER;
   char * buffer = (char *) calloc (size_of_buffer, sizeof(char));
   int x, y, counter = 0;
   int ptr = 0;

   for (x=0; x<loaded_modules_cnt; x++) {
      if(running_modules[x].module_status) {
         counter = 0;
         for (y=0; y<running_modules[x].module_ifces_cnt; y++) {
            if(running_modules[x].module_ifces[y].ifc_direction != NULL) {
               if(strcmp(running_modules[x].module_ifces[y].ifc_direction, "IN") == 0) {
                  ptr += sprintf(buffer + ptr, "%s,in,%d,%d\n", running_modules[x].module_name, counter, running_modules[x].module_counters_array[counter]);
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
                  ptr += sprintf(buffer + ptr, "%s,out,%d,%d,%d,%d\n", running_modules[x].module_name, counter, running_modules[x].module_counters_array[counter + running_modules[x].module_num_in_ifc],
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
   pthread_create(&service_thread_id, NULL, service_thread_routine, NULL);
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

void print_help() // ?????
{
   VERBOSE(N_STDOUT,"--------------------------\n"
         "NEMEA Supervisor:\n"
         "Expected arguments to run supervisor are: ./supervisor [-d|--daemon] [-f|--config-file=path] [-h|--help] [-v|--verbose] [-C|--show-cpuusage] [-s|--daemon-socket=path]\n"
         "Main thread is waiting for input with number of command to execute.\n"
         "Functions:\n"
         "\t- 1. RUN CONFIGURATION\n"
         "\t- 2. STOP CONFIGURATION\n"
         "\t- 3. SET MODULE ENABLED\n"
         "\t- 4. STOP MODUL\n"
         "\t- 5. STARTED MODULES STATUS\n"
         "\t- 6. AVAILABLE MODULES\n"
         "\t- 7. SHOW GRAPH\n"
         "\t- 8. RUN TEMP CONF\n"
         "\t- 9. QUIT\n"
         "--------------------\n"
         // "0 - setter of VERBOSE level of executed modules\n"
         "1 - command executes every loaded module from configuration\n"
         "2 - command stops whole configuration\n"
         // "3 - command starts single module from configuration, expected input is number of module (command num. 7 can show available modules)\n"
         "3 - setter of \"ENABLE\" flag of selected module - this flag is important for autorestarts\n"
         "4 - command stops single running module\n"
         "5 - command prints status of started modules\n"
         "6 - command prints loaded modules configuration\n"
         "7 - command generates code for dot program and shows graph of running modules using display program\n"
         "8 - command parses external configuration of pasted modules, expected input is same xml code as in config_file, first tag is <modules> and last tag </modules>\n"
         "9 - shut down command\n\n"
         "Example of input for command num. 8:\n"
         "<modules>\n"
            "\t<module>\n"
               "\t\t<params></params>\n"
               "\t\t<name>flowcounter</name>\n"
               "\t\t<path>../modules/flowcounter/flowcounter</path>\n"
               "\t\t<trapinterfaces>\n"
                  "\t\t\t<interface>\n"
                     "\t\t\t\t<note>whatever</note>\n"
                     "\t\t\t\t<type>TCP</type>\n"
                     "\t\t\t\t<direction>IN</direction>\n"
                     "\t\t\t\t<params>localhost,8000</params>\n"
                  "\t\t\t</interface>\n"
                  "\t\t\t<interface>\n"
                     "\t\t\t\t<note>whatever</note>\n"
                     "\t\t\t\t<type>SERVICE</type>\n"
                     "\t\t\t\t<direction></direction>\n"
                     "\t\t\t\t<params>9022,1</params>\n"
                  "\t\t\t</interface>\n"
               "\t\t</trapinterfaces>\n"
            "\t</module>\n"
         "</modules>\n");
}

int daemon_init(int * d_sd)
{
   // create daemon
   pid_t process_id = 0;
   pid_t sid = 0;

   fflush(stdout);
   process_id = fork();
   if (process_id < 0)  {
      VERBOSE(N_STDOUT,"fork failed!\n");
      exit(1);
   } else if (process_id > 0) {
      if (config_file != NULL) {
         free(config_file);
         config_file = NULL;
      }
      if (logs_path != NULL) {
         free(logs_path);
         logs_path = NULL;
      }
      VERBOSE(N_STDOUT,"process_id of child process %d \n", process_id);
      exit(0);
   }

   umask(0);
   sid = setsid();
   if (sid < 0) {
      // Return failure
      exit(1);
   }

   // create socket
   union tcpip_socket_addr addr;
   struct addrinfo *p;
   memset(&addr, 0, sizeof(addr));
   addr.unix_addr.sun_family = AF_UNIX;
   snprintf(addr.unix_addr.sun_path, sizeof(addr.unix_addr.sun_path) - 1, "%s", socket_path);

   /* if socket file exists, it could be hard to create new socket and bind */
   unlink(socket_path); /* error when file does not exist is not a problem */
   *d_sd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (bind(*d_sd, (struct sockaddr *) &addr.unix_addr, sizeof(addr.unix_addr)) != -1) {
      p = (struct addrinfo *) &addr.unix_addr;
      chmod(socket_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
   } else {
      /* error bind() failed */
      p = NULL;
   }
   if (p == NULL) {
      // if we got here, it means we didn't get bound
      VERBOSE(N_STDOUT,"selectserver: failed to bind");
      return 10;
   }
   // listen
   if (listen(*d_sd, 0) == -1) {
      //perror("listen");
      VERBOSE(N_STDOUT,"Listen failed");
      return 10;
   }

   return 0;
}


int daemon_get_client(int * d_sd)
{
   int error, ret_val;
   socklen_t len;
   struct sockaddr_storage remoteaddr; // client address
   socklen_t addrlen;
   int newclient;

   VERBOSE(N_STDOUT, "before accept\n");
   // handle new connection
   addrlen = sizeof remoteaddr;

   while(1) {
      newclient = accept(*d_sd, (struct sockaddr *)&remoteaddr, &addrlen);
      if (newclient == -1) {
         VERBOSE(N_STDOUT,"Accepting new client failed.");
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
   VERBOSE(N_STDOUT,"Client has connected.\n");
   return newclient;
}


void daemon_mode(int * arg)
{
   int got_code = FALSE;
   int x = 0;
   int daemon_sd = *arg;
   int client_sd = -1;
   FILE * client_stream_input = NULL;
   FILE * client_stream_output = NULL;

   int terminated = FALSE;

   int ret_val = 0;
   int request = -1;
   char buffer[1000];
   memset(buffer,0,1000);

   fd_set read_fds;
   struct timeval tv;

   int connected = FALSE;

   while (terminated == FALSE) {
      // get client
      client_sd = daemon_get_client(&daemon_sd);
      if (client_sd == -1) {
         connected = FALSE;
         /* Bind was probably unsuccessful. */
         continue;
      }

      client_stream_input = fdopen(client_sd, "r");
      if (client_stream_input == NULL) {
         VERBOSE(N_STDOUT,"Fdopen error\n");
         close(client_sd);
         continue;
      }

      client_stream_output = fdopen(client_sd, "w");
      if (client_stream_output == NULL) {
         VERBOSE(N_STDOUT,"Fdopen error\n");
         fclose(client_stream_input);
         close(client_sd);
         continue;
      }



      int client_stream_input_fd = fileno(client_stream_input);
      if (client_stream_input_fd < 0) {
         VERBOSE(N_STDOUT,"Fdopen error\n");
         fclose(client_stream_input);
         fclose(client_stream_output);
         close(client_sd);
         continue;
      }

      connected = TRUE;

      input_fd = client_stream_input;
      output_fd = client_stream_output;

      while (connected != 0) {
         request = -1;
         FD_ZERO(&read_fds);
         FD_SET(client_stream_input_fd, &read_fds);

         tv.tv_sec = 2;
         tv.tv_usec = 0;

         ret_val = select(client_stream_input_fd+1, &read_fds, NULL, NULL, &tv);
         if (ret_val == -1) {
            perror("select()");
            fclose(client_stream_input);
            fclose(client_stream_output);
            close(client_sd);
            connected = FALSE;
            got_code = FALSE;
            input_fd = stdin;
            output_fd = stdout;
            break;
         } else if (ret_val != 0) {
            if (FD_ISSET(client_stream_input_fd, &read_fds)) {
               ioctl(client_stream_input_fd, FIONREAD, &x);
               if (x == 0 || x == -1) {
                  input_fd = stdin;
                  output_fd = stdout;
                  VERBOSE(N_STDOUT, "Client has disconnected.\n");
                  connected = FALSE;
                  got_code = FALSE;
                  fclose(client_stream_input);
                  fclose(client_stream_output);
                  close(client_sd);
                  break;
               }
               if (fscanf(input_fd,"%s",buffer) != 1) {
                  connected = FALSE;
               }
               sscanf(buffer,"%d", &request);

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
                  interactive_show_graph();
                  break;
               case 8:
                  reload_configuration(FALSE, NULL);
                  break;
               case DAEMON_STOP_CODE:
                  supervisor_termination();
                  connected = FALSE;
                  terminated = TRUE;
                  break;
               case DAEMON_CONFIG_MODE_CODE:
                  got_code = TRUE;
                  //client config mode
                  break;
               case DAEMON_STATS_MODE_CODE: {
                  //client stats mode
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
                  output_fd = stdout;
                  VERBOSE(N_STDOUT, "Client has disconnected.\n");
                  connected = FALSE;
                  got_code = FALSE;
                  fclose(client_stream_input);
                  fclose(client_stream_output);
                  close(client_sd);
                  break;
               }
               default:
                  VERBOSE(N_STDOUT, "Error input\n");
                  break;
               }
               VERBOSE(N_STDOUT,"--------OPTIONS--------\n");
               VERBOSE(N_STDOUT,"1. START ALL MODULES\n");
               VERBOSE(N_STDOUT,"2. STOP ALL MODULES\n");
               VERBOSE(N_STDOUT,"3. START MODULE\n");
               VERBOSE(N_STDOUT,"4. STOP MODULE\n");
               VERBOSE(N_STDOUT,"5. STARTED MODULES STATUS\n");
               VERBOSE(N_STDOUT,"6. AVAILABLE MODULES\n");
               VERBOSE(N_STDOUT,"7. SHOW GRAPH\n");
               VERBOSE(N_STDOUT,"8. RELOAD CONFIGURATION\n");
               VERBOSE(N_STDOUT,"-- Type \"Cquit\" to exit client --\n");
               VERBOSE(N_STDOUT,"-- Type \"Dstop\" to stop daemon --\n");
            }
         } else {
            if (got_code == FALSE) {
               input_fd = stdin;
               output_fd = stdout;
               VERBOSE(N_STDOUT, "Client isn't responding.\n");
               connected = FALSE;
               fclose(client_stream_input);
               fclose(client_stream_output);
               close(client_sd);
               break;
            }
         }

         printf(".");
         fsync(client_stream_input_fd);
         memset(buffer,0,1000);
         fflush(output_fd);
      }
   }

   fclose(client_stream_input);
   fclose(client_stream_output);
   close(client_sd);
   close(daemon_sd);
   unlink(socket_path);
   return;
}

int get_shorter_string_length(char * first, char * second)
{
   int first_len = strlen(first);
   int second_len = strlen(second);

   if (first_len > second_len) {
      return second_len;
   } else {
      return first_len;
   }
}

int find_loaded_module(char * name)
{
   int x;

   for (x=0; x<loaded_modules_cnt; x++) {
      if (strcmp(running_modules[x].module_name, name) == 0) {
         return x;
      }
   }

   return -1;
}

void free_module_on_index(int index)
{
   int y;
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
   int y;
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

int reload_configuration(const int choice, xmlNodePtr node)
{
   pthread_mutex_lock(&running_modules_lock);

   int modules_got_profile;
   modules_profile_t * first_profile_ptr = NULL;
   modules_profile_t * actual_profile_ptr = NULL;
   int needed_tags[2];
   int inserted_modules = 0, removed_modules = 0, modified_modules = 0;
   int x = 0, y = 0;
   int ret_val = 0;
   int module_index = 0;
   int modifying = 0;
   int original_module_ifc_cnt = 0, new_module_ifc_cnt = 0;
   int original_loaded_modules_cnt = loaded_modules_cnt;
   int already_loaded_modules[loaded_modules_cnt];
   memset(already_loaded_modules, 0, loaded_modules_cnt*sizeof(int));
   int str_len = 0;
   char buffer[100];
   memset(buffer,0,100);
   xmlNodePtr module_ptr = NULL;
   xmlNodePtr module_atr = NULL, ifc_ptr = NULL, ifc_atr = NULL;
   int ifc_cnt = 0;
   int last_module = FALSE;
   
   xmlChar * key = NULL;
   xmlDocPtr xml_tree = NULL;
   xmlNodePtr current_node = NULL;

   for (x=0; x<loaded_modules_cnt; x++) {
      running_modules[x].module_modified_by_reload = FALSE;
   }
   
   if (choice == FALSE) { //new file
      /*Get the name of a new config file */
      VERBOSE(N_STDOUT, "Type in a name of the xml file to be loaded including \".xml\"; (to reload same config file type \"default\" or to cancel reloading type \"cancel\"):\n");
      if (fscanf(input_fd,"%s",buffer) == 0) {
         xml_tree = xmlParseFile(config_file);
      } else if (strcmp(buffer, "cancel") == 0) {
         pthread_mutex_unlock(&running_modules_lock);
         return FALSE;
      } else if (strcmp(buffer, "default") == 0) {
         xml_tree = xmlParseFile(config_file);
      } else {
         xml_tree = xmlParseFile(buffer);
      }

      if (xml_tree == NULL) {
         fprintf(stderr,"Document not parsed successfully. \n");
         pthread_mutex_unlock(&running_modules_lock);
         return FALSE;
      }

      current_node = xmlDocGetRootElement(xml_tree);
   } else { //netconf reload
      current_node = node;
   }

   if (current_node == NULL) {
      fprintf(stderr,"empty document\n");
      xmlFreeDoc(xml_tree);
      pthread_mutex_unlock(&running_modules_lock);
      return FALSE;
   }

   if (xmlStrcmp(current_node->name, BAD_CAST "nemea-supervisor")) {
      fprintf(stderr,"document of the wrong type, root node != nemea-supervisor\n");
      xmlFreeDoc(xml_tree);
      pthread_mutex_unlock(&running_modules_lock);
      return FALSE;
   }

   if (current_node->xmlChildrenNode == NULL) {
      fprintf(stderr,"no child of nemea-supervisor tag found\n");
      xmlFreeDoc(xml_tree);
      pthread_mutex_unlock(&running_modules_lock);
      return FALSE;
   }

   current_node = current_node->xmlChildrenNode;

   // while (1) {
   //    if (!xmlStrcmp(current_node->name, BAD_CAST "modules")) {
   //       break;
   //    }
   //    current_node = current_node-> next;
   //    if (current_node == NULL) {
   //       fprintf(stderr,"Tag \"modules\" wasnt found.\n");
   //       xmlFreeDoc(xml_tree);
   //       pthread_mutex_unlock(&running_modules_lock);
   //       return FALSE;
   //    }
   // }

   /*****************/
   VERBOSE(N_STDOUT,"- - -\nProcessing new configuration...\n");

   while(current_node != NULL) {
      if (!xmlStrcmp(current_node->name, BAD_CAST "modules")) {
         modules_got_profile = FALSE;
         module_ptr = current_node->xmlChildrenNode;
         module_atr = NULL, ifc_ptr = NULL, ifc_atr = NULL;
         ifc_cnt = 0;
         last_module = FALSE;

         while (module_ptr != NULL) {
            if (!xmlStrcmp(module_ptr->name,BAD_CAST "name")) {
               key = xmlNodeListGetString(xml_tree, module_ptr->xmlChildrenNode, 1);
               if (key != NULL) {
                  if (modules_got_profile) {
                     actual_profile_ptr->profile_name = (char *) calloc (strlen(key)+1, sizeof(char));
                     strcpy(actual_profile_ptr->profile_name, key);
                  } else {
                     if (first_profile_ptr == NULL) {
                        first_profile_ptr = (modules_profile_t *) calloc (1, sizeof(modules_profile_t));
                        first_profile_ptr->profile_name = (char *) calloc (strlen(key)+1, sizeof(char));
                        strcpy(first_profile_ptr->profile_name, key);
                        first_profile_ptr->profile_enabled = TRUE;
                        first_profile_ptr->next = NULL;
                        actual_profile_ptr = first_profile_ptr;
                     } else {
                        modules_profile_t * ptr = (modules_profile_t *) calloc (1, sizeof(modules_profile_t));
                        ptr->profile_name = (char *) calloc (strlen(key)+1, sizeof(char));
                        strcpy(ptr->profile_name, key);
                        ptr->profile_enabled = TRUE;
                        ptr->next = NULL;
                        actual_profile_ptr->next = ptr;
                        actual_profile_ptr = ptr;
                     }
                     modules_got_profile = TRUE;
                  }
                  if (key != NULL) {
                     xmlFree(key);
                     key = NULL;
                  }
               }
            } else if (!xmlStrcmp(module_ptr->name,BAD_CAST "enabled")) {
               key = xmlNodeListGetString(xml_tree, module_ptr->xmlChildrenNode, 1);
               if (key != NULL) {
                  if (modules_got_profile) {
                     if (strcmp(key,"true") == 0) {
                        actual_profile_ptr->profile_enabled = TRUE;
                     } else {
                        actual_profile_ptr->profile_enabled = FALSE;
                     }
                  } else {
                     if (first_profile_ptr == NULL) {
                        first_profile_ptr = (modules_profile_t *) calloc (1, sizeof(modules_profile_t));
                        first_profile_ptr->profile_name = NULL;
                        first_profile_ptr->next = NULL;
                        actual_profile_ptr = first_profile_ptr;
                     } else {
                        modules_profile_t * ptr = (modules_profile_t *) calloc (1, sizeof(modules_profile_t));
                        ptr->profile_name = NULL;
                        ptr->next = NULL;
                        actual_profile_ptr->next = ptr;
                        actual_profile_ptr = ptr;
                     }
                     if (strcmp(key,"true") == 0) {
                        actual_profile_ptr->profile_enabled = TRUE;
                     } else {
                        actual_profile_ptr->profile_enabled = FALSE;
                     }
                     modules_got_profile = TRUE;
                  }
                  if (key != NULL) {
                     xmlFree(key);
                     key = NULL;
                  }
               }
            } else if (!xmlStrcmp(module_ptr->name,BAD_CAST "module")) {
               module_index = -1;
               memset(needed_tags,0,2*sizeof(int));
               //check allocated memory, if we dont have enough -> realloc
               if (loaded_modules_cnt == running_modules_array_size) {
                  // VERBOSE(N_STDOUT,"REALLOCING MODULES ARRAY --->\n");
                  int origin_size = running_modules_array_size;
                  running_modules_array_size += running_modules_array_size/2;
                  running_modules = (running_module_t * ) realloc (running_modules, (running_modules_array_size)*sizeof(running_module_t));
                  memset(running_modules + origin_size,0,(origin_size/2)*sizeof(running_module_t));
                  for (y=loaded_modules_cnt; y<running_modules_array_size; y++) {
                     running_modules[y].module_ifces = (interface_t *) calloc (IFCES_ARRAY_START_SIZE, sizeof(interface_t));
                     running_modules[y].module_running = FALSE;
                     running_modules[y].module_ifces_array_size = IFCES_ARRAY_START_SIZE;
                     running_modules[y].module_ifces_cnt = 0;
                  }
                  graph_node_t * node_ptr = graph_first_node;
                  x = 0;
                  while (node_ptr != NULL) {
                     node_ptr->module_data = (void *) &running_modules[x];
                     node_ptr = node_ptr->next_node;
                     x++;
                  }
               }
               module_atr = module_ptr->xmlChildrenNode;
               
               while (module_atr != NULL) {
                  if ((!xmlStrcmp(module_atr->name,BAD_CAST "name"))) {
                     key = xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1);
                     if (key == NULL) {
                        module_ptr = module_ptr->next;
                        module_ptr = module_ptr->next;
                        if (module_ptr == NULL) {
                           last_module = TRUE;
                           break;
                        } else {
                           if ((module_index < original_loaded_modules_cnt) && (module_index != -1)) {
                              already_loaded_modules[module_index] = 0;               
                           }
                           module_index = -1;
                           memset(needed_tags,0,2*sizeof(int));
                           module_atr = module_ptr->xmlChildrenNode;
                           continue;
                        }
                     } else {
                        needed_tags[0]++;
                        ret_val = find_loaded_module(key);
                        if (ret_val == -1) { // new module
                           // VERBOSE(N_STDOUT,"------\nLoading new module..\n");
                           module_index = loaded_modules_cnt;
                           modifying = FALSE;
                        } else { // already loaded module -> check vals
                           // VERBOSE(N_STDOUT,"------\nUploading existing module..\n");
                           module_index = ret_val;
                           already_loaded_modules[module_index] = 1;
                           modifying = TRUE;        
                        }
                     }
                  } else if ((!xmlStrcmp(module_atr->name,BAD_CAST "path"))) {
                     key = xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1);
                     if (key == NULL) {
                        module_ptr = module_ptr->next;
                        module_ptr = module_ptr->next;
                        if (module_ptr == NULL) {
                           last_module = TRUE;
                           break;
                        } else {
                           if ((module_index < original_loaded_modules_cnt) && (module_index != -1)) {
                              already_loaded_modules[module_index] = 0;               
                           }
                           module_index = -1;
                           memset(needed_tags,0,2*sizeof(int));
                           module_atr = module_ptr->xmlChildrenNode;
                           continue;
                        }
                     } else {
                        needed_tags[1]++;
                     }
                  }

                  if (key != NULL) {
                     xmlFree(key);
                     key = NULL;
                  }
                  module_atr = module_atr->next;
               }
               
               if (last_module) {
                  break;
               }
               if (key != NULL) {
                  xmlFree(key);
                  key = NULL;
               }
               if ((needed_tags[0] != 1) || (needed_tags[1] != 1)) {
                  if ((module_index < original_loaded_modules_cnt) && (module_index != -1)) {
                     already_loaded_modules[module_index] = 0;               
                  }
                  module_ptr = module_ptr->next;
                  module_ptr = module_ptr->next;
                  continue;
               }

               module_atr = module_ptr->xmlChildrenNode;
               while (module_atr != NULL) {
                  if ((!xmlStrcmp(module_atr->name,BAD_CAST "enabled"))) {
                     key = xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1);
                     if (key == NULL) {
                        running_modules[module_index].module_enabled = FALSE;
                     } else {
                        if (strncmp(key, "true", strlen(key)) == 0 && running_modules[module_index].module_enabled == FALSE) {
                           running_modules[module_index].module_enabled = TRUE;
                           running_modules[module_index].module_restart_cnt = -1;
                           running_modules[module_index].module_modified_by_reload = TRUE;
                        } else if (strncmp(key, "false", strlen(key)) == 0 && running_modules[module_index].module_enabled == TRUE){
                           running_modules[module_index].module_enabled = FALSE;
                           running_modules[module_index].module_modified_by_reload = TRUE;
                        }
                        if (key != NULL) {
                           xmlFree(key);
                           key = NULL;
                        }
                     }
                  } else if ((!xmlStrcmp(module_atr->name,BAD_CAST "params"))) {
                     key = xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1);
                     if (modifying) {
                        if (running_modules[module_index].module_params != NULL && key != NULL) {
                           if (strcmp(key, running_modules[module_index].module_params) != 0) {
                              running_modules[module_index].module_modified_by_reload = TRUE;
                              if (running_modules[module_index].module_params != NULL) {
                                 free(running_modules[module_index].module_params);
                                 running_modules[module_index].module_params = NULL;
                              }
                              str_len = strlen((char *) key);
                              running_modules[module_index].module_params = (char *) calloc (str_len+1, sizeof(char));
                              strncpy(running_modules[module_index].module_params, (char *) key, str_len+1);
                           }
                           if (key != NULL) {
                              xmlFree(key);
                              key = NULL;
                           }
                        } else if (running_modules[module_index].module_params == NULL && key == NULL) {
                           // new one and old one NULL -> OK
                        } else if (running_modules[module_index].module_params == NULL) {
                           running_modules[module_index].module_modified_by_reload = TRUE;
                           str_len = strlen((char *) key);
                           running_modules[module_index].module_params = (char *) calloc (str_len+1, sizeof(char));
                           strncpy(running_modules[module_index].module_params, (char *) key, str_len+1);
                           if (key != NULL) {
                              xmlFree(key);
                              key = NULL;
                           }
                        } else if (key == NULL) {
                           running_modules[module_index].module_modified_by_reload = TRUE;
                           if (running_modules[module_index].module_params != NULL) {
                              free(running_modules[module_index].module_params);
                              running_modules[module_index].module_params = NULL;
                           }
                        }
                     } else {
                        if (key == NULL) {
                        running_modules[module_index].module_params = NULL;
                        } else {
                           str_len = strlen((char *) key);
                           running_modules[module_index].module_params = (char *) calloc (str_len+1, sizeof(char));
                           strncpy(running_modules[module_index].module_params, (char *) key, str_len+1);
                           if (key != NULL) {
                              xmlFree(key);
                              key = NULL;
                           }
                        }
                     }
                  } else if ((!xmlStrcmp(module_atr->name,BAD_CAST "name"))) {
                     key = xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1);
                     if (modifying) {
                        if (running_modules[module_index].module_name != NULL && key != NULL) {
                           if (strcmp(key, running_modules[module_index].module_name) != 0) {
                              running_modules[module_index].module_modified_by_reload = TRUE;
                              if (running_modules[module_index].module_name != NULL) {
                                 free(running_modules[module_index].module_name);
                                 running_modules[module_index].module_name = NULL;
                              }
                              str_len = strlen((char *) key);
                              running_modules[module_index].module_name = (char *) calloc (str_len+1, sizeof(char));
                              strncpy(running_modules[module_index].module_name, (char *) key, str_len+1);
                           }
                           if (key != NULL) {
                              xmlFree(key);
                              key = NULL;
                           }
                        } else if (running_modules[module_index].module_name == NULL && key == NULL) {
                           // new one and old one NULL -> OK
                        } else if (running_modules[module_index].module_name == NULL) {
                           running_modules[module_index].module_modified_by_reload = TRUE;
                           str_len = strlen((char *) key);
                           running_modules[module_index].module_name = (char *) calloc (str_len+1, sizeof(char));
                           strncpy(running_modules[module_index].module_name, (char *) key, str_len+1);
                           if (key != NULL) {
                              xmlFree(key);
                              key = NULL;
                           }
                        } else if (key == NULL) {
                           running_modules[module_index].module_modified_by_reload = TRUE;
                           if (running_modules[module_index].module_name != NULL) {
                              free(running_modules[module_index].module_name);
                              running_modules[module_index].module_name = NULL;
                           }
                        }
                     } else {
                        if (key == NULL) {
                        running_modules[module_index].module_name = NULL;
                        } else {
                           str_len = strlen((char *) key);
                           running_modules[module_index].module_name = (char *) calloc (str_len+1, sizeof(char));
                           strncpy(running_modules[module_index].module_name, (char *) key, str_len+1);
                           if (key != NULL) {
                              xmlFree(key);
                              key = NULL;
                           }
                        }
                     }
                  } else if ((!xmlStrcmp(module_atr->name,BAD_CAST "path"))) {
                     key = xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1);
                     if (modifying) {
                        if (running_modules[module_index].module_path != NULL && key != NULL) {
                           if (strcmp(key, running_modules[module_index].module_path) != 0) {
                              running_modules[module_index].module_modified_by_reload = TRUE;
                              if (running_modules[module_index].module_path != NULL) {
                                 free(running_modules[module_index].module_path);
                                 running_modules[module_index].module_path = NULL;
                              }
                              str_len = strlen((char *) key);
                              running_modules[module_index].module_path = (char *) calloc (str_len+1, sizeof(char));
                              strncpy(running_modules[module_index].module_path, (char *) key, str_len+1);
                           }
                           if (key != NULL) {
                              xmlFree(key);
                              key = NULL;
                           }
                        } else if (running_modules[module_index].module_path == NULL && key == NULL) {
                           // new one and old one NULL -> OK
                        } else if (running_modules[module_index].module_path == NULL) {
                           running_modules[module_index].module_modified_by_reload = TRUE;
                           str_len = strlen((char *) key);
                           running_modules[module_index].module_path = (char *) calloc (str_len+1, sizeof(char));
                           strncpy(running_modules[module_index].module_path, (char *) key, str_len+1);
                           if (key != NULL) {
                              xmlFree(key);
                              key = NULL;
                           }
                        } else if (key == NULL) {
                           running_modules[module_index].module_modified_by_reload = TRUE;
                           if (running_modules[module_index].module_path != NULL) {
                              free(running_modules[module_index].module_path);
                              running_modules[module_index].module_path = NULL;
                           }
                        }
                     } else {
                        if (key == NULL) {
                        running_modules[module_index].module_path = NULL;
                        } else {
                           str_len = strlen((char *) key);
                           running_modules[module_index].module_path = (char *) calloc (str_len+1, sizeof(char));
                           strncpy(running_modules[module_index].module_path, (char *) key, str_len+1);
                           if (key != NULL) {
                              xmlFree(key);
                              key = NULL;
                           }
                        }
                     }
                  } else if ((!xmlStrcmp(module_atr->name,BAD_CAST "trapinterfaces"))) {
                     ifc_cnt=0;
                     ifc_ptr = module_atr->xmlChildrenNode;

                     if (modifying) {
                        original_module_ifc_cnt = running_modules[module_index].module_ifces_cnt;
                        new_module_ifc_cnt = 0;

                        while (ifc_ptr != NULL) {
                           if (!xmlStrcmp(ifc_ptr->name,BAD_CAST "interface")) {
                              new_module_ifc_cnt++;
                           }
                           ifc_ptr = ifc_ptr->next;
                        }
                        if (original_module_ifc_cnt != new_module_ifc_cnt) {
                           // VERBOSE(N_STDOUT,"Original ifc cnt:%d, New ifc cnt:%d -> reloading module ifces..\n", original_module_ifc_cnt, new_module_ifc_cnt);                     
                           running_modules[module_index].module_modified_by_reload = TRUE;
                           free_module_interfaces_on_index(module_index);
                           running_modules[module_index].module_ifces_cnt = 0;
                           running_modules[module_index].module_num_out_ifc = 0;
                           running_modules[module_index].module_num_in_ifc = 0;
                           modifying = FALSE;
                        }
                        ifc_ptr = module_atr->xmlChildrenNode;
                     }

                     while (ifc_ptr != NULL) {
                        if (!xmlStrcmp(ifc_ptr->name,BAD_CAST "interface")) {
                           ifc_atr = ifc_ptr->xmlChildrenNode;
                           if (running_modules[module_index].module_ifces_array_size == 0) {
                              running_modules[module_index].module_ifces = (interface_t *) calloc(IFCES_ARRAY_START_SIZE, sizeof(interface_t));
                              running_modules[module_index].module_ifces_array_size = IFCES_ARRAY_START_SIZE;
                              running_modules[module_index].module_ifces_cnt = 0;
                           } else if (ifc_cnt == running_modules[module_index].module_ifces_array_size) {
                              // VERBOSE(N_STDOUT,"REALLOCING MODULE INTERFACES ARRAY --->\n");
                              int origin_size = running_modules[module_index].module_ifces_array_size;
                              running_modules[module_index].module_ifces_array_size += running_modules[module_index].module_ifces_array_size/2;
                              running_modules[module_index].module_ifces = (interface_t *) realloc (running_modules[module_index].module_ifces, (running_modules[module_index].module_ifces_array_size) * sizeof(interface_t));
                              memset(running_modules[module_index].module_ifces + origin_size,0,(origin_size/2)*sizeof(interface_t));
                           }

                           while (ifc_atr != NULL) {
                              if ((!xmlStrcmp(ifc_atr->name,BAD_CAST "note"))) {
                                 key =xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1);
                                 if (modifying) {
                                    if (running_modules[module_index].module_ifces[ifc_cnt].ifc_note != NULL && key != NULL) {
                                       if (strcmp(key, running_modules[module_index].module_ifces[ifc_cnt].ifc_note) != 0) {
                                          running_modules[module_index].module_modified_by_reload = TRUE;
                                          free_module_interfaces_on_index(module_index);
                                          ifc_cnt = -1;
                                          ifc_ptr = module_atr->xmlChildrenNode;
                                          running_modules[module_index].module_ifces_cnt = -1;
                                          running_modules[module_index].module_num_out_ifc = 0;
                                          running_modules[module_index].module_num_in_ifc = 0;
                                          modifying = FALSE;
                                          if (key != NULL) {
                                             xmlFree(key);
                                             key = NULL;
                                          }
                                          break;
                                       }
                                       if (key != NULL) {
                                          xmlFree(key);
                                          key = NULL;
                                       }
                                    } else if (running_modules[module_index].module_ifces[ifc_cnt].ifc_note == NULL && key == NULL) {
                                       // new one and old one NULL -> OK
                                    } else {
                                       running_modules[module_index].module_modified_by_reload = TRUE;
                                       free_module_interfaces_on_index(module_index);
                                       ifc_cnt = -1;
                                       ifc_ptr = module_atr->xmlChildrenNode;
                                       running_modules[module_index].module_ifces_cnt = -1;
                                       running_modules[module_index].module_num_out_ifc = 0;
                                       running_modules[module_index].module_num_in_ifc = 0;
                                       modifying = FALSE;
                                       if (key != NULL) {
                                          xmlFree(key);
                                          key = NULL;
                                       }
                                       break;
                                    }
                                 } else {
                                    if (key == NULL) {
                                       running_modules[module_index].module_ifces[ifc_cnt].ifc_note = NULL;
                                    } else {
                                       str_len = strlen((char *) key);
                                       running_modules[module_index].module_ifces[ifc_cnt].ifc_note = (char *) calloc (str_len+1, sizeof(char));
                                       strncpy(running_modules[module_index].module_ifces[ifc_cnt].ifc_note , (char *) key, str_len+1);
                                       if (key != NULL) {
                                          xmlFree(key);
                                          key = NULL;
                                       }
                                    }
                                 }
                              } else if ((!xmlStrcmp(ifc_atr->name,BAD_CAST "type"))) {
                                 key =xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1);
                                 if (modifying) {
                                    if (running_modules[module_index].module_ifces[ifc_cnt].ifc_type != NULL && key != NULL) {
                                       if (strcmp(key, running_modules[module_index].module_ifces[ifc_cnt].ifc_type) != 0) {
                                          running_modules[module_index].module_modified_by_reload = TRUE;
                                          free_module_interfaces_on_index(module_index);
                                          ifc_cnt = -1;
                                          ifc_ptr = module_atr->xmlChildrenNode;
                                          running_modules[module_index].module_ifces_cnt = -1;
                                          running_modules[module_index].module_num_out_ifc = 0;
                                          running_modules[module_index].module_num_in_ifc = 0;
                                          modifying = FALSE;
                                          if (key != NULL) {
                                             xmlFree(key);
                                             key = NULL;
                                          }
                                          break;
                                       }
                                       if (key != NULL) {
                                          xmlFree(key);
                                          key = NULL;
                                       }
                                    } else if (running_modules[module_index].module_ifces[ifc_cnt].ifc_type == NULL && key == NULL) {
                                       // new one and old one NULL -> OK
                                    } else {
                                       running_modules[module_index].module_modified_by_reload = TRUE;
                                       free_module_interfaces_on_index(module_index);
                                       ifc_cnt = -1;
                                       ifc_ptr = module_atr->xmlChildrenNode;
                                       running_modules[module_index].module_ifces_cnt = -1;
                                       running_modules[module_index].module_num_out_ifc = 0;
                                       running_modules[module_index].module_num_in_ifc = 0;
                                       modifying = FALSE;
                                       if (key != NULL) {
                                          xmlFree(key);
                                          key = NULL;
                                       }
                                       break;
                                    }
                                 } else {
                                    if (key == NULL) {
                                       running_modules[module_index].module_ifces[ifc_cnt].ifc_type = NULL;
                                    } else {
                                       str_len = strlen((char *) key);
                                       running_modules[module_index].module_ifces[ifc_cnt].ifc_type = (char *) calloc (str_len+1, sizeof(char));
                                       strncpy(running_modules[module_index].module_ifces[ifc_cnt].ifc_type, (char *) key, str_len+1);
                                       if (key != NULL) {
                                          xmlFree(key);
                                          key = NULL;
                                       }
                                    }
                                 }
                              } else if ((!xmlStrcmp(ifc_atr->name,BAD_CAST "direction"))) {
                                 key =xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1);
                                 if (modifying) {
                                    if (running_modules[module_index].module_ifces[ifc_cnt].ifc_direction != NULL && key != NULL) {
                                       if (strcmp(key, running_modules[module_index].module_ifces[ifc_cnt].ifc_direction) != 0) {
                                          running_modules[module_index].module_modified_by_reload = TRUE;
                                          free_module_interfaces_on_index(module_index);
                                          ifc_cnt = -1;
                                          ifc_ptr = module_atr->xmlChildrenNode;
                                          running_modules[module_index].module_ifces_cnt = -1;
                                          running_modules[module_index].module_num_out_ifc = 0;
                                          running_modules[module_index].module_num_in_ifc = 0;
                                          modifying = FALSE;
                                          if (key != NULL) {
                                             xmlFree(key);
                                             key = NULL;
                                          }
                                          break;
                                       }
                                       if (key != NULL) {
                                          xmlFree(key);
                                          key = NULL;
                                       }
                                    } else if (running_modules[module_index].module_ifces[ifc_cnt].ifc_direction == NULL && key == NULL) {
                                       // new one and old one NULL -> OK
                                    } else {
                                       running_modules[module_index].module_modified_by_reload = TRUE;
                                       free_module_interfaces_on_index(module_index);
                                       ifc_cnt = -1;
                                       ifc_ptr = module_atr->xmlChildrenNode;
                                       running_modules[module_index].module_ifces_cnt = -1;
                                       running_modules[module_index].module_num_out_ifc = 0;
                                       running_modules[module_index].module_num_in_ifc = 0;
                                       modifying = FALSE;
                                       if (key != NULL) {
                                          xmlFree(key);
                                          key = NULL;
                                       }
                                       break;
                                    }
                                 } else {
                                    if (key == NULL) {
                                       running_modules[module_index].module_ifces[ifc_cnt].ifc_direction= NULL;
                                    } else {
                                       str_len = strlen((char *) key);
                                       running_modules[module_index].module_ifces[ifc_cnt].ifc_direction = (char *) calloc (str_len+1, sizeof(char));
                                       strncpy(running_modules[module_index].module_ifces[ifc_cnt].ifc_direction, (char *) key, str_len+1);
                                       if (key != NULL) {
                                          xmlFree(key);
                                          key = NULL;
                                       }
                                    }
                                 }
                              } else if ((!xmlStrcmp(ifc_atr->name,BAD_CAST "params"))) {
                                 key =xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1);
                                 if (modifying) {
                                    if (running_modules[module_index].module_ifces[ifc_cnt].ifc_params != NULL && key != NULL) {
                                       if (strcmp(key, running_modules[module_index].module_ifces[ifc_cnt].ifc_params) != 0) {
                                          running_modules[module_index].module_modified_by_reload = TRUE;
                                          free_module_interfaces_on_index(module_index);
                                          ifc_cnt = -1;
                                          ifc_ptr = module_atr->xmlChildrenNode;
                                          running_modules[module_index].module_ifces_cnt = -1;
                                          running_modules[module_index].module_num_out_ifc = 0;
                                          running_modules[module_index].module_num_in_ifc = 0;
                                          modifying = FALSE;
                                          if (key != NULL) {
                                             xmlFree(key);
                                             key = NULL;
                                          }
                                          break;
                                       }
                                       if (key != NULL) {
                                          xmlFree(key);
                                          key = NULL;
                                       }
                                    } else if (running_modules[module_index].module_ifces[ifc_cnt].ifc_params == NULL && key == NULL) {
                                       // new one and old one NULL -> OK
                                    } else {
                                       running_modules[module_index].module_modified_by_reload = TRUE;
                                       free_module_interfaces_on_index(module_index);
                                       ifc_cnt = -1;
                                       ifc_ptr = module_atr->xmlChildrenNode;
                                       running_modules[module_index].module_ifces_cnt = -1;
                                       running_modules[module_index].module_num_out_ifc = 0;
                                       running_modules[module_index].module_num_in_ifc = 0;
                                       modifying = FALSE;
                                       if (key != NULL) {
                                          xmlFree(key);
                                          key = NULL;
                                       }
                                       break;
                                    }
                                 } else {
                                    if (key == NULL) {
                                       running_modules[module_index].module_ifces[ifc_cnt].ifc_params = NULL;
                                    } else {
                                       str_len = strlen((char *) key);
                                       running_modules[module_index].module_ifces[ifc_cnt].ifc_params = (char *) calloc (str_len+1+10, sizeof(char));
                                       strncpy(running_modules[module_index].module_ifces[ifc_cnt].ifc_params, (char *) key, str_len+1);
                                       if (key != NULL) {
                                          xmlFree(key);
                                          key = NULL;
                                       }
                                    }
                                 }
                              }
                              ifc_atr=ifc_atr->next;
                           }

                           ifc_cnt++;
                           if (!modifying) {
                              running_modules[module_index].module_ifces_cnt++;
                           }
                        }
                        ifc_ptr = ifc_ptr->next;
                     }
                  }

                  module_atr = module_atr->next;
               }
               if (!modifying) {
                  running_modules[module_index].module_num_in_ifc = 0;
                  running_modules[module_index].module_num_in_ifc = 0;
                  for (x=0; x<running_modules[module_index].module_ifces_cnt; x++) {
                     if (running_modules[module_index].module_ifces[x].ifc_direction != NULL) {
                        if (strncmp(running_modules[module_index].module_ifces[x].ifc_direction, "IN", 2) == 0) {
                           running_modules[module_index].module_num_in_ifc++;
                        } else if (strncmp(running_modules[module_index].module_ifces[x].ifc_direction, "OUT", 3) == 0) {
                           running_modules[module_index].module_num_out_ifc++;
                        }
                     }
                  }
               }
               if (module_index == loaded_modules_cnt) {
                  loaded_modules_cnt++;
                  inserted_modules++;
               }

               if (modules_got_profile) {
                  running_modules[module_index].modules_profile = actual_profile_ptr->profile_name;
               }
            }
            module_ptr = module_ptr->next;
         }
      }
      current_node = current_node->next;
   }

   xmlFreeDoc(xml_tree);
   xmlCleanupParser();


   int deleted_modules_cnt = -1;
   // VERBOSE(N_STDOUT,"-- Finishing --\n");
   for (x=0; x<original_loaded_modules_cnt; x++) {
      if (already_loaded_modules[x] == 0) {
         removed_modules++;
         deleted_modules_cnt++;
         if (running_modules[x].module_status) {
            kill(running_modules[x].module_pid, 2);
         }
         // VERBOSE(N_STDOUT,"Deleting module %d\n",x);
         free_module_on_index(x-deleted_modules_cnt);
         running_modules[x-deleted_modules_cnt].module_ifces_cnt = 0;
         running_modules[x-deleted_modules_cnt].module_num_out_ifc = 0;
         running_modules[x-deleted_modules_cnt].module_num_in_ifc = 0;
         running_modules[x-deleted_modules_cnt].module_ifces_array_size = 0;
         for (y=(x-deleted_modules_cnt); y<(loaded_modules_cnt-1); y++) {
            // VERBOSE(N_STDOUT,"posouvam %d<-%d\n",y,y+1);
            memcpy(&running_modules[y], &running_modules[y+1], sizeof(running_module_t));
         }
         loaded_modules_cnt--;
         memset(&running_modules[loaded_modules_cnt], 0, sizeof(running_module_t));
      }
   }

   destroy_graph(graph_first_node);
   graph_first_node = NULL;
   graph_last_node = NULL;
   for (x=0; x<loaded_modules_cnt; x++) {
      running_modules[x].module_served_by_service_thread = FALSE;
      if (running_modules[x].module_running && running_modules[x].module_modified_by_reload) {   
         if (running_modules[x].module_counters_array != NULL) {
            free(running_modules[x].module_counters_array);
            running_modules[x].module_counters_array = NULL;
         }
         running_modules[x].module_running = FALSE;
         if (running_modules[x].module_status) {
            kill(running_modules[x].module_pid, 2);   
         }
      }
   }

   for (x=0; x<loaded_modules_cnt; x++) {
      if (running_modules[x].module_modified_by_reload) {
         modified_modules++;
      }
   }

   /* update module_enabled according to profile enabled */
   modules_profile_t * ptr = first_profile_ptr;
   modules_profile_t * p = NULL;
   while (ptr != NULL) {
      for (x=0; x<loaded_modules_cnt; x++) {
         if (running_modules[x].modules_profile == ptr->profile_name) {
            running_modules[x].module_enabled = (running_modules[x].module_enabled && ptr->profile_enabled);
         }
      }
      p = ptr;
      ptr = ptr->next;
      if (p->profile_name != NULL) {
         free(p->profile_name);
      }
      if (p != NULL) {
         free(p);
      }
   }

   VERBOSE(N_STDOUT,"Inserted modules:\t%d\n", inserted_modules);
   VERBOSE(N_STDOUT,"Removed modules:\t%d\n", removed_modules);
   VERBOSE(N_STDOUT,"Modified modules:\t%d\n", modified_modules);
   VERBOSE(N_STDOUT,"Unmodified modules:\t%d\n", original_loaded_modules_cnt - modified_modules - removed_modules);

   pthread_mutex_unlock(&running_modules_lock);
   return TRUE;
}

int nc_supervisor_initialization()
{
   int y = 0;

   logs_path = NULL;
   config_file = NULL;
   socket_path = DEFAULT_DAEMON_UNIXSOCKET_PATH_FILENAME;
   verbose_flag = FALSE;
   help_flag = FALSE;
   file_flag = FALSE;
   daemon_flag = FALSE;
   netconf_flag = TRUE;
   graph_first_node = NULL;
   graph_last_node = NULL;

   input_fd = stdin;
   output_fd = stdout;

   create_output_dir();
   create_output_files_strings();

   VERBOSE(N_STDOUT,"--- LOADING CONFIGURATION ---\n");
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

   VERBOSE(N_STDOUT,"-- Starting service thread --\n");
   start_service_thread();

   /* function prototype to set handler */
   void sigpipe_handler(int sig);
   struct sigaction sa;

   sa.sa_handler = sigpipe_handler;
   sa.sa_flags = 0;
   sigemptyset(&sa.sa_mask);

   if (sigaction(SIGPIPE,&sa,NULL) == -1) {
      VERBOSE(N_STDOUT,"ERROR sigaction !!\n");
   }

   return 0;
}

xmlDocPtr nc_get_state_data()
{
   int x, y, in_ifc_cnt, out_ifc_cnt;
   char buffer[20];
   const char *templ = "<?xml version=\"1.0\"?><nemea-supervisor xmlns=\"urn:cesnet:tmc:nemea:1.0\"><modules/></nemea-supervisor>";
   xmlDocPtr resp = NULL;
   xmlNodePtr modules = NULL, module = NULL, trapinterfaces = NULL, interface = NULL;

   if (loaded_modules_cnt > 0) {
      resp = xmlParseMemory(templ, strlen(templ));
      if (resp == NULL) {
         return NULL;
      }

      modules = xmlDocGetRootElement(resp);
      modules = modules->children;

      for (x = 0; x < loaded_modules_cnt; x++) {
         memset(buffer,0,20);
         module = xmlNewChild(modules, NULL, BAD_CAST "module", NULL);
         xmlNewChild(module, NULL, BAD_CAST "name", running_modules[x].module_name);

         if (running_modules[x].module_status == TRUE) {
            xmlNewChild(module, NULL, BAD_CAST "running", "true");
         } else {
            xmlNewChild(module, NULL, BAD_CAST "running", "false");
         }

         if (running_modules[x].module_restart_cnt < 0) {
            sprintf(buffer,"%d",0);
            xmlNewChild(module, NULL, BAD_CAST "restart-counter", buffer);
         } else {
            sprintf(buffer,"%d",running_modules[x].module_restart_cnt);
            xmlNewChild(module, NULL, BAD_CAST "restart-counter", buffer);
         }

         if (running_modules[x].module_has_service_ifc && running_modules[x].module_status) {
            in_ifc_cnt = 0;
            out_ifc_cnt = 0;
            trapinterfaces = xmlNewChild(module, NULL, BAD_CAST "trapinterfaces", NULL);
            for (y=0; y<running_modules[x].module_ifces_cnt; y++) {
               if (running_modules[x].module_ifces[y].ifc_direction != NULL) {
                  interface = xmlNewChild(trapinterfaces, NULL, BAD_CAST "interface", NULL);
                  xmlNewChild(interface, NULL, BAD_CAST "type", running_modules[x].module_ifces[y].ifc_type);
                  xmlNewChild(interface, NULL, BAD_CAST "params", running_modules[x].module_ifces[y].ifc_params);
                  if (strcmp(running_modules[x].module_ifces[y].ifc_direction, "IN") == 0) {
                     memset(buffer,0,20);
                     sprintf(buffer,"%d",running_modules[x].module_counters_array[in_ifc_cnt]);
                     xmlNewChild(interface, NULL, BAD_CAST "recv-msg-cnt", buffer);
                     in_ifc_cnt++;
                  } else if (strcmp(running_modules[x].module_ifces[y].ifc_direction, "OUT") == 0) {
                     memset(buffer,0,20);
                     sprintf(buffer,"%d",running_modules[x].module_counters_array[running_modules[x].module_num_in_ifc + out_ifc_cnt]);
                     xmlNewChild(interface, NULL, BAD_CAST "sent-msg-cnt", buffer);
                     memset(buffer,0,20);
                     sprintf(buffer,"%d",running_modules[x].module_counters_array[running_modules[x].module_num_in_ifc + running_modules[x].module_num_out_ifc + out_ifc_cnt]);
                     xmlNewChild(interface, NULL, BAD_CAST "sent-buffer-cnt", buffer);
                     memset(buffer,0,20);
                     sprintf(buffer,"%d",running_modules[x].module_counters_array[running_modules[x].module_num_in_ifc + 2*running_modules[x].module_num_out_ifc + out_ifc_cnt]);
                     xmlNewChild(interface, NULL, BAD_CAST "autoflush-cnt", buffer);
                     in_ifc_cnt++;
                  }
               }
            }
         }

         /* TODO check and free */
         if (xmlAddChild(modules, module) == NULL) {
            xmlFree(module);
         }
      }
   }
   return resp;
}