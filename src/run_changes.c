
#include <libtrap/trap.h>
#include <sysrepo/xpath.h>

#include "run_changes.h"
#include "module.h"
#include "conf.h"
#include "inst_control.h"

#define RUN_CHE_STR(che) ((che)->type == RUN_CHE_T_INVAL ? "--" : ((che)->type == RUN_CHE_T_INST ? (che)->inst_name : (che)->mod_name))

/**
 * @brief Defines type of action to take for changed node (module or group).
 * */
typedef enum run_change_action_e {
   RUN_CHE_ACTION_NONE, ///< Take no action, do not restart or stop the element
   RUN_CHE_ACTION_DELETE, ///< Stop the element and remove it from vector
   RUN_CHE_ACTION_RESTART, ///< Restart the element
} run_change_action_t;

/**
 * @brief Type of run_change_t parsed from XPATH of old or new value.
 */
typedef enum run_change_type_e {
   RUN_CHE_T_INVAL, ///< Type was not recognized or set

   RUN_CHE_T_INST,
   RUN_CHE_T_MOD, ///< Module root E.g. /.../module-group[name='x']/module[name='y']
} run_change_type_t;

/**
 * @brief Specifies change from sysrepo, node that got changed, type and action to take.
 * @details Type and names gets parsed from XPATH of changed element. In case type is
 *  NS_CHE_TYPE_GROUP_NODE or NS_CHE_TYPE_GROUP_ROOT, module_name is NULL.
 * */
typedef struct run_change_s {
   char *mod_name; ///< Name of Nemea module that got changed
   char *inst_name; ///< Name of Nemea module instance that got changed
   char *node_name; ///< Name of tree node that got changed,
                    ///<  applies only to NS_CHE_TYPE_*_NODE
   run_change_type_t type; ///< Type of change
   run_change_action_t action; ///< Action to take for this module or group
} run_change_t;


/**
 * @brief Creates new run_change_t from given params from given xpath.
 * @details Gets XPATH of modified element from old_val or new_val depending on
 *  operation op. Parses XPATH and by found nodes sets type of sysrepo tree node,
 *  group name and possibly module name.
 * @param op Operation relationship between new_val and old_val
 * @param old_val Old value before change
 * @param new_val New value after change
 * @return Pointer to new dynamically allocated run_change_t.
 * */
static run_change_t * 
run_change_load(sr_change_oper_t op, sr_val_t *old_val, sr_val_t *new_val);

/**
 * @brief Frees dynamic fields of elem and sets initial values of the struct.
 * @param elem Element to be freed
 * */
static inline void
run_change_free(run_change_t **elem);

/**
 * @brief For each registered change in reg_chgs vector execute the
 *  change - stop/restart group or module.
 * @param sess Sysrepo session context for loading in case of restart action
 * @param reg_chgs Vector of registered changes
 * @return In case of restart action, it returns sr_error_t from
 *  configuration loading functions.
 * */
static inline int
run_change_proc_reg_chgs(sr_session_ctx_t *sess, vector_t *reg_chgs);

/**
 * @brief Handles SR_OP_CREATE operation for given change.
 * @details Assigns change action and adds it to vector via run_change_add_new_change.
 * @see run_change_add_new_change()
 * @param reg_chgs Vector of already registered changes
 * @param change New change to handle
 * */
static inline void run_change_handle_create(run_change_t *change);
static inline void run_change_handle_delete(run_change_t *change);
static inline void run_change_handle_modify(run_change_t *change);

/**
 * @brief Checks case where new and reg change are of same type and name. If so, it replaces values in reg by values in new.
 * */
static inline int
run_change_replace_same_registered(run_change_t *new, run_change_t *reg);

/**
 * @brief Checks case where new change of instance (new variable) is already handled by change of its owner module (reg). In case it's handled, new change is ignored.
 * */
static inline int run_change_ignore_new(run_change_t *new, run_change_t *reg);

