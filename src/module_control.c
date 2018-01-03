
#include <libtrap/trap.h>
#include "module_control.h"

/*--BEGIN superglobal vars--*/
//without extern
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

   instance_t *inst = NULL;
   FOR_EACH_IN_VEC(insts_v, inst) {
      if (inst->running
          && inst->sigint_sent
          && (inst->module->group->enabled == false || inst->enabled == false)) {

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
   instance_t *inst = NULL;
   FOR_EACH_IN_VEC(insts_v, inst) {
      if (inst->running
          && inst->root_perm_needed == false
          && inst->sigint_sent == false
          && (inst->module->group->enabled == false || inst->enabled == false)) {

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

   instance_remove_at(fi);
   instance_free(inst);
}
/* --END superglobal fns-- */

/* --BEGIN local fns-- */
static void clean_after_children()
{

   instance_t *inst = NULL;
   pid_t result;
   int status;

   FOR_EACH_IN_VEC(insts_v, inst) {
      if (inst->pid > 0 && inst->is_my_child && inst->sigint_sent) {
         /* waitpid releases children that failed to execute execv in inst_start.
          * if this would be left out, processes would stay there as zombies */
         result = waitpid(inst->pid , &status, WNOHANG);
         switch (result) {
            case 0:
               VERBOSE(V1, "Instance (%s) is still running after killing",
                       instance_tree_path(inst))
               break;

            case -1: // Error
               if (errno == ECHILD) {
                  // Process with PID doesn't exist or isn't supervisor's child
                  inst->is_my_child = false;
               }
               VERBOSE(V2, "Some error occured, but inst %s is not running",
                       instance_tree_path(inst))
               inst->running = false;
               instance_clear_socks(inst);
               break;

            default: // Module is not running
               VERBOSE(V2, "Instance %s is not running", instance_tree_path(inst))
               inst->running = false;
               instance_clear_socks(inst);
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
/* --END local fns-- */
