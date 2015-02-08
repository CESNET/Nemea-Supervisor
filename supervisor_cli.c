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

#define TRUE            1 ///< Bool true
#define FALSE           0 ///< Bool false
#define DAEMON_UNIX_PATH_FILENAME_FORMAT  "/tmp/supervisor_daemon.sock"

typedef struct client_internals_s {
   FILE *   supervisor_input_stream;
   FILE *   supervisor_output_stream;
   int         supervisor_input_stream_fd;
   int         supervisor_sd;
   int         connected;
} client_internals_t;

/***** GLOBAL VARIABLES *****/

client_internals_t * client_internals = NULL;

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
      fprintf(stderr, "[ERROR] Could not create socket!\n");
      return EXIT_FAILURE;
   }

   if (connect(client_internals->supervisor_sd, (struct sockaddr *) &addr.unix_addr, sizeof(addr.unix_addr)) == -1) {
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
   client_internals = (client_internals_t *) calloc (1, sizeof(client_internals_t));
   if (client_internals == NULL) {
      exit(EXIT_FAILURE);
   }
   int bytes_to_read = 0; // value can be also -1 <=> ioctl error
   int x = 0, ret_val = 0;
   char * buffer = NULL;
   char *socket_path = NULL;
   int just_stats_flag = FALSE;
   int reload_command_flag = FALSE;

   int opt;
   while ((opt = getopt(argc, argv, "rhs:x")) != -1) {
      switch (opt) {
      case 'h':
         printf("Usage: supervisor_cli [-h] [-s <path>]\n"
               "\t-h\tshows this help\n\t-s <path>\tpath to UNIX socket file\n");
         exit(EXIT_SUCCESS);

      case 's':
         socket_path = optarg;
         break;

      case 'x':
         just_stats_flag = TRUE;
         break;

      case 'r':
         reload_command_flag = TRUE;
         break;

      default:
         fprintf(stderr, "[ERROR] Invalid program arguments! (try to run it with \"-h\" argument)\n");
         exit(EXIT_FAILURE);
      }
   }

   if (reload_command_flag && just_stats_flag) {
      fprintf(stderr, "[ERROR] Cannot run client with \"-x\" and with \"-r\" arguments at the same time!\n");
      free_client_internals_variables();
      exit(EXIT_FAILURE);
   }

   if (socket_path == NULL) {
      /* socket_path was not set by user, use default value. */
      socket_path = DAEMON_UNIX_PATH_FILENAME_FORMAT;
   }

   if(connect_to_supervisor(socket_path) == EXIT_FAILURE){
      fprintf(stderr, "[ERROR] Could not connect to supervisor!\n");
      free_client_internals_variables();
      exit(EXIT_FAILURE);
   } else {
      client_internals->connected = TRUE;
   }

   client_internals->supervisor_input_stream = fdopen(client_internals->supervisor_sd, "r");
   if(client_internals->supervisor_input_stream == NULL) {
      fprintf(stderr, "[ERROR] Fdopen: could not open supervisor input stream!\n");
      free_client_internals_variables();
      exit(EXIT_FAILURE);
   }

   client_internals->supervisor_output_stream = fdopen(client_internals->supervisor_sd, "w");
   if(client_internals->supervisor_output_stream == NULL) {
      fprintf(stderr, "[ERROR] Fdopen: could not open supervisor output stream!\n");
      free_client_internals_variables();
      exit(EXIT_FAILURE);
   }

   client_internals->supervisor_input_stream_fd = fileno(client_internals->supervisor_input_stream);
   if(client_internals->supervisor_input_stream_fd < 0) {
      fprintf(stderr, "[ERROR] Fileno: could not get supervisor input stream descriptor!\n");
      free_client_internals_variables();
      exit(EXIT_FAILURE);
   }

   fd_set read_fds;
   struct timeval tv;

   if(just_stats_flag) {
      fprintf(client_internals->supervisor_output_stream,"%d\n", DAEMON_STATS_MODE_CODE);
   } else if (reload_command_flag) {
      fprintf(client_internals->supervisor_output_stream,"%d\n", DAEMON_RELOAD_MODE_CODE);
      fflush(client_internals->supervisor_output_stream);
      free_client_internals_variables();
      exit(EXIT_SUCCESS);
   } else {
      fprintf(client_internals->supervisor_output_stream,"%d\n", DAEMON_CONFIG_MODE_CODE);
   }
   fflush(client_internals->supervisor_output_stream);


   while (client_internals->connected) {
      FD_ZERO(&read_fds);
      FD_SET(client_internals->supervisor_input_stream_fd, &read_fds);
      FD_SET(0, &read_fds);

      tv.tv_sec = 1;
      tv.tv_usec = 0;

      ret_val = select(client_internals->supervisor_input_stream_fd+1, &read_fds, NULL, NULL, &tv);
      if (ret_val == -1) {
         fprintf(stderr, "[ERROR] Select: error!\n");
         free_client_internals_variables();
         exit(EXIT_FAILURE);
      } else if (ret_val) {
         if (FD_ISSET(0, &read_fds)) {
            buffer = get_input_from_stream(stdin);
            if (buffer == NULL) {
               continue;
            }
            if ((strcmp(buffer,"Cquit") == 0)) {
               free_client_internals_variables();
               free(buffer);
               exit(EXIT_SUCCESS);
            } else if (strcmp(buffer,"Dstop") == 0) {
               for (x=0; x<3; x++) {
                  fprintf(client_internals->supervisor_output_stream,"9\n");
                  fflush(client_internals->supervisor_output_stream);
                  usleep(300000);
               }
            } else {
               fprintf(client_internals->supervisor_output_stream,"%s\n",buffer);
               fflush(client_internals->supervisor_output_stream);
            }
            free(buffer);
         }
         if (FD_ISSET(client_internals->supervisor_input_stream_fd, &read_fds)) {
            usleep(200000);
            ioctl(client_internals->supervisor_input_stream_fd, FIONREAD, &bytes_to_read);
            if (bytes_to_read == 0 || bytes_to_read == -1) {
               fprintf(stderr, ANSI_RED_BOLD "[WARNING] Supervisor has disconnected, I'm done!\n" ANSI_ATTR_RESET);
               fflush(stderr);
               free_client_internals_variables();
               exit(EXIT_SUCCESS);
            } else {
               for(x=0; x<bytes_to_read; x++) {
                  printf("%c", (char) fgetc(client_internals->supervisor_input_stream));
               }
               fflush(stdout);
            }
         }
      } else {
         // timeout, nothing to do
      }

      if (just_stats_flag) {
         free_client_internals_variables();
         exit(EXIT_SUCCESS);
      }
   }

   return 0;
}