/**
 * @brief  Adds n_change to reg_chgs vector or ignores the change in case it
 *  would be handled by some other change that is already in the vector.
 * @details TODO big time
 * @param reg_chgs Vector of already registered changes
 * @param n_change New change to add
 * */
static inline void
run_change_add_new_change(vector_t *reg_chgs, run_change_t *n_change);

static char * run_change_type_str(run_change_type_t type);

int
run_config_change_cb(sr_session_ctx_t *sess,
                     const char *smn,
                     sr_notif_event_t evnt,
                     void *priv_ctx)
{

#define NULLP_TEST_AND_FREE_SR_VAL(val) do { \
   if ((val) != NULL) { \
      sr_free_val((val)); \
      (val) = NULL; \
   } \
} while (0);

   int rc;
   sr_change_iter_t *iter = NULL;
   sr_val_t *new_val = NULL;
   sr_val_t *old_val = NULL;
   sr_change_oper_t op;
   run_change_t *change = NULL;
   vector_t reg_chgs; // Registered changes

   /* This should allocate enough space since not all avaiable modules and it's instances
    *  configuration gets to reload at the same time */
   vector_init(&reg_chgs, avmods_v.total + insts_v.total + 1);


   VERBOSE(V2, "Config change captured inside run_config_change_cb.")
   pthread_mutex_lock(&config_lock);

   VERBOSE(V3, "Getting changes iterator for %s", NS_ROOT_XPATH)
   rc = sr_get_changes_iter(sess, NS_ROOT_XPATH, &iter);
   if (rc != SR_ERR_OK) {
      VERBOSE(N_ERR, "Failed to get changes iterator: %s", sr_strerror(rc));
      goto err_cleanup;
   }

   rc = sr_get_change_next(sess, iter, &op, &old_val, &new_val);
   while (rc == SR_ERR_OK) {
      VERBOSE(V3, "-----NEXT CHANGE LOOP-----")
      if (old_val != NULL) { VERBOSE(V3, " old_val=%s", old_val->xpath) }
      if (new_val != NULL) { VERBOSE(V3, " new_val=%s", new_val->xpath) }

      change = run_change_load(op, old_val, new_val);
      if (change == NULL) {
         VERBOSE(N_ERR, "Failed to parse XPATH of change type %d", op)
         goto err_cleanup;
      }

      switch (op) {
         case SR_OP_CREATED:
            VERBOSE(V3, " CREATED %s (%s)", run_change_type_str(change->type),
                    RUN_CHE_STR(change));
            run_change_handle_create(change);
            break;
         case SR_OP_MODIFIED:
            VERBOSE(V3, " MODIFIED %s (%s)", run_change_type_str(change->type),
                    RUN_CHE_STR(change));
            run_change_handle_modify(change);
            break;
         case SR_OP_DELETED:
            VERBOSE(V3, " DELETED %s (%s)", run_change_type_str(change->type),
                    RUN_CHE_STR(change));
            run_change_handle_delete(change);
            break;
         case SR_OP_MOVED:
            VERBOSE(V3, " MOVED %s (%s)", run_change_type_str(change->type),
                    RUN_CHE_STR(change));
            /* This operation is intentionally omitted, since it's quite
             * complex and not really required to be handled. */
            break;
         default:
            // This branch is here so that compiler doesn't complain
            break;
      }

      if (change->action != RUN_CHE_ACTION_NONE) {
         run_change_add_new_change(&reg_chgs, change);
      } else {
         VERBOSE(V3, "Runtime change ignore for old XPATH=%s new XPATH=%s",
                 old_val->xpath, new_val->xpath)
      }

      // Both cases might happen, depending on operation
      NULLP_TEST_AND_FREE_SR_VAL(old_val)
      NULLP_TEST_AND_FREE_SR_VAL(new_val)
      rc = sr_get_change_next(sess, iter, &op, &old_val, &new_val);
   }

   if (rc != SR_ERR_NOT_FOUND) {
      // This should be returned if there is no element left, other codes are errors
      VERBOSE(N_ERR, "Fetching next change failed. Sysrepo error: %s", sr_strerror(rc))
      goto err_cleanup;
   }

   rc = run_change_proc_reg_chgs(sess, &reg_chgs);
   if (rc != SR_ERR_OK) {
      goto err_cleanup;
   }

   VERBOSE(V2, "Successfully leaving change callback")
   pthread_mutex_unlock(&config_lock);
   sr_free_change_iter(iter);

   return SR_ERR_OK;

