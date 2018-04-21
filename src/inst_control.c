
#include <libtrap/trap.h>
#include "utils.h"
#include "inst_control.h"

/**
 * @brief Releases child processes of supervisor and cleans socket files
 * @details When supervisor kills instance it started (its child) or fails to start
 *  instance in forked process, the process stays as zombie in the system and needs
 *  to be released via waitpid(WNOHANG) inside this function.
 *  Some instances are also leaving their libtrap socket files in the filesystem so
 *  those get removed in this function as well.
 * */
static void clean_after_children();

/**
 * @brief Start single instance process
 * @param inst Instance to start
 * */
static void inst_start(inst_t *inst);


uint32_t get_running_insts_cnt()
{
   uint32_t some_inst_running = 0;

   inst_t *inst;
   for (uint32_t i = 0; i < insts_v.total; i++) {
      inst = insts_v.items[i];

      if (inst->pid <= 0) {
         continue;
      }
      inst_set_running_status(inst);
      some_inst_running += inst->running ? 1 : 0;
   }

   return some_inst_running;
}

void insts_stop_sigkill()
{
   // Find running status of each instance first
   (void) get_running_insts_cnt();

   bool should_be_killed;
   inst_t *inst;
   for (uint32_t i = 0; i < insts_v.total; i++) {
      inst = insts_v.items[i];
      should_be_killed = (inst->enabled == false || inst->should_die);

      if (inst->running
          && inst->sigint_sent
          && should_be_killed) {

         // Only running instance that is disabled or has group disabled
         VERBOSE(V2, "Stopping inst (%s). Sending SIGKILL",
                 inst->name)
         kill(inst->pid, SIGKILL);
      }
   }
   clean_after_children();
}

void insts_stop_sigint()
{
   bool should_be_killed;
   inst_t *inst;
   for (uint32_t i = 0; i < insts_v.total; i++) {
      inst = insts_v.items[i];

      should_be_killed = (inst->enabled == false|| inst->should_die);

      if (inst->running
          && inst->root_perm_needed == false
          && inst->sigint_sent == false
          && should_be_killed) {

            VERBOSE(V2, "Stopping inst (%s). Sending SIGINT", inst->name)
            kill(inst->pid, SIGINT);
            inst->sigint_sent = true;
            inst->restarts_cnt = 0;
      }
   }
   usleep(WAIT_FOR_INSTS_TO_HANDLE_SIGINT);
   clean_after_children();
}

void av_module_stop_remove_by_name(const char *name)
{
   uint32_t fi; // Index of found module
   av_module_t *mod = NULL;

   VERBOSE(V2, "Stopping instances of module '%s'", name)

   { // find module structure by name
      av_module_t *tmp = NULL;
      for (uint32_t i = 0; i < avmods_v.total; i++) {
         tmp = avmods_v.items[i];
         if (strcmp(tmp->name, name) == 0) {
            fi = i;
            mod = tmp;
            break;
         }
      }

      if (mod == NULL) {
         // Module was not found, no need to do anything
         return;
      }
   }

   inst_t *inst;
   for (uint32_t i = 0; i < insts_v.total; i++) {
      inst = insts_v.items[i];
      if (strcmp(inst->mod_ref->name, name) == 0) {
         if (inst->pid > 0) {
            kill(inst->pid, SIGINT);
         }
      }
   }
   usleep(WAIT_FOR_INSTS_TO_HANDLE_SIGINT);
   for (uint32_t i = 0; i < insts_v.total; i++) {
      inst = insts_v.items[i];
      if (strcmp(inst->mod_ref->name, name) == 0) {
         VERBOSE(V3, "Stopping instance '%s'", inst->name)
         if (inst->pid > 0) {
            kill(inst->pid, SIGKILL);
         }
         vector_delete(&insts_v, i);
         inst_free(inst);
         i--; // TODO
      }
   }


   vector_delete(&avmods_v, fi);
   av_module_free(mod);
}

void inst_stop_remove_by_name(const char *name)
{
   uint32_t fi; // Index of found instance
   inst_t *inst = inst_get_by_name(name, &fi);

   if (inst == NULL) {
      // Instance was not found, no need to do anything
      return;
   }
   VERBOSE(V3, "Stopping instance '%s'", inst->name)

   if (inst->pid > 0) {
      kill(inst->pid, SIGINT);
      usleep(WAIT_FOR_INSTS_TO_HANDLE_SIGINT);
      kill(inst->pid, SIGKILL);
   }

   vector_delete(&insts_v, fi);
   inst_free(inst);
}

void insts_terminate()
{
   inst_t *inst = NULL;
   pthread_mutex_lock(&config_lock);
   for (uint32_t i = 0; i < insts_v.total; i++) {
      inst = insts_v.items[i];

      if (inst->enabled) {
         inst->enabled = false;
      }
   }

   VERBOSE(V3, "Killing all instances")
   // Ensure they get killed
   insts_stop_sigint();
   usleep(WAIT_FOR_INSTS_TO_HANDLE_SIGINT);
   (void) get_running_insts_cnt();
   insts_stop_sigkill();
   pthread_mutex_unlock(&config_lock);
}

