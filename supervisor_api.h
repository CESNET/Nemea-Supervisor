/**
 * \file supervisor_api.h
 * \brief Supervisor public functions.
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

#ifndef SUPERVISOR_API_H
#define SUPERVISOR_API_H

#define INTERACTIVE_MODE_CODE   1
#define DAEMON_MODE_CODE   2

#include <libxml/tree.h>
#include <inttypes.h>

int init_paths();
int parse_prog_args(int *argc, char **argv);
int supervisor_initialization();
void supervisor_termination(const uint8_t stop_all_modules, const uint8_t generate_backup);

/* Interactive mode functions */
void interactive_mode();
int interactive_get_option();
void interactive_start_configuration();
void interactive_stop_configuration();
void interactive_set_enabled();
void interactive_set_disabled();
void interactive_restart_module();
void interactive_print_brief_status();
void interactive_print_detailed_status();
void interactive_print_loaded_configuration();
int reload_configuration(const int, xmlNodePtr *);
void interactive_print_supervisor_info();
void interactive_browse_log_files();

/* Daemon mode functions */
int daemon_mode_initialization();
void daemon_mode_server_routine();

/* Netconf mode functions */
#ifdef nemea_plugin
int netconf_supervisor_initialization(xmlNodePtr *running);
xmlDocPtr netconf_get_state_data();
#endif

#endif