err_cleanup:
   pthread_mutex_unlock(&config_lock);

   for (uint32_t i = 0; change = reg_chgs.items[i], i < reg_chgs.total; i++) {
      run_change_free(&change);
   }
   vector_free(&reg_chgs);

   if (old_val != NULL) {
      sr_free_val(old_val);
   }
   if (new_val != NULL) {
      sr_free_val(new_val);
   }
   if (iter != NULL) {
      sr_free_change_iter(iter);
   }

   return rc;
}

static inline int
run_change_replace_same_registered(run_change_t *new, run_change_t *reg)
{
   if (new->type == RUN_CHE_T_INST && reg->type == RUN_CHE_T_INST) {
      if (strcmp(new->inst_name, reg->inst_name) == 0) {
         reg->action = new->action;
         reg->type = new->type;
         VERBOSE(V3, "New INSTANCE '%s' change updates already registred change",
                 new->inst_name)
         run_change_free(&new);
         return 0;
      }
   }

   if (new->type == RUN_CHE_T_MOD && reg->type == RUN_CHE_T_MOD) {
      if (strcmp(new->mod_name, reg->mod_name) == 0) {
         reg->action = new->action;
         reg->type = new->type;
         VERBOSE(V3, "New MODULE '%s' change updates already registred change",
                 new->mod_name)
         run_change_free(&new);
         return 0;
      }
   }

   // Replace of registred change was not possible, return error
   return -1;
}

static inline int run_change_ignore_new(run_change_t *new, run_change_t *reg)
{
   if (reg->type == RUN_CHE_T_MOD && new->type == RUN_CHE_T_INST) {
      inst_t * inst = inst_get_by_name(new->inst_name, NULL);

      if (inst != NULL && strcmp(reg->mod_name, inst->mod_ref->name) == 0) {
         VERBOSE(V3, "New change of %s (%s) is being ignored due to registered %s (%s) change",
                 run_change_type_str(new->type), RUN_CHE_STR(new),
                 run_change_type_str(reg->type), RUN_CHE_STR(reg))
         run_change_free(&new);
         return 0;
      }
   }

   return -1;
}

static inline void run_change_add_new_change(vector_t *reg_chgs, run_change_t *n_change)
{
   run_change_t *r_change = NULL; // Already registered change


   for (uint32_t i = 0; i < reg_chgs->total; i++) {
      r_change = reg_chgs->items[i];

      if (run_change_replace_same_registered(n_change, r_change) == 0) {
            return;
      }
      if (run_change_ignore_new(n_change, r_change) == 0) {
         return;
      }

      /**
       * Checks case where registered change is for instance and new change is
       * for module. If module that should be changed is parent of instance which
       * change is registered, then registered instance change is removed.
       * */
      if (r_change->type == RUN_CHE_T_INST && n_change->type == RUN_CHE_T_MOD) {
         inst_t *inst = inst_get_by_name(r_change->inst_name, NULL);
         if (inst != NULL && strcmp(inst->mod_ref->name, n_change->mod_name) == 0) {
            VERBOSE(V3, "Removing change of instance '%s', it's child of '%s'",
                    r_change->inst_name, n_change->mod_name)
            run_change_free(&r_change);
            vector_delete(reg_chgs, i);
            i--;
         }
      }
   }

   /* This new change is not registered yet and doesn't interfere with
    * already registered ones. Add it to vector of registered changes */
   vector_add(reg_chgs, n_change);
   VERBOSE(V3, "New change of %s (%s) registered",
           run_change_type_str(n_change->type), RUN_CHE_STR(n_change))
}

