/**
 * \file internal.h
 * \brief VERBOSE macro header.
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

#ifndef INTERNAL_H
#define INTERNAL_H

#include <stdio.h>
#include <stdlib.h>

#define TRUE                        1 ///< Bool true
#define FALSE                       0 ///< Bool false

#define STATISTICS      		1
#define MODULE_EVENT    	2
#define N_STDOUT     		3

#define RELOAD_INIT_LOAD_CONFIG 		1
#define RELOAD_CALLBACK_ROOT_ELEM 	2
#define RELOAD_INTERACTIVE			3
#define RELOAD_DEFAULT_CONFIG_FILE	4

#define DAEMON_STOP_CODE 			951753
#define DAEMON_CONFIG_MODE_CODE 	789123
#define DAEMON_STATS_MODE_CODE 		456987
#define DAEMON_RELOAD_MODE_CODE 	115599

extern char * statistics_file_path;
extern char * module_event_file_path;
extern char * supervisor_log_file_path;
extern char * graph_picture_file_path;
extern char * graph_code_file_path;

extern FILE * input_fd;
extern FILE * output_fd;

extern FILE * supervisor_log_fd;
extern FILE * statistics_fd;
extern FILE * module_event_fd;

extern char verbose_msg[4096];

void print_msg(int level, char *string);

#define VERBOSE(level, format, args...) if (1) { \
   snprintf(verbose_msg, 4095, format, ##args); \
   print_msg(level, verbose_msg); \
}

#endif