void insts_start()
{
   time_t time_now;
   VERBOSE(V3, "Updating instances status")

   inst_t *inst;
   for (uint32_t i = 0; i < insts_v.total; i++) {
      inst = insts_v.items[i];

      if (inst->enabled == false || inst->running == true) {
         continue;
      }

      time(&time_now);

      // Has it been less than minute since last start attempt?
      if (time_now - inst->restart_time <= 60) {
         inst->restarts_cnt++;
         if (inst->restarts_cnt == inst->max_restarts_minute) {
            VERBOSE(V2,
                    "Instance '%s' reached restart limit. Disabling.",
                    inst->name)
            inst->enabled = false;
         } else {
            inst_start(inst);
         }
      } else {
         inst->restarts_cnt = 0;
         inst->restart_time = time_now;
         inst_start(inst);
      }
   }

   // Clean after instances that failed to start
   clean_after_children();
}

static void clean_after_children()
{
   pid_t result;
   int status;

   inst_t *inst;
   for (uint32_t i = 0; i < insts_v.total; i++) {
      inst = insts_v.items[i];

      if (inst->pid > 0 && inst->is_my_child) {
         /* waitpid releases children that failed to execute execv in inst_start.
          * if this would be left out, processes would stay there as zombies */
         result = waitpid(inst->pid , &status, WNOHANG);
         switch (result) {
            case 0:
               if (inst->sigint_sent) {
                  VERBOSE(V1, "waitpid: Instance (%s) is still running after killing",
                          inst->name)
               }
               break;

            case -1: // Error
               if (errno == ECHILD) {
                  // Process with PID doesn't exist or isn't supervisor's child
                  inst->is_my_child = false;
               }
               VERBOSE(V2, "waitpid: Some error occured, but inst %s is not running",
                       inst->name)
               inst->should_die = true;
               //inst->running = false;
               inst_clear_socks(inst);
               break;

            default: // Module is not running
               VERBOSE(V2, "waitpid: Instance %s is not running %d", inst->name, result)
               //inst->should_die = true;
               //inst->running = false;
               if (inst->enabled == false) {
                  inst_clear_socks(inst);
               }
         }
      }
   }
}

void inst_set_running_status(inst_t *inst)
{
   // Send SIGHUP to check whether process exists
   if (inst->pid > 0 && kill(inst->pid, 0) != -1) {
      inst->running = true;
      inst->root_perm_needed = false;
   } else {
      switch (errno) {
         case EPERM:
            // Error: process doesn't have permission to signal target
            if (inst->root_perm_needed == false) {
               VERBOSE(V2,
                       "kill -0: Does not have permissions to send"
                             "signals to inst '%s' (PID: %d)",
                       inst->name, inst->pid)
               inst->root_perm_needed = true;
            }
            inst->running = true;
            break;

         case ESRCH:
            // Error: process doesn't exist
            VERBOSE(V3,
                    "kill -0: %s (PID: %d) is not running!",
                    inst->name, inst->pid)
            inst->running = false;

         default:
            if (inst->service_sd != -1) {
               close(inst->service_sd);
               inst->service_sd = -1;
            }
            inst->running = false;
            inst->pid = 0;
            inst->service_ifc_connected = false;
      }
   }
}

void inst_start(inst_t *inst)
{
   char log_path_out[PATH_MAX];
   char log_path_err[PATH_MAX];

   VERBOSE(V2, "Starting '%s' from %s", inst->name, inst->mod_ref->path)

   memset(log_path_err, 0, PATH_MAX);
   memset(log_path_out, 0, PATH_MAX);
   sprintf(log_path_out,"%s%s/%s_stdout", logs_path, INSTANCES_LOGS_DIR_NAME, inst->name);
   sprintf(log_path_err,"%s%s/%s_stderr", logs_path, INSTANCES_LOGS_DIR_NAME, inst->name);

   time_t time_now;
   time(&time_now);

   // If the instance was killed due to one of these variables, they should be reseted
   inst->should_die = false;
   inst->sigint_sent = false;

   fflush(stdout);
   inst->pid = fork();

   if (inst->pid == -1) {
      inst->running = false;
      VERBOSE(N_ERR,"Fork: could not fork supervisor process!")
      return;
   }

   if (inst->pid != 0) {
      // Running as parent
      inst->is_my_child = true;
      inst->running = true;
   } else {
      // Running as forked child
      int fd_stdout = open(log_path_out, O_RDWR | O_CREAT | O_APPEND, PERM_LOGSDIR);
      int fd_stderr = open(log_path_err, O_RDWR | O_CREAT | O_APPEND, PERM_LOGSDIR);

      if (fd_stdout != -1) {
         dup2(fd_stdout, 1); //stdout
         close(fd_stdout);
      }
      if (fd_stderr != -1) {
         dup2(fd_stderr, 2); //stderr
         close(fd_stderr);
      }

      /*
       * Important for sending SIGINT to supervisor.
       * */
      setsid();

      // Don't even think about rewriting this to VERBOSE macro
      fprintf(stdout, "[INFO]%s Supervisor executed following command from path=%s: ",
              get_formatted_time(), inst->mod_ref->path);
      for(int i = 0; inst->exec_args[i] != NULL; i++) {
         fprintf(stdout, " %s", inst->exec_args[i]);
      }
      fprintf(stdout, "\n");
      VERBOSE(V1, "")

      // Make sure execution message gets written to logs files
      fflush(stdout);
      fflush(stderr);

      execv(inst->mod_ref->path, inst->exec_args);

      { // If correctly started, this won't be executed
         VERBOSE(N_ERR,
                 "Could not execute '%s' binary! (execvp errno=%d)",
                 inst->name,
                 errno)
         fflush(stdout);
         //terminate_supervisor(false);
         close(fd_stdout);
         close(fd_stderr);
         _exit(EXIT_FAILURE);
      }
   }
}