static inline void run_change_handle_modify(run_change_t *change)
{
   if (change->type == RUN_CHE_T_MOD) {
      change->action = RUN_CHE_ACTION_RESTART;
   } else if (change->type == RUN_CHE_T_INST) {
      if (change->node_name != NULL && strcmp(change->node_name, "last-pid") == 0) {
         change->action = RUN_CHE_ACTION_NONE;
      } else {
         change->action = RUN_CHE_ACTION_RESTART;
      }
   }
}

static inline void run_change_handle_delete(run_change_t *change)
{
   change->action = RUN_CHE_ACTION_DELETE;
   if (change->type == RUN_CHE_T_MOD) {
      if (change->node_name != NULL) {
         // not whole module was deleted, restart the module to load new configuration
         change->action = RUN_CHE_ACTION_RESTART;
      }
   } else if (change->type == RUN_CHE_T_INST) {
      if (change->node_name != NULL) {
         if (strcmp(change->node_name, "last-pid") == 0) {
            change->action = RUN_CHE_ACTION_NONE;
         } else {
            // not whole instance was deleted, old its node. restart instance to load new configuration
            change->action = RUN_CHE_ACTION_RESTART;
         }
      } else {
         change->action = RUN_CHE_ACTION_DELETE;
      }
   }
}

static inline void run_change_handle_create(run_change_t *change)
{
   if (change->type == RUN_CHE_T_MOD) {
      change->action = RUN_CHE_ACTION_RESTART;
   } else if (change->type == RUN_CHE_T_INST) {
      if (change->node_name != NULL && strcmp(change->node_name, "last-pid") == 0) {
         change->action = RUN_CHE_ACTION_NONE;
      } else {
         change->action = RUN_CHE_ACTION_RESTART;
      }
   }
}

static inline int run_change_proc_restart(sr_session_ctx_t *sess, run_change_t * change)
{
   int rc = 0;

   switch (change->type) {
      case RUN_CHE_T_INST:
         VERBOSE(V3, "Action restart for instance '%s'", change->inst_name)
         inst_stop_remove_by_name(change->inst_name);
         rc = inst_load_by_name(sess, change->inst_name);
         break;
      case RUN_CHE_T_MOD:
         VERBOSE(V3, "Action restart for instances of module '%s'", change->mod_name)

         av_module_stop_remove_by_name(change->mod_name);
         rc = av_module_load_by_name(sess, change->mod_name);
         break;
      default:
         break;
   }

   return rc;
}

static inline void run_change_proc_delete(sr_session_ctx_t *sess, run_change_t *change)
{
   if (change->type == RUN_CHE_T_INST) {
      inst_stop_remove_by_name(change->inst_name);
   } else if (change->type == RUN_CHE_T_MOD) {
      av_module_stop_remove_by_name(change->mod_name);
   }
}

static inline int run_change_proc_reg_chgs(sr_session_ctx_t *sess, vector_t *reg_chgs)
{
   int rc;
   run_change_t *change;

   VERBOSE(V3, "Processing %d registered changes", reg_chgs->total)
   // Go from back so that if there is error, not processed elements get freed
   for (long i = reg_chgs->total - 1; i >= 0; i--) {
      change = reg_chgs->items[i];
      VERBOSE(V3, "Processing change of %s (%s)",
              run_change_type_str(change->type), RUN_CHE_STR(change));
      if (change->action == RUN_CHE_ACTION_RESTART) {
         rc = run_change_proc_restart(sess, change);

         if (rc != SR_ERR_OK) {
            //VERBOSE(N_ERR, "Failed to restart ",
            //        change->grp_name, change->mod_name, change->inst_name)
            reg_chgs->total = (uint32_t) (i == 0 ? 0 : i) ;
            run_change_free(&change);
            return rc;
         }
      } else if (change->action == RUN_CHE_ACTION_DELETE) {
         run_change_proc_delete(sess, change);
      } else {
         VERBOSE(V3, "Change ignored")
      }

      run_change_free(&change);
   }
   vector_free(reg_chgs);

   return SR_ERR_OK;
}

