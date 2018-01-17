
#include <libtrap/trap.h>
#include "instance_control.h"
#include "main.h"

/*--BEGIN superglobal vars--*/
/*--END superglobal vars--*/

/*--BEGIN local #define--*/
/*--END local #define--*/

/*--BEGIN local typedef--*/
/*--END local typedef--*/

/* --BEGIN local vars-- */
/* --END local vars-- */

/* --BEGIN full fns prototypes-- */

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
static void inst_start(instance_t *inst);
/* --END full fns prototypes-- */

/* --BEGIN superglobal fns-- */

uint32_t get_running_insts_cnt()
{
   uint32_t some_instance_running = 0;
   instance_t *inst = NULL;

   FOR_EACH_IN_VEC(insts_v, inst) {
      if (inst->pid <= 0) {
         continue;
      }
      instance_set_running_status(inst);
      some_instance_running += inst->running ? 1 : 0;
   }

   return some_instance_running;
}

void insts_stop_sigkill()
{
   // Find running status of each instance first
   (void) get_running_insts_cnt();

   bool should_be_killed;
   instance_t *inst = NULL;
   FOR_EACH_IN_VEC(insts_v, inst) {
      should_be_killed = (inst->module->group->enabled == false
                    || inst->enabled == false
                    || inst->should_die);

      if (inst->running
          && inst->sigint_sent
          && should_be_killed) {

         // Only running instance that is disabled or has group disabled
         VERBOSE(V2, "Stopping inst (%s). Sending SIGKILL",
                 instance_tree_path(inst))
         kill(inst->pid, SIGKILL);
      }
   }
   clean_after_children();
}

void insts_stop_sigint()
{
   bool should_be_killed;
   instance_t *inst = NULL;
   FOR_EACH_IN_VEC(insts_v, inst) {
      should_be_killed = (inst->module->group->enabled == false
                   || inst->enabled == false
                   || inst->should_die);

      if (inst->running
          && inst->root_perm_needed == false
          && inst->sigint_sent == false
          && should_be_killed) {

            VERBOSE(V2, "Stopping inst (%s). Sending SIGINT", instance_tree_path(inst))
            kill(inst->pid, SIGINT);
            inst->sigint_sent = true;
            inst->restarts_cnt = 0;
      }
   }
   usleep(WAIT_FOR_INSTS_TO_HANDLE_SIGINT);
   clean_after_children();
}

void module_group_stop_remove_by_name(const char *name)
{
   uint32_t fi; // Index of found module
   module_group_t *grp = module_group_get_by_name(name, &fi);

   if (grp == NULL) {
      // Module group was not found, no need to do anything
      return;
   }

   module_t *mod = NULL;
   FOR_EACH_IN_VEC(mods_v, mod) {
      if (mod->group == grp) {
         module_stop_remove_by_name(mod->name);
      }
   }

   module_group_remove_at(fi);
   module_group_free(grp);
}

void module_stop_remove_by_name(const char *name)
{
   uint32_t fi; // Index of found module
   module_t *mod = module_get_by_name(name, &fi);

   if (mod == NULL) {
      // Module was not found, no need to do anything
      return;
   }

   instance_t *inst = NULL;
   FOR_EACH_IN_VEC(insts_v, inst) {
      if (inst->module == mod) {
         instance_stop_remove_by_name(inst->name);
      }
   }

   module_remove_at(fi);
   module_free(mod);
}

void instance_stop_remove_by_name(const char *name)
{
   uint32_t fi; // Index of found instance
   instance_t *inst = instance_get_by_name(name, &fi);

   if (inst == NULL) {
      // Instance was not found, no need to do anything
      return;
   }

   if (inst->pid > 0) {
      kill(inst->pid, SIGINT);
      usleep(WAIT_FOR_INSTS_TO_HANDLE_SIGINT);
      kill(inst->pid, SIGKILL);
   }

   run_module_remove_at(fi);
   run_module_free(inst);
}

