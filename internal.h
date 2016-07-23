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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define TRUE   1 ///< Bool true
#define FALSE   0 ///< Bool false

#define STATISTICS   1
#define MODULE_EVENT   2
#define N_STDOUT   3
#define DEBUG   4
#define SUP_LOG   5

#define RELOAD_INIT_LOAD_CONFIG   1
#define RELOAD_CALLBACK_ROOT_ELEM   2
#define RELOAD_DEFAULT_CONFIG_FILE   4

#define CLIENT_CONFIG_MODE_CODE   789123
#define CLIENT_STATS_MODE_CODE   456987
#define CLIENT_INFO_MODE_CODE   113366
#define CLIENT_RELOAD_MODE_CODE   115599

#define FORMAT_MENU   "\x1b[36m"
#define FORMAT_RESET   "\x1b[0m"
#define FORMAT_BOLD   "\x1b[1m"
#define FORMAT_WARNING   "\x1b[38;5;196m"
#define FORMAT_INTERACTIVE   "\x1b[38;5;214;1m"
#define FORMAT_RUNNING   "\x1b[38;5;40;1m"
#define FORMAT_STOPPED   "\x1b[38;5;196;1m"

#define DEFAULT_SIZE_OF_BUFFER   100

/* Log files are being shown with the following pager */
#define PAGER "less"
#define SUP_TMP_DIR "/tmp/sup_tmp_dir"
/* Tmp file used for additional communication between daemon and client (e.g. to keep log file name which is client gonna show) */
#define SUP_CLI_TMP_FILE "/tmp/sup_tmp_dir/sup_cli_tmp_file"

#define MODULES_EVENTS_FILE_NAME   "modules_events"
#define MODULES_STATS_FILE_NAME   "modules_statistics"
#define SUPERVISOR_LOG_FILE_NAME   "supervisor_log"
#define SUPERVISOR_DEBUG_LOG_FILE_NAME   "supervisor_debug_log"

/**
 * Macro for NULL pointer testing, freeing and setting pointer to NULL
 */
#define NULLP_TEST_AND_FREE(pointer) do { \
   if (pointer != NULL) { \
      free(pointer); \
      pointer = NULL; \
   } \
} while (0);

extern FILE *input_fd;
extern FILE *output_fd;

extern FILE *supervisor_debug_log_fd;
extern FILE *supervisor_log_fd;
extern FILE *statistics_fd;
extern FILE *module_event_fd;

extern char verbose_msg[4096];

void print_msg(int level, char *string);
char *get_input_from_stream(FILE *stream);
void show_file_with_pager(char **file_path);

#define VERBOSE(level, format, args...) if (1) { \
   snprintf(verbose_msg, 4095, format, ##args); \
   print_msg(level, verbose_msg); \
}

#endif

