/**
 * \file graph.c
 * \brief Graph methods implementation.
 * \author Marek Svepes <svepemar@fit.cvut.cz>
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

#include "config.h"
#include "graph.h"
#include "internal.h"

graph_node_t * add_graph_node (graph_node_t * first, graph_node_t * last, void * data)
{
   graph_node_t * node_ptr = NULL;
   int x = 0, y = 0, z = 0;
   char buffer[100];
   memset(buffer,0,100);
   int num_clients = 0;

   running_module_t * running_module = (running_module_t *) data;
   graph_node_t * new_node = (graph_node_t *) calloc(1, sizeof(graph_node_t));

   new_node->module_data = data;
   new_node->module_number = running_module->module_number;
   new_node->num_node_input_interfaces = running_module->module_num_in_ifc;
   new_node->num_node_output_interfaces = running_module->module_num_out_ifc;
   new_node->node_input_interfaces = (graph_node_input_interface_t *) calloc(running_module->module_num_in_ifc, sizeof(graph_node_input_interface_t));
   new_node->node_output_interfaces = (graph_node_output_interface_t *) calloc(running_module->module_num_out_ifc, sizeof(graph_node_output_interface_t));

   for (x=0; x<running_module->module_ifces_cnt; x++) {
      if (running_module->module_ifces[x].ifc_direction != NULL && running_module->module_ifces[x].ifc_params != NULL && running_module->module_ifces[x].ifc_type != NULL) {
         memset(buffer,0,100);
         if (!strcmp(running_module->module_ifces[x].ifc_direction, "IN")) {
            if (strcmp(running_module->module_ifces[x].ifc_type, "TCP") == 0) {
               sscanf(running_module->module_ifces[x].ifc_params,"%*[^','],%d", &new_node->node_input_interfaces[y].node_interface_port);
            } else if (strcmp(running_module->module_ifces[x].ifc_type, "UNIXSOCKET") == 0) {
               sscanf(running_module->module_ifces[x].ifc_params,"%*[^','],%s", buffer);
               new_node->node_input_interfaces[y].unix_sock_interface_port = (char *) calloc (strlen(buffer)+1, sizeof(char));
               strcpy(new_node->node_input_interfaces[y].unix_sock_interface_port, buffer);
            }
            new_node->node_input_interfaces[y].parent_node = new_node;
            new_node->node_input_interfaces[y].ifc_struct = &running_module->module_ifces[x];         
            y++;
         } else if (!strcmp(running_module->module_ifces[x].ifc_direction, "OUT")) {
            if (sscanf(running_module->module_ifces[x].ifc_params,"%[^','],%d", buffer, &num_clients) != 2) {
               num_clients = DEFAULT_NUM_CLIENTS_OUTPUT_IFC;
            }
            if (strcmp(running_module->module_ifces[x].ifc_type, "TCP") == 0) {
               new_node->node_output_interfaces[z].node_interface_port = atoi(buffer);
            } else if (strcmp(running_module->module_ifces[x].ifc_type, "UNIXSOCKET") == 0) {
               new_node->node_output_interfaces[z].unix_sock_interface_port = (char *) calloc (strlen(buffer)+1, sizeof(char));
               strcpy(new_node->node_output_interfaces[z].unix_sock_interface_port, buffer);
            }
            new_node->node_output_interfaces[z].node_children = (graph_node_input_interface_t **)calloc(num_clients, sizeof(graph_node_input_interface_t *));
            new_node->node_output_interfaces[z].parent_node = new_node;
            new_node->node_output_interfaces[z].ifc_struct = &running_module->module_ifces[x];
            z++;
         }
      }
   }

   for (x=0; x<running_module->module_num_in_ifc; x++) {
      node_ptr = first;
      while (node_ptr != NULL) {
         for (y=0; y<node_ptr->num_node_output_interfaces; y++) {
            if (strcmp(new_node->node_input_interfaces[x].ifc_struct->ifc_type, "TCP") == 0 && strcmp(node_ptr->node_output_interfaces[y].ifc_struct->ifc_type, "TCP") == 0) {
               if (new_node->node_input_interfaces[x].node_interface_port == node_ptr->node_output_interfaces[y].node_interface_port) {
                  node_ptr->node_output_interfaces[y].node_children[node_ptr->node_output_interfaces[y].node_children_counter] = &(new_node->node_input_interfaces[x]);
                  node_ptr->node_output_interfaces[y].node_children_counter++;
                  new_node->node_input_interfaces[x].node_interface_output_ifc = &node_ptr->node_output_interfaces[y];
               }
            } else if (strcmp(new_node->node_input_interfaces[x].ifc_struct->ifc_type, "UNIXSOCKET") == 0 && strcmp(node_ptr->node_output_interfaces[y].ifc_struct->ifc_type, "UNIXSOCKET") == 0) {
               if (strcmp(new_node->node_input_interfaces[x].unix_sock_interface_port, node_ptr->node_output_interfaces[y].unix_sock_interface_port) == 0) {
                  node_ptr->node_output_interfaces[y].node_children[node_ptr->node_output_interfaces[y].node_children_counter] = &(new_node->node_input_interfaces[x]);
                  node_ptr->node_output_interfaces[y].node_children_counter++;
                  new_node->node_input_interfaces[x].node_interface_output_ifc = &node_ptr->node_output_interfaces[y];
               }
            }
         }
         node_ptr = node_ptr->next_node;
      }
   }

   for (x=0; x<running_module->module_num_out_ifc; x++) {
      node_ptr = first;
      while (node_ptr != NULL) {
         for (y=0; y<node_ptr->num_node_input_interfaces; y++) {
            if (strcmp(new_node->node_output_interfaces[x].ifc_struct->ifc_type, "TCP") == 0 && strcmp(node_ptr->node_input_interfaces[y].ifc_struct->ifc_type, "TCP") == 0) {
               if (new_node->node_output_interfaces[x].node_interface_port == node_ptr->node_input_interfaces[y].node_interface_port) {
                  new_node->node_output_interfaces[x].node_children[new_node->node_output_interfaces[x].node_children_counter] = &(node_ptr->node_input_interfaces[y]);
                  new_node->node_output_interfaces[x].node_children_counter++;
                  node_ptr->node_input_interfaces[y].node_interface_output_ifc = &new_node->node_output_interfaces[x];
               }
            } else if (strcmp(new_node->node_output_interfaces[x].ifc_struct->ifc_type, "UNIXSOCKET") == 0 && strcmp(node_ptr->node_input_interfaces[y].ifc_struct->ifc_type, "UNIXSOCKET") == 0) {
               if (strcmp(new_node->node_output_interfaces[x].unix_sock_interface_port, node_ptr->node_input_interfaces[y].unix_sock_interface_port) == 0) {
                  new_node->node_output_interfaces[x].node_children[new_node->node_output_interfaces[x].node_children_counter] = &(node_ptr->node_input_interfaces[y]);
                  new_node->node_output_interfaces[x].node_children_counter++;
                  node_ptr->node_input_interfaces[y].node_interface_output_ifc = &new_node->node_output_interfaces[x];
               }
            }
         }
         node_ptr = node_ptr->next_node;
      }
   }

   return new_node;
}

void update_graph_values(graph_node_t * first)
{
   if (first == NULL) {
      return;
   }

   int x;
   graph_node_t * node_ptr = first;
   running_module_t * running_module = NULL;

   while (node_ptr != NULL) {
      running_module = (running_module_t *) node_ptr->module_data;
      if (running_module->module_status == TRUE && running_module->module_has_service_ifc == TRUE) {
         for (x=0; x<node_ptr->num_node_input_interfaces; x++) {
            node_ptr->node_input_interfaces[x].message_counter = running_module->module_counters_array[x];
         }

         for (x=0; x<node_ptr->num_node_output_interfaces; x++) {
            node_ptr->node_output_interfaces[x].message_counter = running_module->module_counters_array[x + node_ptr->num_node_input_interfaces];
         }
      }

      node_ptr = node_ptr->next_node;
   }
}

void generate_graph_code(graph_node_t * first)
{
   if (first == NULL) {
      return;
   }

   running_module_t * running_module = NULL;
   int x,y;
   graph_node_t * node_ptr = first;
   FILE * fd = fopen(graph_code_file_path, "w");

   if (fd == NULL) {
      VERBOSE(N_STDOUT, "Could not open graph source file !\n");
      return;
   }

   fprintf(fd,"digraph G {\n");

   while (node_ptr != NULL) {
      running_module = (running_module_t *) node_ptr->module_data;
      fprintf(fd, "\tsubgraph cluster%s%d {\n", running_module->module_name, running_module->module_number);
      if (running_module->module_status) {
         fprintf(fd, "\t\tlabel=\"%s%d\\nrunning\";\n\t\tstyle=filled;\n\t\tcolor=grey;\n\t\tnode[style=filled,color=white];\n", running_module->module_name, running_module->module_number);
      } else {
         fprintf(fd, "\t\tlabel=\"%s%d\\nstopped\";\n\t\tstyle=filled;\n\t\tcolor=grey;\n\t\tnode[style=filled,color=white];\n", running_module->module_name, running_module->module_number);
      }

      for (x=0; x<node_ptr->num_node_input_interfaces;x++) {
         if (strcmp(node_ptr->node_input_interfaces[x].ifc_struct->ifc_type, "TCP") == 0) {
            fprintf(fd, "\t\tIN%d%d\n", node_ptr->node_input_interfaces[x].node_interface_port, running_module->module_number);
         } else if (strcmp(node_ptr->node_input_interfaces[x].ifc_struct->ifc_type, "UNIXSOCKET") == 0) {
            fprintf(fd, "\t\tIN%s%d\n", node_ptr->node_input_interfaces[x].unix_sock_interface_port, running_module->module_number);
         }
      }
      for (x=0; x<node_ptr->num_node_output_interfaces;x++) {
         if (strcmp(node_ptr->node_output_interfaces[x].ifc_struct->ifc_type, "TCP") == 0) {
            fprintf(fd, "\t\tOUT%d%d\n", node_ptr->node_output_interfaces[x].node_interface_port, running_module->module_number);
         } else if (strcmp(node_ptr->node_output_interfaces[x].ifc_struct->ifc_type, "UNIXSOCKET") == 0) {
            fprintf(fd, "\t\tOUT%s%d\n", node_ptr->node_output_interfaces[x].unix_sock_interface_port, running_module->module_number);
         }
      }
      fprintf(fd, "\t}\n");
      node_ptr = node_ptr->next_node;
   }
   node_ptr = first;
   while (node_ptr != NULL) {

      if (node_ptr->num_node_output_interfaces == 0) {
         node_ptr = node_ptr->next_node;
         continue;
      }

      for (x=0; x<node_ptr->num_node_output_interfaces; x++) {
         for (y=0; y<node_ptr->node_output_interfaces[x].node_children_counter; y++) {
            if (strcmp(node_ptr->node_output_interfaces[x].ifc_struct->ifc_type, "TCP") == 0) {
               fprintf(fd, "\tOUT%d%d->IN%d%d[color=red, label=\"send:%d\\nrecv:%d\\n\"];\n",
                     node_ptr->node_output_interfaces[x].node_interface_port,
                     ((running_module_t *) node_ptr->module_data)->module_number,
                     node_ptr->node_output_interfaces[x].node_children[y]->node_interface_port,
                     ((running_module_t *) node_ptr->node_output_interfaces[x].node_children[y]->parent_node->module_data)->module_number,
                     node_ptr->node_output_interfaces[x].message_counter,
                     node_ptr->node_output_interfaces[x].node_children[y]->message_counter);
            } else if (strcmp(node_ptr->node_output_interfaces[x].ifc_struct->ifc_type, "UNIXSOCKET") == 0) {
               fprintf(fd, "\tOUT%s%d->IN%s%d[color=red, label=\"send:%d\\nrecv:%d\\n\"];\n",
                     node_ptr->node_output_interfaces[x].unix_sock_interface_port,
                     ((running_module_t *) node_ptr->module_data)->module_number,
                     node_ptr->node_output_interfaces[x].node_children[y]->unix_sock_interface_port,
                     ((running_module_t *) node_ptr->node_output_interfaces[x].node_children[y]->parent_node->module_data)->module_number,
                     node_ptr->node_output_interfaces[x].message_counter,
                     node_ptr->node_output_interfaces[x].node_children[y]->message_counter);
            }
         }
      }

      node_ptr = node_ptr->next_node;
   }

   fprintf(fd, "}\n");
   fclose(fd);
}

void generate_picture()
{
#if HAVE_DOT == 1
   int pid_dot, status = 0;

   if ((pid_dot = fork()) == 0) {
      //child dot
      execl("/usr/bin/dot","dot","-Tpng", graph_code_file_path,"-o",graph_picture_file_path,NULL);
      printf("error while executing dot\n");
      exit(1);
   } else if (pid_dot == -1) {
      //error dot
      printf("error while forking dot\n");
      return;
   } else {
      waitpid(pid_dot, &status, WUNTRACED);
   }
#endif
}

void show_picture()
{
#if (HAVE_DOT == 1 && HAVE_DISPLAY == 1)
   int result = 0, status = 0;
   int pid_dot, pid_display;
   int pipe2[2];

   if (pipe(pipe2) == -1) {
      fprintf(stderr,"Pipe error.\n");
      return;
   }

   if ((pid_dot = fork()) == 0) {
      //child dot
      dup2(pipe2[1],1);
      close(pipe2[0]);
      execl("/usr/bin/dot","dot","-Tpng", graph_code_file_path,NULL);
      printf("error while executing dot\n");
      exit(1);
   } else if (pid_dot == -1) {
      //error dot
      printf("error while forking dot\n");
      return;
   }

   if ((pid_display = fork()) == 0) {
      //child display
      dup2(pipe2[0],0);
      close(pipe2[1]);
      execl("/usr/bin/display","display","-",NULL);
      printf("error while executing display\n");
      exit(1);
   } else if (pid_display == -1) {
      //error display
      printf("error while forking display\n");
      waitpid(pid_dot, &status, WUNTRACED);
      return;
   } else {
      //parent display
      close(pipe2[0]);
      close(pipe2[1]);
      waitpid(pid_dot, &status, WUNTRACED);
      // waitpid(pid_display, &status, WUNTRACED);
   }
#else
   VERBOSE(N_STDOUT, "[WARNING] Dot or display is not installed!\n");
#endif
}

void free_graph_node(graph_node_t * node)
{
   if (node->next_node != NULL) {
      free_graph_node(node->next_node);
      if (node != NULL) {
         free(node);
         node = NULL;
      }
   } else {
      if (node != NULL) {
         free(node);
         node = NULL;
      }
   }
}

void destroy_graph(graph_node_t * first)
{
   if (first==NULL) {
      return;
   }

   int x;
   graph_node_t * node_ptr = first;

   while (node_ptr != NULL) {
      for (x=0; x<node_ptr->num_node_input_interfaces; x++) {
         if (node_ptr->node_input_interfaces[x].unix_sock_interface_port != NULL) {
            free(node_ptr->node_input_interfaces[x].unix_sock_interface_port);
            node_ptr->node_input_interfaces[x].unix_sock_interface_port = NULL;
         }
      }
      if (node_ptr->node_input_interfaces != NULL) {
         free(node_ptr->node_input_interfaces);
         node_ptr->node_input_interfaces = NULL;
      }
      for (x=0; x<node_ptr->num_node_output_interfaces; x++) {
         if (node_ptr->node_output_interfaces[x].node_children != NULL) {
            free(node_ptr->node_output_interfaces[x].node_children);
            node_ptr->node_output_interfaces[x].node_children = NULL;
         }
         if (node_ptr->node_output_interfaces[x].unix_sock_interface_port != NULL) {
            free(node_ptr->node_output_interfaces[x].unix_sock_interface_port);
            node_ptr->node_output_interfaces[x].unix_sock_interface_port = NULL;
         }
      }
      if (node_ptr->node_output_interfaces != NULL) {
         free(node_ptr->node_output_interfaces);
         node_ptr->node_output_interfaces = NULL;
      }
      node_ptr = node_ptr->next_node;
   }

   free_graph_node(first);
}

void check_port_duplicates(graph_node_t * first)
{
   int x, y;
   graph_node_t * node_ptr = first;
   graph_node_t * iter_node_ptr = NULL;

   while (node_ptr != NULL) {
      for (x=0; x<node_ptr->num_node_output_interfaces; x++) {
         iter_node_ptr = node_ptr;
         y = x+1;
         while (iter_node_ptr != NULL) {
            while (y<iter_node_ptr->num_node_output_interfaces) {
               graph_node_output_interface_t *node_x = &node_ptr->node_output_interfaces[x];
               graph_node_output_interface_t *node_y = &iter_node_ptr->node_output_interfaces[y];
               if (node_x->ifc_struct->ifc_type != NULL && node_y->ifc_struct->ifc_type != NULL) {
                  if ((strcmp(node_x->ifc_struct->ifc_type, node_y->ifc_struct->ifc_type) == 0)) {
                     if (strcmp(node_x->ifc_struct->ifc_type, "TCP") == 0) {
                        if (node_x->node_interface_port == node_y->node_interface_port) {
                           VERBOSE(N_STDOUT,"Modules %d%s and %d%s have TCP output interface with same port %d.\n",
                                 node_ptr->module_number,
                                 ((running_module_t *) node_ptr->module_data)->module_name,
                                 iter_node_ptr->module_number,
                                 ((running_module_t *) iter_node_ptr->module_data)->module_name,
                                 node_x->node_interface_port)
                        }
                     } else if (strcmp(node_x->ifc_struct->ifc_type, "UNIXSOCKET") == 0) {
                        if (strcmp(node_x->unix_sock_interface_port, node_y->unix_sock_interface_port) == 0) {
                           VERBOSE(N_STDOUT,"Modules %d%s and %d%s have UNIXSOCKET output interface with same socket %s.\n",
                                 node_ptr->module_number,
                                 ((running_module_t *) node_ptr->module_data)->module_name,
                                 iter_node_ptr->module_number,
                                 ((running_module_t *) iter_node_ptr->module_data)->module_name,
                                 node_x->unix_sock_interface_port)
                        }
                     }
                  }
               }
               y++;
            }
            y=0;
            iter_node_ptr = iter_node_ptr->next_node;
         }
      }

      node_ptr = node_ptr->next_node;
   }
}

