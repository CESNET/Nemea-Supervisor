/**
 * \file graph.h
 * \brief Graph structures and methods.
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

#define GRAPH_SOURCE_FILE 					"./graph_source_file"
#define DEFAULT_NUM_CLIENTS_OUTPUT_IFC		10

typedef struct graph_node_s graph_node_t;


typedef struct graph_node_input_interface_s {
	graph_node_t * parent_node;
	int message_counter;
	int node_interface_port;
} graph_node_input_interface_t;

typedef struct graph_node_output_interface_s {
	int message_counter;
	int node_children_counter;
	graph_node_input_interface_t ** node_children;
	int node_interface_port;
} graph_node_output_interface_t;

struct graph_node_s {
	void * module_data;
	int module_number;
	graph_node_t * next_node;
	graph_node_input_interface_t * node_input_interfaces;
	graph_node_output_interface_t * node_output_interfaces;
	int num_node_input_interfaces;
	int num_node_output_interfaces;
};

/********************************************/

graph_node_t * add_graph_node (graph_node_t * first, graph_node_t * last, void * data);
void update_graph_values (graph_node_t * first);
void check_graph_values (graph_node_t * first);
void print_graph(graph_node_t * first);
void generate_graph_code(graph_node_t * first);
void show_graph();
void destroy_graph(graph_node_t * first);