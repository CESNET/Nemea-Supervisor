/**
 * \file graph.h
 * \brief Graph structures and methods.
 * \author Marek Svepes <svepemar@fit.cvut.cz>
 * \date 2013
 * \date 2014
 */
/*
 * Copyright (C) 2013,2014 CESNET
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

#ifndef _GRAPH_H_
#define _GRAPH_H_

#include "supervisor.h"

#define GRAPH_SOURCE_FILE              "./graph_code" ///< Name of file with generated code for dot program.
#define DEFAULT_NUM_CLIENTS_OUTPUT_IFC    10 ///< Default number of clients of output interface.


/***********STRUCTURES***********/

typedef struct edge_statistics_s edge_statistics_t;
typedef struct graph_node_s graph_node_t;
typedef struct graph_node_input_interface_s graph_node_input_interface_t;
typedef struct graph_node_output_interface_s graph_node_output_interface_t;

/** Structure representing one input interface of module. */
struct graph_node_input_interface_s {
   interface_t *              ifc_struct; ///< Pointer to interface_t structure of running_module_t struct.
   graph_node_t *                parent_node; ///< Pointer to parent node.
   int                     message_counter; ///< Received messages counter of this node input interface.
   int                     node_interface_port; ///< Port of this node interface.
   graph_node_output_interface_t * node_interface_output_ifc; ///< Pointer to connected output interface of another module.
};

/** Structure representing one output interface of module. */
struct graph_node_output_interface_s {
   interface_t            *ifc_struct; ///< Pointer to interface_t structure of running_module_t struct.
   graph_node_t           *parent_node; ///< Pointer to parent node.
   int                     message_counter; ///< Sent messages counter of this node output interface.
   int                     node_children_counter; ///< Node children counter.
   graph_node_input_interface_t **node_children; ///< Array of connected node input interfaces of another nodes (node children).
   int                     node_interface_port; ///< Port of this node interface.
   edge_statistics_t      *statistics; ///< Pointer to array of edge_statistics_t structures.
};

/** Structure representing one module in graph structure. */
struct graph_node_s {
   void                          *module_data; ///< Pointer to running_module_t structure of module represented by this node.
   int                            module_number; ///< Index to running_modules array.
   graph_node_t *                 next_node; ///< Pointer to next node.
   graph_node_input_interface_t   *node_input_interfaces; ///< Array of node input interfaces structures.
   graph_node_output_interface_t  *node_output_interfaces; ///< Array of node output interfaces structures.
   int                     num_node_input_interfaces; ///< Number of node input interfaces structures.
   int                     num_node_output_interfaces; ///< Number of node output interface structures.
};

/** Structure used for message calculations of every output interface. */
struct edge_statistics_s {
   int                     last_period_counters_difference; ///< Difference between sent and received messages of node output interface connected with another node input interface.
   int                     num_periods; ///< Number of checked periods.
   int                     expected_value; ///< Expected value of messages loss rate of node output interface connected with another node input interface.
   int                     period_differences_suma; ///< Suma of all period differences between sent and received messages of node output interface connected with another node input interface.
};



/***********FUNCTIONS***********/

/** Function creates new graph node and links its input and output interfaces with other nodes.
 * @param[in] first Pointer to first node in the list of nodes.
 * @param[in] last Pointer to last node in the list of nodes.
 * @param[in] data Pointer to running_module_t struct of added module.
 * @return New node, NULL if error.
 */
graph_node_t * add_graph_node (graph_node_t * first, graph_node_t * last, void * data);

/** Function updates message counters of all interfaces of every node in graph structure.
 * @param[in] first Pointer to first node in the list of nodes.
 */
void update_graph_values (graph_node_t * first);

/** Function generates code for program dot to GRAPH_SOURCE_FILE.
 * @param[in] first Pointer to first node in the list of nodes.
 */
void generate_graph_code(graph_node_t * first);

/** Function executes dot and display program and connects them with pipe.
 * @param[in]
 */
void show_graph();

/** Graph node memory free.
 * @param[in] first Pointer to first node in the list of nodes.
 */
void free_graph_node(graph_node_t * node);

/** Whole graph structure memory free.
 * @param[in] first Pointer to first node in the list of nodes.
 */
void destroy_graph(graph_node_t * first);

/** Function changes IP addresses of connected intput interfaces to selected module and input interfaces of selected module.
 * @param[in] first Pointer to first node in the list of nodes.
 * @param[in] module_num Module nuber.
 * @param[in] local_addr String with local IP address.
 * @param[in] remote_addr String with remote_supervisor IP address.
 */
void graph_node_addresses_change (graph_node_t * first, int module_num, const char * local_addr, const char * remote_addr);

/** Function updates selected module input interfaces in graph structure.
 * @param[in] first Pointer to first node in the list of nodes.
* @param[in] selected_mod Selected module.
 */
void update_module_input_ifces (graph_node_t * first, int selected_mod);

/** Function updates selected module output interfaces in graph structure.
 * @param[in] first Pointer to first node in the list of nodes.
* @param[in] selected_mod Selected module.
 */
void update_module_output_ifces (graph_node_t * first, int selected_mod);

/** Function computes new message loss rate statistics for each output interface of every module.
 * @param[in] first Pointer to first node in the list of nodes.
 */
void compute_differences(graph_node_t * first);

/** Function prints to stdout node output interfaces message loss rate statistics.
 * @param[in] first Pointer to first node in the list of nodes.
 */
void print_statistics(graph_node_t * first);

/** Function checks in graph structure if loaded modules dont have same output interface port.
 * @param[in] first Pointer to first node in the list of nodes.
 */
void check_port_duplicates(graph_node_t * first);

#endif

