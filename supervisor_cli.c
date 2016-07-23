/**
 * \file supervisor_cli.c
 * \brief Command line interface for supervisor
 * \author Marek Svepes <svepemar@fit.cvut.cz>
 * \author Tomas Cejka <cejkat@cesnet.cz>
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

#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#define DEFAULT_DAEMON_SERVER_SOCKET   DEFAULT_PATH_TO_SOCKET  ///<  Daemon server socket

typedef struct client_internals_s {
   FILE *supervisor_input_stream;
   FILE *supervisor_output_stream;
   int supervisor_input_stream_fd;
   int supervisor_sd;
   int connected;
} client_internals_t;

/***** GLOBAL VARIABLES *****/

client_internals_t *client_internals = NULL;

/***** ************************** *****/

union tcpip_socket_addr {
   struct addrinfo tcpip_addr; ///< used for TCPIP socket
   struct sockaddr_un unix_addr; ///< used for path of UNIX socket
};

void free_client_internals_variables()
{
   client_internals->connected = FALSE;
   if (client_internals->supervisor_input_stream_fd > 0) {
      close(client_internals->supervisor_input_stream_fd);
      client_internals->supervisor_input_stream_fd = 0;
   }
   if (client_internals->supervisor_input_stream != NULL) {
      fclose(client_internals->supervisor_input_stream);
      client_internals->supervisor_input_stream = NULL;
   }
   if (client_internals->supervisor_output_stream != NULL) {
      fclose(client_internals->supervisor_output_stream);
      client_internals->supervisor_output_stream = NULL;
   }
   if (client_internals->supervisor_sd > 0) {
      close(client_internals->supervisor_sd);
      client_internals->supervisor_sd = 0;
   }
   free(client_internals);
   client_internals = NULL;
}