static run_change_t *
run_change_load(sr_change_oper_t op, sr_val_t *old_val, sr_val_t *new_val)
{
   char *xpath;
   sr_xpath_ctx_t state = {0};
   char *res = NULL;
   run_change_t *change = NULL;

   switch (op) {
      case SR_OP_MODIFIED:
      case SR_OP_CREATED:
         xpath = new_val->xpath;
         break;
      case SR_OP_DELETED:
         xpath = old_val->xpath;
         break;
      case SR_OP_MOVED:
         return NULL;
      default:
         xpath = NULL;
   }

   change = (run_change_t *) calloc(1, sizeof(run_change_t));
   if (change == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }
   change->action = RUN_CHE_ACTION_NONE;

   res = sr_xpath_next_node(xpath, &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to parse run_change on line %d", __LINE__)
      goto err_cleanup;
   }

   // available-module OR module
   res = sr_xpath_next_node(NULL, &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to parse run_change on line %d", __LINE__)
      goto err_cleanup;
   }

   { // retrieve name of available-module OR module
      if (strcmp(res, "available-module") == 0) {
         res = sr_xpath_node_key_value(NULL, "name", &state);
         if (res == NULL) {
            VERBOSE(N_ERR, "Failed to parse run_change on line %d", __LINE__)
            sr_xpath_recover(&state);
            return change;
         }
         change->type = RUN_CHE_T_MOD;
         change->mod_name = strdup(res);
         if (change->mod_name == NULL) {
            NO_MEM_ERR
            goto err_cleanup;
         }
      } else if (strcmp(res, "instance") == 0) {
         res = sr_xpath_node_key_value(NULL, "name", &state);
         if (res == NULL) {
            VERBOSE(N_ERR, "Failed to parse run_change on line %d", __LINE__)
            sr_xpath_recover(&state);
            return change;
         }
         change->type = RUN_CHE_T_INST;
         change->inst_name = strdup(res);
         if (change->inst_name == NULL) {
            NO_MEM_ERR
            goto err_cleanup;
         }
      } else {
         change->type = RUN_CHE_T_INVAL;
         sr_xpath_recover(&state);
         return change;
      }
   }

   // node name
   res = sr_xpath_next_node(NULL, &state);
   if (res == NULL) {
      if (change->type == RUN_CHE_T_INVAL) {
         change->type = RUN_CHE_T_MOD;
         sr_xpath_recover(&state);
         return change;
      } else {
         // it's type of PREFIX/instance[name='vportscan_detector1111']
         sr_xpath_recover(&state);
         return change;
      }
   }
   change->node_name = strdup(res);
   if (change->node_name == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   sr_xpath_recover(&state);

   return change;

err_cleanup:
   run_change_free(&change);
   sr_xpath_recover(&state);
   VERBOSE(N_ERR, "Failed run_change_load with XPATH=%s", xpath)

   return NULL;
}

static inline void run_change_free(run_change_t **elem)
{
   NULLP_TEST_AND_FREE((*elem)->mod_name)
   NULLP_TEST_AND_FREE((*elem)->inst_name)
   NULLP_TEST_AND_FREE((*elem)->node_name)
   NULLP_TEST_AND_FREE((*elem))
}

static char * run_change_type_str(run_change_type_t type)
{
   switch(type) {
      case RUN_CHE_T_MOD:
         return "MODULE NODE";
      case RUN_CHE_T_INST:
         return "INSTANCE NODE";
      default: // NS_CHE_T_INVAL
         return "INVALID";
   }
}