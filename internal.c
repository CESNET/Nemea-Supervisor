/**
 * \file internal.c
 * \brief VERBOSE macro implementation.
 * \author Marek Svepes <svepemar@fit.cvut.cz>
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


#include "internal.h"
#include <stdio.h>
#include <string.h>
#include <stdio_ext.h> // because of __fpurge(FILE * stream)

FILE * input_fd = NULL;
FILE * output_fd = NULL;

FILE * supervisor_log_fd = NULL;
FILE * statistics_fd = NULL;
FILE * module_event_fd = NULL;

char verbose_msg[4096];

void print_msg(int level, char *string)
{
   switch (level) {
   case STATISTICS:
      if (statistics_fd != NULL) {
         fprintf(statistics_fd, "%s", string);
         fflush(statistics_fd);
      }
      break;

   case MODULE_EVENT:
      if (module_event_fd != NULL) {
         fprintf(module_event_fd, "%s", string);
         fflush(module_event_fd);
      }
      break;

   case N_STDOUT:
      if (output_fd != NULL) {
         fprintf(output_fd, "%s", string);
         fflush(output_fd);
      }
      break;
   }
}

char * get_input_from_stream (FILE * stream)
{
   char * buffer = NULL,  * ret_val = NULL;
   int buffer_len = 0;

   buffer = (char *) calloc (DEFAULT_SIZE_OF_BUFFER, sizeof(char));
   ret_val = fgets(buffer, DEFAULT_SIZE_OF_BUFFER, stream);
   if (ret_val == NULL) {
      VERBOSE(N_STDOUT, ANSI_RED_BOLD "[WARNING] There is no input!\n" ANSI_ATTR_RESET);
      free(buffer);
      buffer = NULL;
      return NULL;
   }

   buffer_len = strlen(buffer);
   if (buffer_len >= 99) {
      VERBOSE(N_STDOUT, ANSI_RED_BOLD "[WARNING] Too long and wrong input!\n" ANSI_ATTR_RESET);
      free(buffer);
      buffer = NULL;
      __fpurge(stream);
      return NULL;
   } else {
      if (buffer[buffer_len-1] == '\n') {
         buffer[buffer_len-1] = '\0';
      }
      return buffer;
   }
}
