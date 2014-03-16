/**
 * \file graph.c
 * \brief Graph methods implementation.
 * \author Marek Svepes <svepemar@fit.cvut.cz>
 * \date 2014
 */
/*
 * Copyright (C) 2013 CESNET
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


#include "graph.h"
#include "supervisor.h"


graph_node_t * add_graph_node (graph_node_t * first, graph_node_t * last, void * data)
{
	graph_node_t * node_ptr = NULL;
	int x = 0, y = 0;
	char port[50];
	memset(port,0,50);
	int num_clients = 0;

	running_module_t * running_module = (running_module_t *) data;
	graph_node_t * new_node = (graph_node_t *) calloc (1, sizeof(graph_node_t));

	new_node->module_data = data;
	new_node->num_node_input_interfaces = running_module->module_num_in_ifc;
	new_node->num_node_output_interfaces = running_module->module_num_out_ifc;
	new_node->node_input_interfaces = (graph_node_input_interface_t *)calloc(running_module->module_num_in_ifc, sizeof(graph_node_input_interface_t));
	new_node->node_output_interfaces = (graph_node_output_interface_t *)calloc(running_module->module_num_out_ifc, sizeof(graph_node_output_interface_t));

	y=0;
	for(x=0; x<running_module->module_ifces_cnt; x++){
		if (!strcmp(running_module->module_ifces[x].ifc_direction, "IN")) {
			sscanf(running_module->module_ifces[x].ifc_params,"%*[^','],%d", &new_node->node_input_interfaces[y].node_interface_port);
			new_node->node_input_interfaces[y].parent_node = new_node;
			y++;
		}
	}
 
	y=0;
	for(x=0; x<running_module->module_ifces_cnt; x++){
		if (!strcmp(running_module->module_ifces[x].ifc_direction, "OUT")) {
			if(sscanf(running_module->module_ifces[x].ifc_params,"%[^','],%d", port, &num_clients) != 2) {
				num_clients = DEFAULT_NUM_CLIENTS_OUTPUT_IFC;
			}
			new_node->node_output_interfaces[y].node_interface_port = atoi(port);
			new_node->node_output_interfaces[y].node_children = (graph_node_input_interface_t **)calloc(num_clients, sizeof(graph_node_input_interface_t *));
			y++;
		}
	}


	for(x=0; x<running_module->module_num_in_ifc; x++){
		node_ptr = first;
		while(node_ptr != NULL){
			for(y=0; y<node_ptr->num_node_output_interfaces; y++){
				if(new_node->node_input_interfaces[x].node_interface_port == node_ptr->node_output_interfaces[y].node_interface_port){
					node_ptr->node_output_interfaces[y].node_children[node_ptr->node_output_interfaces[y].node_children_counter] = &(new_node->node_input_interfaces[x]);
					node_ptr->node_output_interfaces[y].node_children_counter++;
				}
			}
			node_ptr = node_ptr->next_node;
		}
	}

	for(x=0; x<running_module->module_num_out_ifc; x++){
		node_ptr = first;
		while(node_ptr != NULL){
			for(y=0; y<node_ptr->num_node_input_interfaces; y++){
				if(new_node->node_output_interfaces[x].node_interface_port == node_ptr->node_input_interfaces[y].node_interface_port){
					new_node->node_output_interfaces[x].node_children[new_node->node_output_interfaces[x].node_children_counter] = &(node_ptr->node_input_interfaces[y]);
					new_node->node_output_interfaces[x].node_children_counter++;
				}
			}
			node_ptr = node_ptr->next_node;
		}
	}

	return new_node;
}

void update_graph_values (graph_node_t * first)
{
	if(first == NULL) {
		return;
	}

	int x;
	graph_node_t * node_ptr = first;
	running_module_t * running_module = NULL; 

	while(node_ptr != NULL){
		running_module = (running_module_t *) node_ptr->module_data;

		for(x=0; x<node_ptr->num_node_input_interfaces; x++){
			node_ptr->node_input_interfaces[x].message_counter = running_module->module_counters_array[x];
		}

		for(x=0; x<node_ptr->num_node_output_interfaces; x++){
			node_ptr->node_output_interfaces[x].message_counter = running_module->module_counters_array[x + node_ptr->num_node_input_interfaces];
		}

		node_ptr = node_ptr->next_node;
	}
}

void check_graph_values (graph_node_t * first)
{
	if(first == NULL) {
		return;
	}

	int x,y;
	graph_node_t * node_ptr = first;

	while(node_ptr != NULL){
		if(node_ptr->num_node_output_interfaces == 0){
			node_ptr = node_ptr->next_node;
			continue;
		}

		printf(" - node:  ");
		for(x=0; x<node_ptr->num_node_output_interfaces; x++){
			for(y=0; y<node_ptr->node_output_interfaces[x].node_children_counter; y++){
				printf("%s(%d)%d -> %s(%d)%d    ", ((running_module_t *) node_ptr->module_data)->module_name,
												 node_ptr->node_output_interfaces[x].node_interface_port,
												 node_ptr->node_output_interfaces[x].message_counter,
												 ((running_module_t *)  node_ptr->node_output_interfaces[x].node_children[y]->parent_node->module_data)->module_name,
												 node_ptr->node_output_interfaces[x].node_children[y]->node_interface_port,
												 node_ptr->node_output_interfaces[x].node_children[y]->message_counter);
			}
			printf("\n");
		}

		node_ptr = node_ptr->next_node;
	}

}

void print_graph(graph_node_t * first)
{
	if(first == NULL) {
		return;
	}

	int x,y;
	graph_node_t * node_ptr = first;

	while(node_ptr != NULL){
		printf("\tnode: input interfaces:  ");
		for(x=0; x<node_ptr->num_node_input_interfaces; x++){
			printf("%d, ", node_ptr->node_input_interfaces[x].node_interface_port);
		}
		printf("\n\t\toutput interfaces:  ");
		for(x=0; x<node_ptr->num_node_output_interfaces; x++){
			printf("%d, ", node_ptr->node_output_interfaces[x].node_interface_port);
			for(y=0;y<node_ptr->node_output_interfaces[x].node_children_counter;y++){
				printf("%d->%d  ", node_ptr->node_output_interfaces[x].node_interface_port, node_ptr->node_output_interfaces[x].node_children[y]->node_interface_port);
			}
			printf(" | ");
		}
		printf("\n");
		node_ptr = node_ptr->next_node;
	}
}

void generate_graph_code(graph_node_t * first)
{
	if(first == NULL) {
		return;
	}
	
	int x,y;
	graph_node_t * node_ptr = first;
	FILE * fd = fopen(GRAPH_SOURCE_FILE, "w");

	fprintf(fd,"digraph G {\n");

	while(node_ptr != NULL){
		fprintf(fd, "\tsubgraph cluster%s%d {\n", ((running_module_t *) node_ptr->module_data)->module_name, ((running_module_t *) node_ptr->module_data)->module_number);
		fprintf(fd, "\t\tlabel=\"%s%d\";\n\t\tstyle=filled;\n\t\tcolor=grey;\n\t\tnode[style=filled,color=white];\n", ((running_module_t *) node_ptr->module_data)->module_name, ((running_module_t *) node_ptr->module_data)->module_number);
		for(x=0; x<node_ptr->num_node_input_interfaces;x++){
			fprintf(fd, "\t\tINPUTIFC%d%d\n", node_ptr->node_input_interfaces[x].node_interface_port, ((running_module_t *) node_ptr->module_data)->module_number);
		}
		for(x=0; x<node_ptr->num_node_output_interfaces;x++){
			fprintf(fd, "\t\tOUTPUTIFC%d%d\n", node_ptr->node_output_interfaces[x].node_interface_port, ((running_module_t *) node_ptr->module_data)->module_number);
		}
		fprintf(fd, "\t}\n");
		node_ptr = node_ptr->next_node;
	}
	node_ptr = first;
	while(node_ptr != NULL){
		
		if(node_ptr->num_node_output_interfaces == 0){
			node_ptr = node_ptr->next_node;
			continue;
		}

		for(x=0; x<node_ptr->num_node_output_interfaces; x++){
			for(y=0; y<node_ptr->node_output_interfaces[x].node_children_counter; y++){
				fprintf(fd, "\tOUTPUTIFC%d%d->INPUTIFC%d%d[color=red, label=\"send:%d\\nrecv:%d\"];\n", node_ptr->node_output_interfaces[x].node_interface_port, ((running_module_t *) node_ptr->module_data)->module_number,
															node_ptr->node_output_interfaces[x].node_children[y]->node_interface_port, ((running_module_t *) node_ptr->node_output_interfaces[x].node_children[y]->parent_node->module_data)->module_number,
															node_ptr->node_output_interfaces[x].message_counter, node_ptr->node_output_interfaces[x].node_children[y]->message_counter);
			}
			printf("\n");
		}


		node_ptr = node_ptr->next_node;
	}

	fprintf(fd, "}\n");
	fclose(fd);
}

void show_graph()
{
	int pid_dot, pid_display;
	int pipe2[2];
	if(pipe(pipe2) == -1){
		fprintf(stderr,"Pipe error.\n");
		return;
	}

	if((pid_dot = fork()) == 0){
		//child dot
		dup2(pipe2[1],1);
		close(pipe2[0]);
		execl("/usr/bin/dot","dot","-Tpng", GRAPH_SOURCE_FILE,NULL);
		printf("error while executing dot\n");
		exit(1);
	} else if (pid_dot == -1) {
		//error dot
		printf("error while forking dot\n");
		exit(1);
	}

	if((pid_display = fork()) == 0){
		//child display
		dup2(pipe2[0],0);
		close(pipe2[1]);
		execl("/usr/bin/display","display","-",NULL);
		printf("error while executing display\n");
		exit(1);
	} else if (pid_display == -1) {
		//error display
		printf("error while forking display\n");
		exit(1);
	} else {
		//parent display
		close(pipe2[0]);
		close(pipe2[1]);
	}
}

void free_graph_node(graph_node_t * node)
{
	if(node->next_node != NULL){
		free_graph_node(node->next_node);
		if(node != NULL) {
			free(node);
		}
	} else {
		if(node != NULL) {
			free(node);
		}
	}
}

void destroy_graph(graph_node_t * first)
{
	if(first==NULL){
		return;
	}
	
	int x;
	graph_node_t * node_ptr = first;

	while(node_ptr != NULL){
		if(node_ptr->node_input_interfaces != NULL) {
			free(node_ptr->node_input_interfaces);
		}
		for(x=0;x<node_ptr->num_node_output_interfaces;x++){
			if(node_ptr->node_output_interfaces[x].node_children != NULL) {
				free(node_ptr->node_output_interfaces[x].node_children);
			}
		}
		if(node_ptr->node_output_interfaces != NULL) {
			free(node_ptr->node_output_interfaces);
		}
		node_ptr = node_ptr->next_node;
	}

	free_graph_node(first);
}