/**
 * \file supervisor_main.c
 * \brief Supervisor main function.
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

#include "supervisor_api.h"
#include "internal.h"
#include <stdio.h>

void interactive_mode()
{
   int ret_val = 0;

   while ((ret_val = interactive_get_option()) != 8) {
      switch (ret_val) {
      case 1:
         interactive_start_configuration();
         break;

      case 2:
         interactive_stop_configuration();
         break;

      case 3:
         interactive_set_module_enabled();
         break;

      case 4:
         interactive_stop_module();
         break;

      case 5:
         interactive_show_running_modules_status();
         break;

      case 6:
         interactive_show_available_modules();
         break;

      case 7:
         reload_configuration(RELOAD_INTERACTIVE, NULL);
         break;

      default:
         VERBOSE(N_STDOUT, ANSI_RED_BOLD "[WARNING] Wrong input.\n" ANSI_ATTR_RESET);
         break;
      }
   }
}


int main (int argc, char *argv [])
{
   int ret_val = 0;

   // Initialize main control flags, which will be set in parse_program_arguments and supervisor_initialization functions
   supervisor_flags_initialization();
   // Parse program arguments
   ret_val = parse_program_arguments(&argc, argv);

   if (ret_val == DAEMON_MODE_CODE) {
      // Initialize a new daemon process and it's socket
      if (daemon_mode_initialization() == 0) {
         // Initialize supervisor's structures, service thread, output, signal handler and load startup configuration
         if (supervisor_initialization() == 0) {
            // Start server routine
            server_routine();
         }
      }
   } else if (ret_val == INTERACTIVE_MODE_CODE) {
      // Initialize supervisor's structures, service thread, output, signal handler and load startup configuration
      if (supervisor_initialization() == 0) {
         // Start interactive mode
         interactive_mode();
      }
   } else {
      /* Wrong input arguments - just terminate. */
   }

   // Cleanup all structures and join service thread
   supervisor_termination(FALSE, FALSE);

   exit(EXIT_SUCCESS);
}