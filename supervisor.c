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
#include <time.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <ifaddrs.h>

#define LOGSDIR_NAME             "./modules_logs/" ///< Name of directory with modules logs
#define TRAP_PARAM               "-i" ///< Interface parameter for libtrap
#define MAX_RESTARTS_PER_MINUTE  3  ///< Maximum number of module restarts per minute
#define DEFAULT_SIZE_OF_BUFFER   100

#define DAEMON_STOP_CODE 951753
#define DAEMON_CONFIG_MODE_CODE 789123
#define DAEMON_STATS_MODE_CODE 456987

#define UNIX_PATH_FILENAME_FORMAT   "/tmp/trap-localhost-%s.sock" ///< Modules output interfaces socket, to which connects service thread.
#define DAEMON_UNIX_PATH_FILENAME_FORMAT  "/tmp/supervisor_daemon.sock"  ///<  Daemon socket.

/*******GLOBAL VARIABLES*******/
running_module_t *   running_modules;  ///< Information about running modules

int         running_modules_array_size;  ///< Current size of running_modules array.
int         loaded_modules_cnt; ///< Current number of loaded modules.

pthread_mutex_t running_modules_lock; ///< mutex for locking counters
int         service_thread_continue; ///< condition variable of main loop of the service_thread

graph_node_t *    graph_first_node; ///< First node of graph nodes list.
graph_node_t *    graph_last_node; ///< Last node of graph nodes list.

pthread_t   service_thread_id; ///< Service thread identificator.
pthread_t   acceptor_thread_id; ///< Acceptor thread identificator.

// supervisor flags
int   help_flag;        // -h argument
int   file_flag;        // -f "file" arguments
char *config_file = NULL;
char *socket_path = NULL;
int   show_cpu_usage_flag; // --show-cpuusage
int   verbose_flag;     // -v
int   daemon_flag;      // --daemon


int         remote_supervisor_socketd; ///< Socket descriptor of remote_supervisor.
int         remote_supervisor_connected; ///< TRUE if remote_supervisor is connected.
char        remote_supervisor_address[50]; ///< Remote_supervisor IP address (if connected).
char        supervisor_address[50]; ///< Local IP address.

int         start_range_reserved_ports; ///< Initial reserved port by supervisor.
int         next_reserved_port; ///< Next reserved port by supervisor.

/**************************************/

union tcpip_socket_addr {
   struct addrinfo tcpip_addr; ///< used for TCPIP socket
   struct sockaddr_un unix_addr; ///< used for path of UNIX socket
};

void * remote_supervisor_accept_routine();
void start_service_thread();
// void stop_threads();
int parse_arguments(int *argc, char **argv);
void print_help();
void daemon_mode();
char *get_param_by_delimiter(const char *source, char **dest, const char delimiter);


void get_local_IP ()
{
   struct ifaddrs *myaddrs, *ifa;
   void *in_addr;

   if (getifaddrs(&myaddrs) != 0) {
      perror("getifaddrs");
      /* TODO Do we really want to exit the whole program? */
      exit(1);
   }

   for (ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next) {
      if (ifa->ifa_addr == NULL) {
         continue;
      }
      if (!(ifa->ifa_flags & IFF_UP)) {
         continue;
      }

      switch (ifa->ifa_addr->sa_family) {
      case AF_INET: {
            struct sockaddr_in *s4 = (struct sockaddr_in *)ifa->ifa_addr;
            in_addr = &s4->sin_addr;
            if (strcmp(ifa->ifa_name,"eth0") == 0) {
               if (!inet_ntop(ifa->ifa_addr->sa_family, in_addr, supervisor_address, 50)) {
                  // printf("%s: inet_ntop failed!\n", ifa->ifa_name);
               } else {
                  // printf("%s: %s\n", ifa->ifa_name, supervisor_address);
               }
               freeifaddrs(myaddrs);
               return;
            }
            break;
         }

      case AF_INET6: {
            struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)ifa->ifa_addr;
            in_addr = &s6->sin6_addr;
            break;
         }

      default:
         continue;
      }
   }

   freeifaddrs(myaddrs);
}