void insts_terminate()
{
   instance_t *inst = NULL;
   pthread_mutex_lock(&config_lock);
   FOR_EACH_IN_VEC(insts_v, inst) {
      if (inst->enabled) {
         inst->enabled = false;
      }
   }

   VERBOSE(DEBUG, "killing all the flies")
   // Ensure they get killed
   insts_stop_sigint();
   usleep(WAIT_FOR_INSTS_TO_HANDLE_SIGINT);
   (void) get_running_insts_cnt();
   insts_stop_sigkill();
   pthread_mutex_unlock(&config_lock);
}

void insts_start()
{
   instance_t *inst = NULL;
   time_t time_now;
   VERBOSE(V3, "Updating instances status")

   FOR_EACH_IN_VEC(insts_v, inst) {
      if (inst->module->group->enabled == false || inst->enabled == false ||
          inst->running == true) {
         continue;
      }

      time(&time_now);

      // Has it been less than minute since last start attempt?
      if (time_now - inst->restart_time <= 60) {
         inst->restarts_cnt++;
         if (inst->restarts_cnt == inst->max_restarts_minute) {
            VERBOSE(MOD_EVNT,
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
/* --END superglobal fns-- */

/* --BEGIN local fns-- */
static void clean_after_children()
{

   instance_t *inst = NULL;
   pid_t result;
   int status;

   FOR_EACH_IN_VEC(insts_v, inst) {
      if (inst->pid > 0 && inst->is_my_child) {
         /* waitpid releases children that failed to execute execv in inst_start.
          * if this would be left out, processes would stay there as zombies */
         result = waitpid(inst->pid , &status, WNOHANG);
         switch (result) {
            case 0:
               if (inst->sigint_sent) {
                  VERBOSE(V1, "Instance (%s) is still running after killing",
                          instance_tree_path(inst))
               }
               break;

            case -1: // Error
               if (errno == ECHILD) {
                  // Process with PID doesn't exist or isn't supervisor's child
                  inst->is_my_child = false;
               }
               VERBOSE(V2, "Some error occured, but inst %s is not running",
                       instance_tree_path(inst))
               inst->should_die = true;
               //inst->running = false;
               run_module_clear_socks(inst);
               break;

            default: // Module is not running
               VERBOSE(V2, "Instance %s is not running %d", instance_tree_path(inst), result)
               inst->should_die = true;
               //inst->running = false;
               run_module_clear_socks(inst);
         }
      }
   }
}

void instance_set_running_status(instance_t *inst)
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
                    instance_tree_path(inst), inst->pid)
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

void inst_start(instance_t *inst)
{
   char log_path_out[PATH_MAX];
   char log_path_err[PATH_MAX];

   VERBOSE(V2, "Starting '%s'", instance_tree_path(inst))

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
       * TODO rewrite. why? just comment?
       * Important for sending SIGINT to supervisor.
       * Modules can't receive the signal too !!!
       * */
      setsid();

      // Don't even think about rewriting this to VERBOSE macro
      fprintf(stdout, "[INFO]%s Supervisor executed following command from path=%s: ",
              get_formatted_time(), inst->module->path);
      for(int i = 0; inst->exec_args[i] != NULL; i++) {
         fprintf(stdout, " %s", inst->exec_args[i]);
      }
      fprintf(stdout, "\n");
      VERBOSE(N_INFO, "")

      // Make sure execution message gets written to logs files
      fflush(stdout);
      fflush(stderr);

      execv(inst->module->path, inst->exec_args);

      { // If correctly started, this won't be executed
         VERBOSE(N_ERR,
                 "Could not execute '%s' binary! (execvp errno=%d)",
                 inst->name,
                 errno)
         fflush(stdout);
         //terminate_supervisor(false);
         VERBOSE(DEBUG, " trying to exit")
         close(fd_stdout);
         close(fd_stderr);
         _exit(EXIT_FAILURE);
      }
   }
}
/* --END local fns-- */
