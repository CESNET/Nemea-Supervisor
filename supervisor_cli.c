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
#include <stdio.h>
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

#define TRUE            1 ///< Bool true
#define FALSE           0 ///< Bool false
#define DAEMON_UNIX_PATH_FILENAME_FORMAT  "/tmp/supervisor_daemon.sock"
#define DAEMON_STOP_CODE 951753
#define DAEMON_CONFIG_MODE_CODE 789123
#define DAEMON_STATS_MODE_CODE 456987

union tcpip_socket_addr {
   struct addrinfo tcpip_addr; ///< used for TCPIP socket
   struct sockaddr_un unix_addr; ///< used for path of UNIX socket
};

int connect_to_supervisor(char *socket_path)
{
   int sockfd;
   union tcpip_socket_addr addr;

   memset(&addr, 0, sizeof(addr));

   addr.unix_addr.sun_family = AF_UNIX;
   snprintf(addr.unix_addr.sun_path, sizeof(addr.unix_addr.sun_path) - 1, "%s", socket_path);
   sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (connect(sockfd, (struct sockaddr *) &addr.unix_addr, sizeof(addr.unix_addr)) == -1) {
      return -1;
   }
   return sockfd;
}

int main(int argc, char **argv)
{
   int x = 0;
   int connected = FALSE;
   int ret_val;
   int supervisor_sd = -1;
   FILE * supervisor_stream_input = NULL;
   FILE * supervisor_stream_output = NULL;
   char buffer[1000];
   memset(buffer,0,1000);
   int command;

   char testbuffer[100];
   memset(testbuffer,0,100);
   char *socket_path = NULL;
   int just_stats = FALSE;

   int opt;
   while ((opt = getopt(argc, argv, "hs:x")) != -1) {
      switch (opt) {
      case 'h':
         printf("Usage: supervisor_cli [-h] [-s <path>]\n"
               "\t-h\tshows this help\n\t-s <path>\tpath to UNIX socket file\n");
         return 0;
      
      case 's':
         socket_path = optarg;
         break;

      case 'x':
         just_stats = TRUE;
         break;

      default:
         fprintf(stderr, "Invalid arguments.\n");
         return 3;
      }
   }
   if (socket_path == NULL) {
      /* socket_path was not set by user, use default value. */
      socket_path = DAEMON_UNIX_PATH_FILENAME_FORMAT;
   }

   supervisor_sd = connect_to_supervisor(socket_path);
   if(supervisor_sd < 0){
      printf("Could not connect to supervisor.\n");
      return 0;
   } else {
      connected = TRUE;
   }

   supervisor_stream_input = fdopen(supervisor_sd, "r");
   if(supervisor_stream_input == NULL) {
      printf("1Fdopen error\n");
      close(supervisor_sd);
      return;
   }

   supervisor_stream_output = fdopen(supervisor_sd, "w");
   if(supervisor_stream_output == NULL) {
      printf("1Fdopen error\n");
      close(supervisor_sd);
      return;
   }

   int supervisor_stream_input_fd = fileno(supervisor_stream_input);
   if(supervisor_stream_input_fd < 0) {
      printf("2Fdopen error\n");
      close(supervisor_sd);
      return;
   }

   fd_set read_fds;
   struct timeval tv;

   if(just_stats) {
      fprintf(supervisor_stream_output,"%d\n", DAEMON_STATS_MODE_CODE);
   } else {
      fprintf(supervisor_stream_output,"%d\n", DAEMON_CONFIG_MODE_CODE);
   }
   fflush(supervisor_stream_output);


   while (connected) {
      FD_ZERO(&read_fds);
      FD_SET(supervisor_stream_input_fd, &read_fds);
      FD_SET(0, &read_fds);

      tv.tv_sec = 1;
      tv.tv_usec = 0;

      ret_val = select(supervisor_stream_input_fd+1, &read_fds, NULL, NULL, &tv);

      if (ret_val == -1) {
         perror("select()");
         close(supervisor_stream_input_fd);
         fclose(supervisor_stream_input);
         fclose(supervisor_stream_output);
         close(supervisor_sd);
         connected = FALSE;
         break;
      } else if (ret_val) {
         if (FD_ISSET(0, &read_fds)) {
            if ((scanf("%s", buffer) == 0) || (strcmp(buffer,"Cquit") == 0)) {
               close(supervisor_stream_input_fd);
               fclose(supervisor_stream_input);
               fclose(supervisor_stream_output);
               close(supervisor_sd);
               connected = FALSE;
               break;
            } else if (strcmp(buffer,"Dstop") == 0) {
               fprintf(supervisor_stream_output,"%d\n", DAEMON_STOP_CODE);
               fflush(supervisor_stream_output);
            } else {
               fprintf(supervisor_stream_output,"%s\n",buffer);
               fflush(supervisor_stream_output);
            }
         }
         if (FD_ISSET(supervisor_stream_input_fd, &read_fds)) {
            usleep(200000);
            ioctl(supervisor_stream_input_fd, FIONREAD, &x);
            if (x == 0) {
               printf("Supervisor has disconnected, I'm done.\n");
               close(supervisor_sd);
               fclose(supervisor_stream_input);
               fclose(supervisor_stream_output);
               close(supervisor_stream_input_fd);
               connected = FALSE;
               break;
            } else {
               int y;
               for(y=0; y<x; y++) {
                  printf("%c", (char) fgetc(supervisor_stream_input));
               }
            }
         }
      } else {
         // timeout, nothing to do
      }
      memset(buffer,0,1000);
      fsync(supervisor_stream_input_fd);
      fflush(supervisor_stream_input);
      fflush(stdout);

      if (just_stats) {
         close(supervisor_sd);
         fclose(supervisor_stream_input);
         fclose(supervisor_stream_output);
         close(supervisor_stream_input_fd);
         return 0;
      }
   }

   return 0;
}