/**if choice TRUE -> parse file, else parse buffer*/
int load_configuration(const int choice, const char * buffer)
{
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
                  xmlFree(key);
               }
            } else if ((!xmlStrcmp(module_atr->name,BAD_CAST "params"))) {
               key = xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1);
               if (key == NULL) {
                  running_modules[loaded_modules_cnt].module_params = NULL;
               } else {
                  str_len = strlen((char *) key);
                  running_modules[loaded_modules_cnt].module_params = (char *) calloc (str_len+1, sizeof(char));
                  strncpy(running_modules[loaded_modules_cnt].module_params, (char *) key, str_len+1);
                  xmlFree(key);
               }
            } else if ((!xmlStrcmp(module_atr->name,BAD_CAST "name"))) {
               key = xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1);
               if (key == NULL) {
                  running_modules[loaded_modules_cnt].module_name = NULL;
               } else {
                  str_len = strlen((char *) key);
                  running_modules[loaded_modules_cnt].module_name = (char *) calloc (str_len+1, sizeof(char));
                  strncpy(running_modules[loaded_modules_cnt].module_name, (char *) key, str_len+1);
                  xmlFree(key);
               }
            } else if ((!xmlStrcmp(module_atr->name,BAD_CAST "path"))) {
               key = xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1);
               if (key == NULL) {
                  running_modules[loaded_modules_cnt].module_path = NULL;
               } else {
                  str_len = strlen((char *) key);
                  running_modules[loaded_modules_cnt].module_path = (char *) calloc (str_len+1, sizeof(char));
                  strncpy(running_modules[loaded_modules_cnt].module_path, (char *) key, str_len+1);
                  xmlFree(key);
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
                              xmlFree(key);
                           }
                        } else if ((!xmlStrcmp(ifc_atr->name,BAD_CAST "type"))) {
                           key =xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1);
                           if (key == NULL) {
                              running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_type = NULL;
                           } else {
                              str_len = strlen((char *) key);
                              running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_type = (char *) calloc (str_len+1, sizeof(char));
                              strncpy(running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_type, (char *) key, str_len+1);
                              xmlFree(key);
                           }
                        } else if ((!xmlStrcmp(ifc_atr->name,BAD_CAST "direction"))) {
                           key =xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1);
                           if (key == NULL) {
                              running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_direction= NULL;
                           } else {
                              str_len = strlen((char *) key);
                              running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_direction = (char *) calloc (str_len+1, sizeof(char));
                              strncpy(running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_direction, (char *) key, str_len+1);
                              xmlFree(key);
                           }
                        } else if ((!xmlStrcmp(ifc_atr->name,BAD_CAST "params"))) {
                           key =xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1);
                           if (key == NULL) {
                              running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_params = NULL;
                           } else {
                              str_len = strlen((char *) key);
                              running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_params = (char *) calloc (str_len+1+10, sizeof(char));
                              strncpy(running_modules[loaded_modules_cnt].module_ifces[ifc_cnt].ifc_params, (char *) key, str_len+1);
                              xmlFree(key);
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

   for (x=0; x<running_modules[number_of_module].module_ifces_cnt; x++) {
      if (running_modules[number_of_module].module_ifces[x].ifc_direction != NULL) {
         if (!strncmp(running_modules[number_of_module].module_ifces[x].ifc_direction,"OUT", 3)) {
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

   for (x=0; x<running_modules[number_of_module].module_ifces_cnt; x++) {
      if (!strncmp(running_modules[number_of_module].module_ifces[x].ifc_type,"SERVICE", 7)) {
         strncpy(atr + ptr,"s",1);
         ptr++;
      }
   }

   strncpy(atr + ptr,";",1);
   ptr++;

   for (x=0; x<running_modules[number_of_module].module_ifces_cnt; x++) {
      if (running_modules[number_of_module].module_ifces[x].ifc_direction != NULL) {
         if (!strncmp(running_modules[number_of_module].module_ifces[x].ifc_direction,"IN", 2)) {
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

   for (x=0; x<running_modules[number_of_module].module_ifces_cnt; x++) {
      if (running_modules[number_of_module].module_ifces[x].ifc_direction != NULL) {
         if (!strncmp(running_modules[number_of_module].module_ifces[x].ifc_direction,"OUT", 3)) {
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

   for (x=0; x<running_modules[number_of_module].module_ifces_cnt; x++) {
      if (!strncmp(running_modules[number_of_module].module_ifces[x].ifc_type,"SERVICE", 7)) {
         if ((strlen(atr) + strlen(running_modules[number_of_module].module_ifces[x].ifc_params) + 1) >= (3*size_of_atr)/5) {
            size_of_atr += strlen(running_modules[number_of_module].module_ifces[x].ifc_params) + (size_of_atr/2);
            atr = (char *) realloc (atr, size_of_atr*sizeof(char));
            memset(atr + ptr, 0, size_of_atr - ptr);
         }
         sprintf(atr + ptr,"%s;",running_modules[number_of_module].module_ifces[x].ifc_params);
         ptr += strlen(running_modules[number_of_module].module_ifces[x].ifc_params) + 1;
      }
   }
   memset(atr + ptr-1,0,1);

   if (running_modules[number_of_module].module_params == NULL) {
      params = (char **) calloc (4,sizeof(char*));
      /* TODO check if params is not NULL!!! */

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
      /* TODO check if params is not NULL!!! */
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
         /* TODO why 32? */
         if (running_modules[number_of_module].module_params[x] == 32) {
            params[params_counter] = (char *) calloc (strlen(buffer)+1,sizeof(char));
            /* TODO check if params is not NULL */
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
      /* TODO check if params is not NULL */
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

   fscanf(input_fd, "%s", buffer);
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
   VERBOSE(N_STDOUT,"8. LOAD CONFIGURATION\n");
   VERBOSE(N_STDOUT,"9. STOP SUPERVISOR\n");

   return get_number_from_input();
}


void re_start_module(const int module_number)
{
   if (running_modules[module_number].module_running == FALSE) {
      VERBOSE(MODULE_EVENT,"Starting module %d_%s.\n", module_number, running_modules[module_number].module_name);
      running_modules[module_number].module_counters_array = (int *) calloc (3*running_modules[module_number].module_num_out_ifc + running_modules[module_number].module_num_in_ifc,sizeof(int));
      running_modules[module_number].remote_module = FALSE;
      running_modules[module_number].module_running = TRUE;
   } else {
      VERBOSE(MODULE_EVENT,"Restarting module %d_%s\n", module_number, running_modules[module_number].module_name);
   }

   char log_path_stdout[100];
   char log_path_stderr[100];
   sprintf(log_path_stdout,"%s%d_%s_stdout",LOGSDIR_NAME, module_number, running_modules[module_number].module_name);
   sprintf(log_path_stderr,"%s%d_%s_stderr",LOGSDIR_NAME, module_number, running_modules[module_number].module_name);

   memset(running_modules[module_number].module_counters_array, 0, (running_modules[module_number].module_num_in_ifc + (3*running_modules[module_number].module_num_out_ifc)) * sizeof(int));
   running_modules[module_number].last_cpu_usage_user_mode = 0;
   running_modules[module_number].last_cpu_usage_kernel_mode = 0;
   running_modules[module_number].module_service_sd = -1;

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
      char **params = make_module_arguments(running_modules[module_number].module_number);
      fflush(stdout);
      fflush(stderr);
      execvp(running_modules[module_number].module_path, params);
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

void stop_module (const int module_number)
{
   running_modules[module_number].module_enabled = FALSE;
   if (running_modules[module_number].module_status) {
      VERBOSE(MODULE_EVENT,"Stopping module %d_%s.\n", module_number, running_modules[module_number].module_name);
      kill(running_modules[module_number].module_pid,2);
   }
}

void update_module_status()
{
   int x;
   int status;
   pid_t result;

   for (x=0; x<loaded_modules_cnt; x++) {
      if (running_modules[x].module_running) {
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
      VERBOSE(N_STDOUT,"SIGPIPE catched..\n");
   }
}

int supervisor_initialization(int *argc, char **argv)
{
   start_range_reserved_ports = 12000;
   next_reserved_port = start_range_reserved_ports;
   get_local_IP();
   input_fd = stdin;
   output_fd = stdout;

   statistics_fd = fopen("./supervisor_log_statistics", "w");
   module_event_fd = fopen("./supervisor_log_module_event", "w");

   config_file = NULL;
   show_cpu_usage_flag = FALSE;
   verbose_flag = FALSE;
   help_flag = FALSE;
   file_flag = FALSE;
   int ret_val = parse_arguments(argc, argv);
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


   int y;
   VERBOSE(N_STDOUT,"---LOADING CONFIGURATION---\n");
   loaded_modules_cnt = 0;
   //init running_modules structures alloc size RUNNING_MODULES_ARRAY_START_SIZE
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

   //logs directory
   struct stat st = {0};
   if (stat(LOGSDIR_NAME, &st) == -1) {
      mkdir(LOGSDIR_NAME, PERM_LOGSDIR);
   }

   // verbose_level = 0;
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

   graph_first_node = NULL;
   graph_last_node = NULL;

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
   VERBOSE(MODULE_EVENT,"Configuration is running.\n");
   pthread_mutex_unlock(&running_modules_lock);
}


void interactive_stop_configuration()
{
   char * dest_port = NULL;
   char buffer[DEFAULT_SIZE_OF_BUFFER];
   memset(buffer, 0, DEFAULT_SIZE_OF_BUFFER);
   int x = 0, y = 0;

   pthread_mutex_lock(&running_modules_lock);
   VERBOSE(MODULE_EVENT,"Stopping configuration...\n");
   for (x=0; x<loaded_modules_cnt; x++) {
      if (running_modules[x].module_enabled) {
         stop_module(x);
      }
   }
   VERBOSE(MODULE_EVENT,"Configuration stopped.\n");
   pthread_mutex_unlock(&running_modules_lock);

   sleep(1);

   pthread_mutex_lock(&running_modules_lock);
   update_module_status();
   for (x=0; x<loaded_modules_cnt; x++) {
      if (running_modules[x].module_status) {
         kill(running_modules[x].module_pid,9);
         for (y=0; y<running_modules[x].module_ifces_cnt; y++) {
            if (strcmp(running_modules[x].module_ifces[y].ifc_type, "SERVICE") == 0 || ((strcmp(running_modules[x].module_ifces[y].ifc_type, "UNIXSOCKET") == 0)
                                                                                       && (strcmp(running_modules[x].module_ifces[y].ifc_direction, "OUT") == 0))) {
               get_param_by_delimiter(running_modules[x].module_ifces[y].ifc_params, &dest_port, ',');
               sprintf(buffer,UNIX_PATH_FILENAME_FORMAT,dest_port);
               unlink(buffer);
            }
         }
      }
   }
   pthread_mutex_unlock(&running_modules_lock);
   if (dest_port != NULL) {
      free(dest_port);      
   }
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


void interactive_stop_module()
{
   char * dest_port = NULL;
   char buffer[DEFAULT_SIZE_OF_BUFFER];
   memset(buffer, 0, DEFAULT_SIZE_OF_BUFFER);
   int x = 0, y = 0, running_modules_counter = 0;
   
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
      stop_module(x);
   }
   pthread_mutex_unlock(&running_modules_lock);

   sleep(1);

   pthread_mutex_lock(&running_modules_lock);
   update_module_status();
   if (running_modules[x].module_status) {
      kill(running_modules[x].module_pid,9);
      for (y=0; y<running_modules[x].module_ifces_cnt; y++) {
         if (strcmp(running_modules[x].module_ifces[y].ifc_type, "SERVICE") == 0 || ((strcmp(running_modules[x].module_ifces[y].ifc_type, "UNIXSOCKET") == 0)
                                                                                       && (strcmp(running_modules[x].module_ifces[y].ifc_direction, "OUT") == 0))) {
            get_param_by_delimiter(running_modules[x].module_ifces[y].ifc_params, &dest_port, ',');
            sprintf(buffer,UNIX_PATH_FILENAME_FORMAT,dest_port);
            unlink(buffer);
         }
      }
   }   
   pthread_mutex_unlock(&running_modules_lock);
   if (dest_port != NULL) {
      free(dest_port);      
   }
}


void restart_modules()
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
      } else if (running_modules[x].module_status == FALSE && running_modules[x].module_enabled == TRUE && running_modules[x].remote_module == FALSE) {
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
      } else if (running_modules[x].remote_module == TRUE) {
         VERBOSE(N_STDOUT,"%d_%s remote_module (PID: %d)\n",x, running_modules[x].module_name,running_modules[x].module_pid);
      } else {
         VERBOSE(N_STDOUT,"%d_%s stopped (PID: %d)\n",x, running_modules[x].module_name,running_modules[x].module_pid);
      }
   }
}

void supervisor_termination()
{
   int x, y;
   // stop_threads();
   VERBOSE(N_STDOUT,"-- Stopping modules --\n");
   interactive_stop_configuration();
   VERBOSE(N_STDOUT,"-- Aborting service thread --\n");
   service_thread_continue = 0;
   sleep(3);
   /* TODO use local pointer */
   for (x=0;x<loaded_modules_cnt;x++) {
      if (running_modules[x].module_counters_array != NULL) {
         free(running_modules[x].module_counters_array);
      }
   }
   for (x=0;x<running_modules_array_size;x++) {
      for (y=0; y<running_modules[x].module_ifces_cnt; y++) {
         /* TODO set every freed pointer to NULL */
         if (running_modules[x].module_ifces[y].ifc_note != NULL) {
            free(running_modules[x].module_ifces[y].ifc_note);
         }
         if (running_modules[x].module_ifces[y].ifc_type != NULL) {
            free(running_modules[x].module_ifces[y].ifc_type);
         }
         if (running_modules[x].module_ifces[y].ifc_direction != NULL) {
            free(running_modules[x].module_ifces[y].ifc_direction);
         }
         if (running_modules[x].module_ifces[y].ifc_params != NULL) {
            free(running_modules[x].module_ifces[y].ifc_params);
         }
      }
      if (running_modules[x].module_ifces != NULL) {
         free(running_modules[x].module_ifces);
      }
      if (running_modules[x].module_path != NULL) {
         free(running_modules[x].module_path);
      }
      if (running_modules[x].module_name != NULL) {
         free(running_modules[x].module_name);
      }
      if (running_modules[x].module_params != NULL) {
         free(running_modules[x].module_params);
      }
   }
   if (running_modules != NULL) {
      free(running_modules);
   }
   destroy_graph(graph_first_node);
   if (config_file != NULL) {
      free(config_file);
   }

   // close(remote_supervisor_socketd);
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

void update_cpu_usage(long int * last_total_cpu_usage)
{
   int utime = 0, stime = 0;
   long int new_total_cpu_usage = 0;
   FILE * proc_stat_fd = fopen("/proc/stat","r");
   int num = 0;
   int x;
   char path[20];
   memset(path,0,20);
   int difference_total = 0;

   if (fscanf(proc_stat_fd,"cpu") != 0) {
      return;
   }

   for (x=0; x<10; x++) {
      if (!fscanf(proc_stat_fd,"%d",&num)) {
         continue;
      }
      new_total_cpu_usage += num;
   }

   difference_total = new_total_cpu_usage - *last_total_cpu_usage;

   if (show_cpu_usage_flag) {
      VERBOSE(STATISTICS,"total_cpu difference total cpu usage> %d\n", difference_total);
   }
   *last_total_cpu_usage = new_total_cpu_usage;
   fclose(proc_stat_fd);

   for (x=0;x<loaded_modules_cnt;x++) {
      if (running_modules[x].module_status) {
         sprintf(path,"/proc/%d/stat",running_modules[x].module_pid);
         proc_stat_fd = fopen(path,"r");
         if (!fscanf(proc_stat_fd,"%*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %d %d", &utime , &stime)) {
            continue;
         }
         if (show_cpu_usage_flag) {
            VERBOSE(STATISTICS,"Last_u%d Last_s%d, New_u%d New_s%d |>>   u%d%% s%d%%\n",
                  running_modules[x].last_cpu_usage_user_mode,
                  running_modules[x].last_cpu_usage_kernel_mode,
                  utime, stime,
                  100 * (utime - running_modules[x].last_cpu_usage_user_mode)/difference_total,
                  100 * (stime - running_modules[x].last_cpu_usage_kernel_mode)/difference_total);
         }
         running_modules[x].percent_cpu_usage_kernel_mode = 100 * (stime - running_modules[x].last_cpu_usage_kernel_mode)/difference_total;
         running_modules[x].percent_cpu_usage_user_mode = 100 * (utime - running_modules[x].last_cpu_usage_user_mode)/difference_total;
         running_modules[x].last_cpu_usage_user_mode = utime;
         running_modules[x].last_cpu_usage_kernel_mode = stime;
         fclose(proc_stat_fd);
      }
   }
}

int count_struct_size(int module_num)
{
   int size = 0;
   int x;

   if (running_modules[module_num].module_path != NULL) {
      size += strlen(running_modules[module_num].module_path) + 1;
   }
   if (running_modules[module_num].module_name != NULL) {
      size += strlen(running_modules[module_num].module_name) + 1;
   }
   if (running_modules[module_num].module_params != NULL) {
      size += strlen(running_modules[module_num].module_params) + 1;
   }

   for (x=0; x<running_modules[module_num].module_ifces_cnt; x++) {
      // if (running_modules[module_num].module_ifces[x].ifc_note != NULL) {
      //    size += strlen(running_modules[module_num].module_ifces[x].ifc_note) + 1;
      // }
      if (running_modules[module_num].module_ifces[x].ifc_type != NULL) {
         size += strlen(running_modules[module_num].module_ifces[x].ifc_type) + 1;
      }
      if (running_modules[module_num].module_ifces[x].ifc_params != NULL) {
         size += strlen(running_modules[module_num].module_ifces[x].ifc_params) + 1;
      }
      if (running_modules[module_num].module_ifces[x].ifc_direction != NULL) {
         size += strlen(running_modules[module_num].module_ifces[x].ifc_direction) + 1;
      }
   }

   return size;
}

void send_to_remote_running_module_struct(int num_module)
{
   int ret_val = 0;
   int x;

   ret_val = send(remote_supervisor_socketd, &running_modules[num_module], sizeof(running_module_t), 0);
   if (ret_val == -1) {
      printf("Error while sending request to remote Supervisor\n");
   }

   if (running_modules[num_module].module_params != NULL) {
      ret_val = send(remote_supervisor_socketd, running_modules[num_module].module_params, strlen(running_modules[num_module].module_params)+1, 0);
      if (ret_val == -1) {
         printf("Error while sending request to remote Supervisor\n");
      }
   }
   ret_val = send(remote_supervisor_socketd, running_modules[num_module].module_name, strlen(running_modules[num_module].module_name)+1, 0);
   if (ret_val == -1) {
      printf("Error while sending request to remote Supervisor\n");
   }
   ret_val = send(remote_supervisor_socketd, running_modules[num_module].module_path, strlen(running_modules[num_module].module_path)+1, 0);
   if (ret_val == -1) {
      printf("Error while sending request to remote Supervisor\n");
   }

   for (x=0; x<running_modules[num_module].module_ifces_cnt; x++) {
      ret_val = send(remote_supervisor_socketd, running_modules[num_module].module_ifces[x].ifc_type, strlen(running_modules[num_module].module_ifces[x].ifc_type)+1, 0);
      if (ret_val == -1) {
         printf("Error while sending request to remote Supervisor\n");
      }
      ret_val = send(remote_supervisor_socketd, running_modules[num_module].module_ifces[x].ifc_params, strlen(running_modules[num_module].module_ifces[x].ifc_params)+1, 0);
      if (ret_val == -1) {
         printf("Error while sending request to remote Supervisor\n");
      }
      if (running_modules[num_module].module_ifces[x].ifc_direction != NULL) {
         ret_val = send(remote_supervisor_socketd, running_modules[num_module].module_ifces[x].ifc_direction, strlen(running_modules[num_module].module_ifces[x].ifc_direction)+1, 0);
         if (ret_val == -1) {
            printf("Error while sending request to remote Supervisor\n");
         }
      }
   }
}

void send_command_to_remote(int num_command, int num_module)
{
   remote_info_t command;
   int ret_val = 0;

   switch (num_command) {
   case 1:
      command.command_num = 1;
      command.size_of_recv = count_struct_size(num_module);
      ret_val = send(remote_supervisor_socketd, &command, sizeof(remote_info_t), 0);
      if (ret_val == -1) {
         printf("Error while sending request to remote Supervisor\n");
      }
      send_to_remote_running_module_struct(num_module);
      break;

   case 2:
      command.command_num = 2;
      command.size_of_recv = count_struct_size(num_module);
      ret_val = send(remote_supervisor_socketd, &command, sizeof(remote_info_t), 0);
      if (ret_val == -1) {
         printf("Error while sending request to remote Supervisor\n");
      }
      send_to_remote_running_module_struct(num_module);
      break;

   case 3:
      command.command_num = 3;
      command.size_of_recv = 0;
      ret_val = send(remote_supervisor_socketd, &command, sizeof(remote_info_t), 0);
      if (ret_val == -1) {
         printf("Error while sending request to remote Supervisor\n");
      }
      break;

   case 4:
      command.command_num = 4;
      command.size_of_recv = 0;
      ret_val = send(remote_supervisor_socketd, &command, sizeof(remote_info_t), 0);
      if (ret_val == -1) {
         printf("Error while sending request to remote Supervisor\n");
      }
      break;
   }
}

void check_cpu_usage()
{
   remote_info_t command;
   int x, y, ret_val;
   int tosend[1];
   int selected_module = -1;

   // condition 1, if (num_periods_overload>10)
   for (x=0;x<loaded_modules_cnt;x++) {
      if (running_modules[x].module_status) {
         if ((running_modules[x].percent_cpu_usage_kernel_mode > 50) ||
               (running_modules[x].percent_cpu_usage_user_mode > 50)) {
            running_modules[x].num_periods_overload++;
            VERBOSE(N_STDOUT,"module %d%s overload\n",x,running_modules[x].module_name);
         } else {
            running_modules[x].num_periods_overload--;
         }
      }
   }
   for (x=0;x<loaded_modules_cnt;x++) {
      if (running_modules[x].module_status) {
         /* TODO why 3? it should be defined somewhere as a constant... */
         if (running_modules[x].num_periods_overload > 3) {
            selected_module = x;
            break;
         }
      }
   }

   if ((selected_module >= 0) && (running_modules[selected_module].remote_module == FALSE) &&
         remote_supervisor_connected && running_modules[selected_module].module_status) {

      VERBOSE(N_STDOUT,"de se na to\nsup> %s, remote> %s", supervisor_address, remote_supervisor_address);

      // stop module
      stop_module(selected_module);
      running_modules[selected_module].remote_module = TRUE;

      //change IP adrresses of remote module input ifces and input ifces of modules connected to remote module
      graph_node_addresses_change(graph_first_node, selected_module, supervisor_address, remote_supervisor_address);

      //send to remote command to run new module and running_module struct
      send_command_to_remote(1, selected_module);

      sleep(1);
      //restart modules which are connected to remote_module
      graph_node_t * node_ptr = graph_first_node;
      running_module_t * module_data = NULL;
      while (node_ptr != NULL) {
         if (((running_module_t *) node_ptr->module_data)->module_number == selected_module) {
            break;
         }
         node_ptr = node_ptr->next_node;
      }
      for (x=0; x<node_ptr->num_node_output_interfaces; x++) {
         for (y=0; y<node_ptr->node_output_interfaces[x].node_children_counter; y++) {
            module_data = node_ptr->node_output_interfaces[x].node_children[y]->parent_node->module_data;
            if (module_data->remote_module) {
               send_command_to_remote(2, module_data->module_number);
            } else {
               if (module_data->module_enabled) {
                  stop_module(module_data->module_number);
                  module_data->module_enabled = TRUE;
               }
            }
         }
      }
      return;
   } else {
      // condition 2, if (CPU overload)
   }

}

void check_differences()
{
   int x,y,cnt;
   int str_len = 0;
   int select = -1;
   graph_node_t * node_ptr = graph_first_node;

   // while(node_ptr != NULL) {
   //    //if (...) {
   //    // select module
   //    //}
   //    node_ptr = node_ptr->next_node;
   // }
   // if (running_modules[select].module_cloned == FALSE && running_modules[select].module_status && running_modules[select].remote_module == FALSE) {
   if (FALSE) {
      running_modules[select].module_cloned = TRUE;
      if ((loaded_modules_cnt + running_modules[select].module_num_in_ifc + running_modules[select].module_num_out_ifc + 1) >= running_modules_array_size) {
         //realloc
      }

      /* TODO check pointers after allocation */
      if (running_modules[select].module_params != NULL) {
         str_len = strlen(running_modules[select].module_params);
         running_modules[loaded_modules_cnt].module_params = (char *) calloc(str_len+1, sizeof(char));
         strncpy(running_modules[loaded_modules_cnt].module_params, running_modules[select].module_params, str_len+1);
      }

      /* TODO check pointers after allocation */
      str_len = strlen(running_modules[select].module_path);
      running_modules[loaded_modules_cnt].module_path = (char *) calloc(str_len+1, sizeof(char));
      strncpy(running_modules[loaded_modules_cnt].module_path, running_modules[select].module_path, str_len+1);

      /* TODO check pointers after allocation */
      str_len = strlen(running_modules[select].module_name);
      running_modules[loaded_modules_cnt].module_name = (char *) calloc(str_len+1+5, sizeof(char));
      sprintf(running_modules[loaded_modules_cnt].module_name,"%sClone", running_modules[select].module_name);

      /* TODO check pointers after allocation */
      running_modules[loaded_modules_cnt].module_ifces = (interface_t *) calloc(running_modules[select].module_ifces_cnt, sizeof(interface_t));
      for (x=0; x<running_modules[select].module_ifces_cnt; x++) {
         if (running_modules[select].module_ifces[x].ifc_note != NULL) {
            /* TODO check pointers after allocation */
            str_len = strlen(running_modules[select].module_ifces[x].ifc_note);
            running_modules[loaded_modules_cnt].module_ifces[x].ifc_note = (char *) calloc(str_len+1, sizeof(char));
            strcpy(running_modules[loaded_modules_cnt].module_ifces[x].ifc_note, running_modules[select].module_ifces[x].ifc_note);
         }
         if (running_modules[select].module_ifces[x].ifc_type != NULL) {
            /* TODO check pointers after allocation */
            str_len = strlen(running_modules[select].module_ifces[x].ifc_type);
            running_modules[loaded_modules_cnt].module_ifces[x].ifc_type = (char *) calloc(str_len+1, sizeof(char));
            strcpy(running_modules[loaded_modules_cnt].module_ifces[x].ifc_type, running_modules[select].module_ifces[x].ifc_type);
         }
         if (running_modules[select].module_ifces[x].ifc_params != NULL) {
            /* TODO check pointers after allocation */
            str_len = strlen(running_modules[select].module_ifces[x].ifc_params);
            running_modules[loaded_modules_cnt].module_ifces[x].ifc_params = (char *) calloc(str_len+1, sizeof(char));
            strcpy(running_modules[loaded_modules_cnt].module_ifces[x].ifc_params, running_modules[select].module_ifces[x].ifc_params);
         }
         if (running_modules[select].module_ifces[x].ifc_direction != NULL) {
            /* TODO check pointers after allocation */
            str_len = strlen(running_modules[select].module_ifces[x].ifc_direction);
            running_modules[loaded_modules_cnt].module_ifces[x].ifc_direction = (char *) calloc(str_len+1, sizeof(char));
            strcpy(running_modules[loaded_modules_cnt].module_ifces[x].ifc_direction, running_modules[select].module_ifces[x].ifc_direction);
         }
      }
      running_modules[loaded_modules_cnt].module_ifces_cnt = running_modules[select].module_ifces_cnt;
      running_modules[loaded_modules_cnt].module_num_in_ifc = running_modules[select].module_num_in_ifc;
      running_modules[loaded_modules_cnt].module_num_out_ifc = running_modules[select].module_num_out_ifc;

      cnt = 0;
      for (x=0; x<running_modules[select].module_ifces_cnt; x++) {
         if (running_modules[select].module_ifces[x].ifc_direction != NULL && strcmp(running_modules[select].module_ifces[x].ifc_direction, "IN") == 0) {
            /* TODO check pointers after allocation */
            running_modules[loaded_modules_cnt+1+cnt].module_ifces = (interface_t *) calloc(4, sizeof(interface_t));

            if (running_modules[select].module_ifces[x].ifc_note != NULL) {
               str_len = strlen(running_modules[select].module_ifces[x].ifc_note);
               running_modules[loaded_modules_cnt+1+cnt].module_ifces[0].ifc_note = (char *) calloc(str_len+1, sizeof(char));
               strcpy(running_modules[loaded_modules_cnt+1+cnt].module_ifces[0].ifc_note, running_modules[select].module_ifces[x].ifc_note);
            }
            if (running_modules[select].module_ifces[x].ifc_type != NULL) {
               str_len = strlen(running_modules[select].module_ifces[x].ifc_type);
               /* TODO check pointers after allocation */
               running_modules[loaded_modules_cnt+1+cnt].module_ifces[0].ifc_type = (char *) calloc(str_len+1, sizeof(char));
               strcpy(running_modules[loaded_modules_cnt+1+cnt].module_ifces[0].ifc_type, running_modules[select].module_ifces[x].ifc_type);
            }
            if (running_modules[select].module_ifces[x].ifc_params != NULL) {
               /* TODO check pointers after allocation */
               str_len = strlen(running_modules[select].module_ifces[x].ifc_params);
               running_modules[loaded_modules_cnt+1+cnt].module_ifces[0].ifc_params = (char *)calloc(str_len+1, sizeof(char));
               strcpy(running_modules[loaded_modules_cnt+1+cnt].module_ifces[0].ifc_params, running_modules[select].module_ifces[x].ifc_params);
            }
            if (running_modules[select].module_ifces[x].ifc_direction != NULL) {
               /* TODO check pointers after allocation */
               str_len = strlen(running_modules[select].module_ifces[x].ifc_direction);
               running_modules[loaded_modules_cnt+1+cnt].module_ifces[0].ifc_direction = (char *)calloc(str_len+1, sizeof(char));
               strcpy(running_modules[loaded_modules_cnt+1+cnt].module_ifces[0].ifc_direction, running_modules[select].module_ifces[x].ifc_direction);
            }

            /* TODO why this size for alloc? */
            /* TODO check pointers after allocation */
            running_modules[loaded_modules_cnt+1+cnt].module_path = (char *) calloc(10, sizeof(char));
            sprintf(running_modules[loaded_modules_cnt+1+cnt].module_path,"Path");
            running_modules[loaded_modules_cnt+1+cnt].module_name = (char *) calloc(10, sizeof(char));
            sprintf(running_modules[loaded_modules_cnt+1+cnt].module_name,"Spliter");

            //first spliter output ifc
            /* TODO check pointers after allocation */
            running_modules[loaded_modules_cnt+1+cnt].module_ifces[1].ifc_type = (char *) calloc(10, sizeof(char));
            sprintf(running_modules[loaded_modules_cnt+1+cnt].module_ifces[1].ifc_type,"TCP");
            running_modules[loaded_modules_cnt+1+cnt].module_ifces[1].ifc_params = (char *) calloc(10, sizeof(char));
            sprintf(running_modules[loaded_modules_cnt+1+cnt].module_ifces[1].ifc_params,"%d,1", next_reserved_port);
            //update selected module input ifc params to connect to spliter
            free(running_modules[select].module_ifces[x].ifc_params);
            /* TODO check pointers after allocation */
            running_modules[select].module_ifces[x].ifc_params = (char *) calloc(20, sizeof(char));
            sprintf(running_modules[select].module_ifces[x].ifc_params,"localhost,%d",next_reserved_port);
            next_reserved_port++;
            running_modules[loaded_modules_cnt+1+cnt].module_ifces[1].ifc_direction = (char *) calloc(10, sizeof(char));
            sprintf(running_modules[loaded_modules_cnt+1+cnt].module_ifces[1].ifc_direction,"OUT");


            /* TODO check pointers after allocation */
            //second spliter output ifc
            running_modules[loaded_modules_cnt+1+cnt].module_ifces[2].ifc_type = (char *) calloc(10, sizeof(char));
            sprintf(running_modules[loaded_modules_cnt+1+cnt].module_ifces[2].ifc_type,"TCP");
            running_modules[loaded_modules_cnt+1+cnt].module_ifces[2].ifc_params = (char *) calloc(10, sizeof(char));
            sprintf(running_modules[loaded_modules_cnt+1+cnt].module_ifces[2].ifc_params,"%d,1", next_reserved_port);
            //update clone module input ifc params to connect to spliter
            free(running_modules[loaded_modules_cnt].module_ifces[x].ifc_params);
            running_modules[loaded_modules_cnt].module_ifces[x].ifc_params = (char *) calloc(20, sizeof(char));
            sprintf(running_modules[loaded_modules_cnt].module_ifces[x].ifc_params,"localhost,%d",next_reserved_port);
            next_reserved_port++;
            running_modules[loaded_modules_cnt+1+cnt].module_ifces[2].ifc_direction = (char *) calloc(10, sizeof(char));
            sprintf(running_modules[loaded_modules_cnt+1+cnt].module_ifces[2].ifc_direction,"OUT");

            running_modules[loaded_modules_cnt+1+cnt].module_ifces_cnt = 3;
            running_modules[loaded_modules_cnt+1+cnt].module_num_in_ifc = 1;
            running_modules[loaded_modules_cnt+1+cnt].module_num_out_ifc = 2;

            cnt++;
         }
      }

      update_module_input_ifces(graph_first_node, select);

      cnt = 0;
      for (x=0; x<running_modules[select].module_ifces_cnt; x++) {
         if (running_modules[select].module_ifces[x].ifc_direction != NULL && strcmp(running_modules[select].module_ifces[x].ifc_direction, "OUT") == 0) {
            running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces = (interface_t *)calloc(4, sizeof(interface_t));

            if (running_modules[select].module_ifces[x].ifc_note != NULL) {
               str_len = strlen(running_modules[select].module_ifces[x].ifc_note);
               running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[0].ifc_note = (char *)calloc(str_len+1, sizeof(char));
               strcpy(running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[0].ifc_note, running_modules[select].module_ifces[x].ifc_note);
            }
            if (running_modules[select].module_ifces[x].ifc_type != NULL) {
               str_len = strlen(running_modules[select].module_ifces[x].ifc_type);
               running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[0].ifc_type = (char *)calloc(str_len+1, sizeof(char));
               strcpy(running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[0].ifc_type, running_modules[select].module_ifces[x].ifc_type);
            }
            if (running_modules[select].module_ifces[x].ifc_params != NULL) {
               str_len = strlen(running_modules[select].module_ifces[x].ifc_params);
               running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[0].ifc_params = (char *)calloc(str_len+1, sizeof(char));
               strcpy(running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[0].ifc_params, running_modules[select].module_ifces[x].ifc_params);
            }
            if (running_modules[select].module_ifces[x].ifc_direction != NULL) {
               str_len = strlen(running_modules[select].module_ifces[x].ifc_direction);
               running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[0].ifc_direction = (char *)calloc(str_len+1, sizeof(char));
               strcpy(running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[0].ifc_direction, running_modules[select].module_ifces[x].ifc_direction);
            }

            running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_path = (char *) calloc (10, sizeof(char));
            sprintf(running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_path,"Path");
            running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_name = (char *) calloc (10, sizeof(char));
            sprintf(running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_name,"Merger");


            //first merger input ifc
            running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[1].ifc_type = (char *)calloc(10, sizeof(char));
            sprintf(running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[1].ifc_type,"TCP");
            running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[1].ifc_params = (char *)calloc(20, sizeof(char));
            sprintf(running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[1].ifc_params,"localhost,%d", next_reserved_port);
            //update selected module output ifc params
            free(running_modules[select].module_ifces[x].ifc_params);
            running_modules[select].module_ifces[x].ifc_params = (char *)calloc(20, sizeof(char));
            sprintf(running_modules[select].module_ifces[x].ifc_params,"%d,1",next_reserved_port);
            next_reserved_port++;
            /* TODO check pointer for NULL */
            running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[1].ifc_direction = (char *) calloc(10, sizeof(char));
            sprintf(running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[1].ifc_direction,"IN");


            /* TODO check pointer for NULL */
            //second merger input ifc
            running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[2].ifc_type = (char *)calloc(10, sizeof(char));
            sprintf(running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[2].ifc_type,"TCP");
            running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[2].ifc_params = (char *)calloc(20, sizeof(char));
            sprintf(running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[2].ifc_params,"localhost,%d", next_reserved_port);
            //update clone module output ifc params
            free(running_modules[loaded_modules_cnt].module_ifces[x].ifc_params);
            /* TODO set freed pointer to NULL */
            running_modules[loaded_modules_cnt].module_ifces[x].ifc_params = (char *)calloc(20, sizeof(char));
            sprintf(running_modules[loaded_modules_cnt].module_ifces[x].ifc_params,"%d,1",next_reserved_port);
            next_reserved_port++;
            running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[2].ifc_direction = (char *)calloc(10, sizeof(char));
            sprintf(running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces[2].ifc_direction,"IN");

            running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_ifces_cnt = 3;
            running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_num_in_ifc = 2;
            running_modules[loaded_modules_cnt+1+cnt+running_modules[select].module_num_in_ifc].module_num_out_ifc = 1;

            cnt++;
         }
      }

      update_module_output_ifces(graph_first_node, select);

      loaded_modules_cnt = loaded_modules_cnt + 1 + running_modules[select].module_num_in_ifc + running_modules[select].module_num_out_ifc;
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

   get_param_by_delimiter(running_modules[module].module_ifces[num_ifc].ifc_params, &dest_port, ',');
   VERBOSE(MODULE_EVENT,"-- Connecting to module %d_%s on port %s\n",module,running_modules[module].module_name, dest_port);

   memset(&addr, 0, sizeof(addr));

   addr.unix_addr.sun_family = AF_UNIX;
   snprintf(addr.unix_addr.sun_path, sizeof(addr.unix_addr.sun_path) - 1, UNIX_PATH_FILENAME_FORMAT, dest_port);
   sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (sockfd == -1) {
      VERBOSE(MODULE_EVENT,"Error while opening socket.\n");
      running_modules[module].module_service_ifc_isconnected = FALSE;
      free(dest_port);
      return;
   }
   if (connect(sockfd, (struct sockaddr *) &addr.unix_addr, sizeof(addr.unix_addr)) == -1) {
      VERBOSE(MODULE_EVENT,"Error while connecting to module %d_%s on port %s\n",module,running_modules[module].module_name, dest_port);
      running_modules[module].module_service_ifc_isconnected = FALSE;
      free(dest_port);
      close(sockfd);
      return;
   }
   running_modules[module].module_service_sd = sockfd;
   running_modules[module].module_service_ifc_isconnected = TRUE;
   free(dest_port);
}


void *service_thread_routine(void* arg)
{
   long int last_total_cpu_usage = 0;
   int sizeof_intptr = 4;
   int x,y;

   time_t rawtime;
   struct tm * timeinfo;

   while (service_thread_continue == TRUE) {
      pthread_mutex_lock(&running_modules_lock);
      time(&rawtime);
      timeinfo = localtime(&rawtime);

      usleep(100000);
      update_module_status();
      restart_modules();
      usleep(100000);

      for (y=0; y<loaded_modules_cnt; y++) {
         if (running_modules[y].module_served_by_service_thread == FALSE) {
            running_modules[y].module_number = y;
            for (x=0; x<running_modules[y].module_ifces_cnt; x++) {
               if ((strncmp(running_modules[y].module_ifces[x].ifc_type, "SERVICE", 7) == 0)) {
                  running_modules[y].module_has_service_ifc = TRUE;
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
                  if ((strncmp(running_modules[x].module_ifces[y].ifc_type, "SERVICE", 7) == 0)) {
                     break;
                  } else {
                     y++;
                  }
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
                  if (errno == ENOTCONN) {
                     running_modules[x].module_service_ifc_isconnected = FALSE;
                  }
               }
            }
         }
      }
      update_module_status();
      for (x=0;x<loaded_modules_cnt;x++) {
         if (running_modules[x].module_has_service_ifc == TRUE && running_modules[x].module_status == TRUE) {
            if (running_modules[x].module_service_ifc_isconnected == TRUE) {
               if (!(service_get_data(running_modules[x].module_service_sd, x))) {
                  //TODO
                  continue;
               }
            }
         }
      }
      update_cpu_usage(&last_total_cpu_usage);
      // check_cpu_usage();

      update_graph_values(graph_first_node);
      generate_periodic_picture();
      // compute_differences(graph_first_node);
      // check_differences();

      pthread_mutex_unlock(&running_modules_lock);
      // check_graph_values(graph_first_node);
      // print_statistics(graph_first_node);

      VERBOSE(STATISTICS,"------> %s", asctime(timeinfo));
      for (x=0;x<loaded_modules_cnt;x++) {
         if (running_modules[x].module_running) {
            VERBOSE(STATISTICS,"NAME:  %s, PID: %d, EN: %d, SIFC: %d, S: %d, ISC: %d | ",
                  running_modules[x].module_name,
                  running_modules[x].module_pid,
                  running_modules[x].module_enabled,
                  running_modules[x].module_has_service_ifc,
                  running_modules[x].module_status,
                  running_modules[x].module_service_ifc_isconnected);
            if (running_modules[x].module_has_service_ifc && running_modules[x].module_service_ifc_isconnected && running_modules[x].module_status) {
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
      sleep(2);
      
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
         ptr += sprintf(buffer + ptr, "%s,cpu,%d,%d\n", running_modules[x].module_name, running_modules[x].percent_cpu_usage_kernel_mode, running_modules[x].percent_cpu_usage_user_mode);
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
   // service_thread_id = (pthread_t) calloc (1,sizeof(pthread_t));
   pthread_create(&service_thread_id, NULL, service_thread_routine, NULL);

   // acceptor_thread_id = (pthread_t *) calloc (1,sizeof(pthread_t));
   //pthread_create(&acceptor_thread_id, NULL, remote_supervisor_accept_routine, NULL);
}

void interactive_run_temp_conf()
{
   pthread_mutex_lock(&running_modules_lock);
   int x = loaded_modules_cnt;
   /* TODO check pointer for NULL */
   char * buffer1 = (char *)calloc(1000, sizeof(char));
   char * buffer2 = (char *)calloc(1000, sizeof(char));
   char * ptr = buffer1;
   VERBOSE(N_STDOUT,"Paste in xml code with modules configuration starting with <modules> tag and ending with </modules> tag.\n");
   while(1) {
      if (!fscanf(input_fd,"%s",ptr)) {
         VERBOSE(N_STDOUT,"Wrong input.\n");
         continue;
      }
      if (strstr(ptr,"</modules>") != NULL) {
         break;
      }
      ptr += strlen(ptr);
      sprintf(ptr," ");
      ptr++;
   }
   sprintf(buffer2,"<?xml version=\"1.0\"?>\n<nemea-supervisor>\n%s\n</nemea-supervisor>",buffer1);
   free(buffer1);
   /* TODO set pointer to NULL */

   if (load_configuration(FALSE, buffer2) == FALSE) {
      VERBOSE(N_STDOUT,"Xml code was not parsed successfully.\n");
      free(buffer2);
      pthread_mutex_unlock(&running_modules_lock);
      return;
   }
   free(buffer2);
   // while (x<loaded_modules_cnt) {
   //    start_module(x);
   //    x++;
   // }
   pthread_mutex_unlock(&running_modules_lock);
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
         {"show-cpuusage",  no_argument, 0,  'C' },
         {"daemon-socket",  required_argument,  0, 's'},
         {0, 0, 0, 0}
      };

      c = getopt_long(*argc, argv, "df:hvCs:", long_options, &option_index);
      if (c == -1) {
         break;
      }

      switch (c) {
      case 'h':
         printf("Usage: supervisor [-d|--daemon] -f|--config-file=path [-h|--help] [-v|--verbose] [-C|--show-cpuusage] [-s|--daemon-socket=path]\n");
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
      case 'C':
         show_cpu_usage_flag = TRUE;
         break;
      case 'd':
         daemon_flag = TRUE;
         break;
      }
   }

   if (socket_path == NULL) {
      /* socket_path was not set by user, use default value. */
      socket_path = DAEMON_UNIX_PATH_FILENAME_FORMAT;
   }
   if (config_file == NULL) {
      fprintf(stderr, "Missing required config file (-f|--config-file).\n");
      return FALSE;
   }
   if (strstr(config_file, ".xml") == NULL) {
      free(config_file);
      config_file = NULL;
      fprintf(stderr, "File does not have expected .xml extension.\n");
      return FALSE;
   }
   return TRUE;
}

void print_help()
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
   // create socket
   union tcpip_socket_addr addr;
   struct addrinfo *p;
   memset(&addr, 0, sizeof(addr));
   addr.unix_addr.sun_family = AF_UNIX;
   snprintf(addr.unix_addr.sun_path, sizeof(addr.unix_addr.sun_path) - 1, socket_path);

   /* if socket file exists, it could be hard to create new socket and bind */
   unlink(DAEMON_UNIX_PATH_FILENAME_FORMAT); /* error when file does not exist is not a problem */
   *d_sd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (bind(*d_sd, (struct sockaddr *) &addr.unix_addr, sizeof(addr.unix_addr)) != -1) {
      p = (struct addrinfo *) &addr.unix_addr;
      chmod(DAEMON_UNIX_PATH_FILENAME_FORMAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
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
   if (listen(*d_sd, 1) == -1) {
      //perror("listen");
      VERBOSE(N_STDOUT,"Listen failed");
      return 10;
   }

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
      }
      fclose(statistics_fd);
      fclose(module_event_fd);
      VERBOSE(N_STDOUT,"process_id of child process %d \n", process_id);
      exit(0);
   }

   umask(0);
   sid = setsid();
   if (sid < 0) {
      // Return failure
      exit(1);
   }
   // chdir("./modules_logs/");

   int fd_fd = open ("supervisor_log", O_RDWR | O_CREAT | O_APPEND, PERM_LOGFILE);
   dup2(fd_fd, 1);
   dup2(fd_fd, 2);
   close(fd_fd);

   return 0;
}


int daemon_get_client(int * d_sd)
{
   struct sockaddr_storage remoteaddr; // client address
   socklen_t addrlen;
   int newclient;

   // handle new connection
   addrlen = sizeof remoteaddr;
   newclient = accept(*d_sd, (struct sockaddr *)&remoteaddr, &addrlen);
   if (newclient == -1) {
      VERBOSE(N_STDOUT,"Accepting new client failed.");
      return newclient;
   }
   VERBOSE(N_STDOUT,"Client has connected.\n");
   return newclient;
}


void daemon_mode(int * arg)
{
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
         return;
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

         tv.tv_sec = 1;
         tv.tv_usec = 0;

         ret_val = select(client_stream_input_fd+1, &read_fds, NULL, NULL, &tv);

         if (ret_val == -1) {
            perror("select()");
            fclose(client_stream_input);
            fclose(client_stream_output);
            close(client_sd);
            connected = FALSE;
            input_fd = stdin;
            output_fd = stdout;
            break;
         } else if (ret_val != 0) {
            if (FD_ISSET(client_stream_input_fd, &read_fds)) {
               ioctl(client_stream_input_fd, FIONREAD, &x);
               if (x == 0) {
                  input_fd = stdin;
                  output_fd = stdout;
                  VERBOSE(N_STDOUT, "Client has disconnected.\n");
                  connected = FALSE;
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
                  interactive_run_temp_conf();
                  break;
               case DAEMON_STOP_CODE:
                  supervisor_termination();
                  connected = FALSE;
                  terminated = TRUE;
                  break;
               case DAEMON_CONFIG_MODE_CODE:
                  //client config mode
                  break;
               case DAEMON_STATS_MODE_CODE: {
                  //client stats mode
                  char * stats_buffer = make_formated_statistics();
                  int buffer_len = strlen(stats_buffer);
                  char stats_buffer2[buffer_len+1];
                  memset(stats_buffer2,0,buffer_len+1);
                  strncpy(stats_buffer2, stats_buffer, buffer_len+1);
                  VERBOSE(N_STDOUT,"STATS:\n%s", stats_buffer2);
                  free(stats_buffer);

                  input_fd = stdin;
                  output_fd = stdout;
                  VERBOSE(N_STDOUT, "Client has disconnected.\n");
                  connected = FALSE;
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
               VERBOSE(N_STDOUT,"8. LOAD CONFIGURATION\n");
               VERBOSE(N_STDOUT,"-- Type \"Cquit\" to exit client --\n");
               VERBOSE(N_STDOUT,"-- Type \"Dstop\" to stop daemon --\n");

            }
         } else {
            /* TODO nothing here? -> add comment */
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


void *remote_supervisor_accept_routine()
{
   remote_supervisor_connected = FALSE;
   char buffer[10];
   int ret_val = 0;
   struct sockaddr_in supervisor;
   struct sockaddr_in supervisor_remote;
   int supervisor_sd;
   int supervisor_remote_sd;
   int port = 11011;
   socklen_t addrlen;

   if ((supervisor_sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
      VERBOSE(N_STDOUT,"Remote thread> Cannot create a socket.\n");
      pthread_exit(NULL);
   }

   supervisor.sin_family = AF_INET;
   supervisor.sin_port = htons(port);
   supervisor.sin_addr.s_addr = INADDR_ANY;

   if (bind(supervisor_sd, (struct sockaddr *)&supervisor, sizeof(supervisor)) == -1) {
      VERBOSE(N_STDOUT, "Remote thread> Cannot bind the socket.\n");
      close(supervisor_sd);
      pthread_exit(NULL);
   }

   if (listen(supervisor_sd, 2) == -1) {
      VERBOSE(N_STDOUT, "Remote thread> Error listening.\n");
      close(supervisor_sd);
      pthread_exit(NULL);
   }


   while (service_thread_continue == TRUE) {
      if (remote_supervisor_connected == FALSE) {
         addrlen = sizeof(supervisor_remote);
         supervisor_remote_sd = accept(supervisor_sd, (struct sockaddr*)&supervisor_remote, &addrlen);
         if (supervisor_remote_sd == -1) {
            VERBOSE(N_STDOUT, "Remote thread> Error accept.\n");
            close(supervisor_sd);
            pthread_exit(NULL);
         }
         strcpy(remote_supervisor_address, inet_ntoa((struct in_addr)supervisor_remote.sin_addr));
         remote_supervisor_socketd = supervisor_remote_sd;
         remote_supervisor_connected = TRUE;
         VERBOSE(N_STDOUT, "Remote thread> Got connection: %s", remote_supervisor_address);
      }
      ret_val = recv(supervisor_remote_sd, buffer, 1, MSG_DONTWAIT);
      if (ret_val == -1) {
         if (errno == EAGAIN) {
            sleep(1);
            continue;
         }
         VERBOSE(N_STDOUT, "Remote thread> Error recv.\n");
      } else if (ret_val == 0) {
         VERBOSE(N_STDOUT, "Remote thread> Remote client has disconnected.\n");
         close(supervisor_remote_sd);
         remote_supervisor_connected = FALSE;
      }
   }

   close(remote_supervisor_socketd);
   close(supervisor_sd);
   pthread_exit(NULL);
}