int connect_to_supervisor(char *socket_path)
{
   union tcpip_socket_addr addr;
   memset(&addr, 0, sizeof(addr));

   addr.unix_addr.sun_family = AF_UNIX;
   snprintf(addr.unix_addr.sun_path, sizeof(addr.unix_addr.sun_path) - 1, "%s", socket_path);

   client_internals->supervisor_sd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (client_internals->supervisor_sd < 0) {
      fprintf(stderr, FORMAT_WARNING "[ERROR] Could not create socket!" FORMAT_RESET "\n");
      return EXIT_FAILURE;
   }

   if (connect(client_internals->supervisor_sd, (struct sockaddr *) &addr.unix_addr, sizeof(addr.unix_addr)) == -1) {
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
   uint64_t timeouts = 0;
   client_internals = (client_internals_t *) calloc (1, sizeof(client_internals_t));
   if (client_internals == NULL) {
      exit(EXIT_FAILURE);
   }
   int bytes_to_read = 0; // value can be also -1 <=> ioctl error
   int x = 0, ret_val = 0;
   char *buffer = NULL;
   char *socket_path = NULL;
   int modules_stats_flag = FALSE;
   int modules_info_flag = FALSE;
   int reload_command_flag = FALSE;
   int flag_cnt = 0;

   FILE *tmp_file = NULL;
   char *file_path = NULL;
   int file_path_len = 0;

   int opt;
   while ((opt = getopt(argc, argv, "rhs:xi")) != -1) {
      switch (opt) {
      case 'h':
         printf("Usage:  supervisor_cli  [OPTIONAL]...\n"
                  "   OPTIONAL parameters:\n"
                  "      [-h]   Prints this help.\n"
                  "      [-s <path>]   Path of the unix socket which is used for supervisor daemon and client communication.\n"
                  "      [-x]   Receives and prints statistics about modules and terminates.\n"
                  "      [-r]   Sends a command to supervisor to reload the configuration.\n"
                  "      [-i]   Receives and prints information about modules in JSON and terminates.\n");
         exit(EXIT_SUCCESS);

      case 's':
         socket_path = optarg;
         break;

      case 'x':
         modules_stats_flag = TRUE;
         flag_cnt++;
         break;

      case 'i':
         modules_info_flag = TRUE;
         flag_cnt++;
         break;

      case 'r':
         reload_command_flag = TRUE;
         flag_cnt++;
         break;

      default:
         fprintf(stderr, "[ERROR] Invalid program arguments! (try to run it with \"-h\" argument)\n");
         exit(EXIT_FAILURE);
      }
   }

   if (flag_cnt > 1) {
      fprintf(stderr, "[ERROR] Cannot run client with more than one parameter {x, r, i} at the same time!\n");
      free_client_internals_variables();
      exit(EXIT_FAILURE);
   }

   if (socket_path == NULL) {
      /* socket_path was not set by user, use default value. */
      socket_path = DEFAULT_DAEMON_SERVER_SOCKET;
   }

   if (connect_to_supervisor(socket_path) == EXIT_FAILURE) {
      fprintf(stderr, FORMAT_WARNING "[ERROR] Could not connect to supervisor!" FORMAT_RESET "\n");
      free_client_internals_variables();
      exit(EXIT_FAILURE);
   } else {
      client_internals->connected = TRUE;
   }

   client_internals->supervisor_input_stream = fdopen(client_internals->supervisor_sd, "r");
   if (client_internals->supervisor_input_stream == NULL) {
      fprintf(stderr, FORMAT_WARNING "[ERROR] Fdopen: could not open supervisor input stream!" FORMAT_RESET "\n");
      free_client_internals_variables();
      exit(EXIT_FAILURE);
   }

   client_internals->supervisor_output_stream = fdopen(client_internals->supervisor_sd, "w");
   if (client_internals->supervisor_output_stream == NULL) {
      fprintf(stderr, FORMAT_WARNING "[ERROR] Fdopen: could not open supervisor output stream!" FORMAT_RESET "\n");
      free_client_internals_variables();
      exit(EXIT_FAILURE);
   }

   client_internals->supervisor_input_stream_fd = fileno(client_internals->supervisor_input_stream);
   if (client_internals->supervisor_input_stream_fd < 0) {
      fprintf(stderr, FORMAT_WARNING "[ERROR] Fileno: could not get supervisor input stream descriptor!" FORMAT_RESET "\n");
      free_client_internals_variables();
      exit(EXIT_FAILURE);
   }

   fd_set read_fds;
   struct timeval tv;

   if (modules_stats_flag == TRUE) {
      fprintf(client_internals->supervisor_output_stream,"%d\n", CLIENT_STATS_MODE_CODE);
   } else if (modules_info_flag == TRUE) {
      fprintf(client_internals->supervisor_output_stream,"%d\n", CLIENT_INFO_MODE_CODE);
   } else if (reload_command_flag == TRUE) {
      fprintf(client_internals->supervisor_output_stream,"%d\n", CLIENT_RELOAD_MODE_CODE);
      fflush(client_internals->supervisor_output_stream);
      free_client_internals_variables();
      exit(EXIT_SUCCESS);
   } else {
      fprintf(client_internals->supervisor_output_stream,"%d\n", CLIENT_CONFIG_MODE_CODE);
   }
   fflush(client_internals->supervisor_output_stream);


   while (client_internals->connected) {
      FD_ZERO(&read_fds);
      FD_SET(client_internals->supervisor_input_stream_fd, &read_fds);
      FD_SET(0, &read_fds);

      tv.tv_sec = 0;
      tv.tv_usec = 200000;

      ret_val = select(client_internals->supervisor_input_stream_fd+1, &read_fds, NULL, NULL, &tv);
      if (ret_val == -1) {
         fprintf(stderr, "[ERROR] Select: error!\n");
         free_client_internals_variables();
         exit(EXIT_FAILURE);
      } else if (ret_val > 0) {
         if (FD_ISSET(0, &read_fds)) {
            buffer = get_input_from_stream(stdin);
            if (buffer != NULL) {
               if ((strcmp(buffer,"Cquit") == 0)) {
                  free_client_internals_variables();
                  free(buffer);
                  exit(EXIT_SUCCESS);
               } else if (strcmp(buffer,"Dstop") == 0) {
                  for (x=0; x<3; x++) {
                     fprintf(client_internals->supervisor_output_stream,"0\n");
                     fflush(client_internals->supervisor_output_stream);
                     usleep(300000);
                  }
               } else {
                  fprintf(client_internals->supervisor_output_stream,"%s\n",buffer);
                  fflush(client_internals->supervisor_output_stream);
               }
               free(buffer);
            }
         }
         if (FD_ISSET(client_internals->supervisor_input_stream_fd, &read_fds)) {
            usleep(200000);
            ioctl(client_internals->supervisor_input_stream_fd, FIONREAD, &bytes_to_read);
            if (bytes_to_read == 0 || bytes_to_read == -1) {
               if (modules_stats_flag != TRUE && modules_info_flag != TRUE) {
                  fprintf(stderr, FORMAT_WARNING "[WARNING] Supervisor has disconnected, I'm done!" FORMAT_RESET "\n");
                  fflush(stderr);
               }
               free_client_internals_variables();
               exit(EXIT_SUCCESS);
            } else {
               for (x=0; x<bytes_to_read; x++) {
                  printf("%c", (char) fgetc(client_internals->supervisor_input_stream));
               }
               fflush(stdout);
            }
         }
      } else {
         timeouts++;
         // Check whether the tmp_file is available (it should contain path to the log file which has to be shown)
         if (access(SUP_CLI_TMP_FILE, R_OK) == 0) {
            tmp_file = fopen(SUP_CLI_TMP_FILE, "r");
            if (tmp_file != NULL) {
               if (fscanf(tmp_file, "%d\n", &file_path_len) > 0 && file_path_len > 0) {
                  file_path = (char *) calloc(file_path_len + 1, sizeof(char));
                  if (fscanf(tmp_file, "%s", file_path) > 0) {
                     unlink(SUP_CLI_TMP_FILE);
                     show_file_with_pager(&file_path);
                  }
               }
               fclose(tmp_file);
               NULLP_TEST_AND_FREE(file_path)
            }
         }
      }

      if ((modules_stats_flag == TRUE || modules_info_flag == TRUE) && timeouts > 3) {
         free_client_internals_variables();
         exit(EXIT_SUCCESS);
      }
   }

   return 0;
}
