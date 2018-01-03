
#include <libtrap/trap.h>
#include <sysrepo/xpath.h>
#include "utils.h"
#include "module.h"
#include "conf.h"
#include "instance_control.h"

/*--BEGIN superglobal vars--*/
extern const char *ns_root_sr_path;
/*--END superglobal vars--*/

/*--BEGIN local #define--*/
/*--END local #define--*/

/*--BEGIN local typedef--*/
/**
 * @brief Defines type of action to take for changed node (module or group).
 * */
typedef enum ns_change_action_e {
   NS_CHE_ACTION_NONE, ///< Take no action, do not restart or stop the element
   NS_CHE_ACTION_DELETE, ///< Stop the element and remove it from vector
   NS_CHE_ACTION_RESTART, ///< Restart the element
} ns_change_action_t;

/**
 * @brief Type of ns_change_t parsed from XPATH of old or new value.
 */
typedef enum ns_change_type_e {
   NS_CHE_T_INVAL, ///< Type was not recognized or set
   /**
    * Group node E.g. /.../module-group[name='x']/everything_not_module
    * */
   /**
    * Instance node -
    * E.g. /.../module-group[name='x']/module[name='y']/instance[name='z']/...
    * */
   //NS_CHE_T_INST_NOD,
   /**
    * Instance root -
    * E.g. /.../module-group[name='x']/module[name='y']/instance[name='z']
    * */
   //NS_CHE_T_INST_RT,
         NS_CHE_T_INST,


   //NS_CHE_T_MOD_NOD,
   //NS_CHE_T_MOD_RT, ///< Module root E.g. /.../module-group[name='x']/module[name='y']
   NS_CHE_T_MOD, ///< Module root E.g. /.../module-group[name='x']/module[name='y']


   //NS_CHE_T_GRP_NOD,
   //NS_CHE_T_GRP_RT, ///< Group root E.g. /.../module-group[name='x']
         NS_CHE_T_GRP, ///< Group root E.g. /.../module-group[name='x']
   /**
    * Module node E.g. /.../module-group[name='x']/module[name='y']/...
    * */
} ns_change_type_t;

/**
 * @brief Specifies change from sysrepo, node that got changed, type and action to take.
 * @details Type and names gets parsed from XPATH of changed element. In case type is
 *  NS_CHE_TYPE_GROUP_NODE or NS_CHE_TYPE_GROUP_ROOT, module_name is NULL.
 * */
typedef struct ns_change_s {
   char *grp_name; ///< Name of Nemea module group that got changed
   char *mod_name; ///< Name of Nemea module that got changed
   char *inst_name; ///< Name of Nemea module instance that got changed
   char *node_name; ///< Name of tree node that got changed,
                    ///<  applies only to NS_CHE_TYPE_*_NODE
   ns_change_type_t type; ///< Type of change
   ns_change_action_t action; ///< Action to take for this module or group
} ns_change_t;
/*--END local typedef--*/

/* --BEGIN local vars-- */
/* --END local vars-- */

/* --BEGIN full fns prototypes-- */
/**
 * @brief Creates new ns_change_t from given params from given xpath.
 * @details Gets XPATH of modified element from old_val or new_val depending on
 *  operation op. Parses XPATH and by found nodes sets type of sysrepo tree node,
 *  group name and possibly module name.
 * @param op Operation relationship between new_val and old_val
 * @param old_val Old value before change
 * @param new_val New value after change
 * @return Pointer to new dynamically allocated ns_change_t.
 * */
static ns_change_t * ns_change_load(sr_change_oper_t op,
                                    sr_val_t *old_val, sr_val_t *new_val);

/**
 * @brief Frees dynamic fields of elem and sets initial values of the struct.
 * @param elem Element to be freed
 * */
static inline void ns_change_free(ns_change_t **elem);

/**
 * @brief For each registered change in reg_chgs vector execute the
 *  change - stop/restart group or module.
 * @param sess Sysrepo session context for loading in case of restart action
 * @param reg_chgs Vector of registered changes
 * @return In case of restart action, it returns sr_error_t from
 *  configuration loading functions.
 * */
static inline int ns_change_proc_reg_chgs(sr_session_ctx_t *sess, vector_t *reg_chgs);

/**
 * @brief Handles SR_OP_CREATE operation for given change.
 * @details Assigns change action and adds it to vector via ns_change_add_new_change.
 * @see ns_change_add_new_change()
 * @param reg_chgs Vector of already registered changes
 * @param change New change to handle
 * */
static inline void ns_change_handle_create(ns_change_t *change);
static inline void ns_change_handle_delete(ns_change_t *change);
static inline void ns_change_handle_modify(ns_change_t *change);
/**
 * @brief  Adds n_change to reg_chgs vector or ignores the change in case it
 *  would be handled by some other change that is already in the vector.
 * @details TODO big time
 * @param reg_chgs Vector of already registered changes
 * @param n_change New change to add
 * */
static inline void ns_change_add_new_change(vector_t *reg_chgs,
                                            ns_change_t *n_change);

static char * ns_change_type_str(ns_change_type_t type);
static void ns_change_print(ns_change_t *change);
/* --END full fns prototypes-- */

/* --BEGIN superglobal fns-- */

int run_config_change_cb(sr_session_ctx_t *sess,
                         const char *sr_module_name,
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
   ns_change_t *change = NULL;
   vector_t reg_chgs; // Registered changes

   /* This should allocate enough space since not all groups, modules, insts
    *  configuration gets to reload at the same time */
   vector_init(&reg_chgs, mgrps_v.total + mods_v.total + insts_v.total);


   VERBOSE(V2, "Config change captured inside run_config_change_cb.")
   pthread_mutex_lock(&config_lock);

   VERBOSE(V3, "Getting changes iterator for %s", ns_root_sr_path)
   rc = sr_get_changes_iter(sess, ns_root_sr_path, &iter);
   if (rc != SR_ERR_OK) {
      VERBOSE(N_ERR, "Failed to get changes iterator: %s", sr_strerror(rc));
      goto err_cleanup;
   }

   rc = sr_get_change_next(sess, iter, &op, &old_val, &new_val);
   if (rc != SR_ERR_OK) {
      VERBOSE(V3, "%s", sr_strerror(rc))
   }
   while (rc == SR_ERR_OK) {
      VERBOSE(V3, "-----NEXT CHANGE LOOP-----")
      if (old_val != NULL) { VERBOSE(V3, " old_val=%s", old_val->xpath) }
      if (new_val != NULL) { VERBOSE(V3, " new_val=%s", new_val->xpath) }

      change = ns_change_load(op, old_val, new_val);
      if (change == NULL) {
         VERBOSE(N_ERR, "Failed to parse XPATH of change type %d", op)
         goto err_cleanup;
      }

      switch (op) {
         case SR_OP_CREATED:
            VERBOSE(V3, " -SR_OP_CREATED %s", ns_change_type_str(change->type));
            ns_change_handle_create(change);
            break;
         case SR_OP_MODIFIED:
            VERBOSE(V3, " -SR_OP_MODIFIED %s", ns_change_type_str(change->type));
            ns_change_handle_modify(change);
            break;
         case SR_OP_DELETED:
            VERBOSE(V3, " -SR_OP_DELETED %s", ns_change_type_str(change->type));
            ns_change_handle_delete(change);
            break;
         case SR_OP_MOVED:
            VERBOSE(V3, " -SR_OP_MOVED %s", ns_change_type_str(change->type));
            /* This operation is intentionally omitted, since it's quite
             * complex and not really required to be handled. */
            break;
         default:
            // This branch is here so that compiler doesn't complain
            break;
      }

      if (change->action != NS_CHE_ACTION_NONE) {
         ns_change_add_new_change(&reg_chgs, change);
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

   rc = ns_change_proc_reg_chgs(sess, &reg_chgs);
   if (rc != SR_ERR_OK) {
      goto err_cleanup;
   }

   VERBOSE(V2, "Successfully leaving change callback")
   pthread_mutex_unlock(&config_lock);
   return SR_ERR_OK;

err_cleanup:
   pthread_mutex_unlock(&config_lock);

   for (uint32_t i = 0; change = reg_chgs.items[i], i < reg_chgs.total; i++) {
      ns_change_free(&change);
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

/* --END superglobal fns-- */

/* --BEGIN local fns-- */

static inline int ns_change_replace_same_registered(ns_change_t *new, ns_change_t *reg)
{
   if (new->type == NS_CHE_T_INST && reg->type == NS_CHE_T_INST) {
      reg->action = new->action;
      reg->type = new->type;
      ns_change_free(&new);
      VERBOSE(V3, "New INSTANCE change updates already registred change")
      return 0;
   }

   if (new->type == NS_CHE_T_MOD && reg->type == NS_CHE_T_MOD) {
      reg->action = new->action;
      reg->type = new->type;
      ns_change_free(&new);
      VERBOSE(V3, "New MODULE change updates already registred change")
      return 0;
   }

   if (new->type == NS_CHE_T_GRP && reg->type == NS_CHE_T_GRP) {
      reg->action = new->action;
      reg->type = new->type;
      ns_change_free(&new);
      VERBOSE(V3, "New GROUP change updates already registred change")
      return 0;
   }

   // Replace of registred change was not possible, return error
   return -1;
}

static inline int ns_change_ignore_new(ns_change_t *new, ns_change_t *reg)
{
   if (reg->type > new->type) {
      VERBOSE(V3, "New change of %s is being ignored due to registered %s change",
              ns_change_type_str(new->type), ns_change_type_str(reg->type))
      ns_change_free(&new);
      return 0;
   }

   return -1;
}

static inline int ns_change_replace_other_registered(ns_change_t *new, ns_change_t *reg)
{
   if (reg->type < new->type) {
      reg->action = new->action;
      reg->type = new->type;

      VERBOSE(V3, "Replacing %s with %s", ns_change_type_str(reg->type),
              ns_change_type_str(new->type))
      if (reg->type > NS_CHE_T_INST) {
         NULLP_TEST_AND_FREE(reg->inst_name)
      }
      if (reg->type > NS_CHE_T_MOD) {
         NULLP_TEST_AND_FREE(reg->mod_name)
      }

      ns_change_free(&new);
      return 0;
   }

   return -1;
}

static inline void ns_change_add_new_change(vector_t *reg_chgs,
                                            ns_change_t *n_change)
{
   ns_change_t *r_change = NULL; // Already registered change

   FOR_EACH_IN_VEC_PTR(reg_chgs, r_change) {
      if (ns_change_replace_same_registered(n_change, r_change) == 0) {
            return;
      }
      if (ns_change_ignore_new(n_change, r_change) == 0) {
         return;
      }
      if (ns_change_replace_other_registered(n_change, r_change) == 0) {
         return;
      }
   }

   /* This new change is not registered yet and doesn't interfere with
    * already registered ones. Add it to vector of registered changes */
   vector_add(reg_chgs, n_change);
   VERBOSE(V3, "New change registered")
}

static inline void ns_change_handle_modify(ns_change_t *change)
{
   // TODO
   VERBOSE(V3, "-SR_OP_MODIFIED")
   switch (change->type) {
      case NS_CHE_T_GRP:
         change->action = NS_CHE_ACTION_RESTART;
         break;
      case NS_CHE_T_MOD:
         change->action = NS_CHE_ACTION_RESTART;
         break;
      case NS_CHE_T_INST:
         if (strcmp(change->node_name, "last-pid") == 0) {
            change->action = NS_CHE_ACTION_NONE;
         } else {
            change->action = NS_CHE_ACTION_RESTART;
         }
         break;
      default:
         /* Ignore everything else, though there is space for optimalization because
          *  it's not needed to restart whole group or module everytime */
         break;
   }
}

static inline void ns_change_handle_delete(ns_change_t *change)
{
   VERBOSE(V3, "-SR_OP_DELETED")
   change->action = NS_CHE_ACTION_DELETE;
   switch (change->type) {
      case NS_CHE_T_GRP:
         change->action = NS_CHE_ACTION_DELETE;
         break;
      case NS_CHE_T_MOD:
         change->action = NS_CHE_ACTION_DELETE;
         break;
      case NS_CHE_T_INST:
         if (strcmp(change->node_name, "last-pid") == 0) {
            change->action = NS_CHE_ACTION_NONE;
         } else {
            change->action = NS_CHE_ACTION_DELETE;
         }
         break;
      default:
         /* Ignore everything else, though there is space for optimalization because
          *  it's not needed to restart whole group or module everytime */
         break;
   }
}

static inline void ns_change_handle_create(ns_change_t *change)
{

   VERBOSE(V3, "  grp_name=%s", change->grp_name)
   VERBOSE(V3, "  mod_name=%s", change->mod_name)
   VERBOSE(V3, "  inst_name=%s", change->inst_name)

   switch (change->type) {
      case NS_CHE_T_GRP:
         change->action = NS_CHE_ACTION_RESTART;
         break;
      case NS_CHE_T_MOD:
         change->action = NS_CHE_ACTION_RESTART;
         break;
      case NS_CHE_T_INST:
         if (strcmp(change->node_name, "last-pid") == 0) {
            change->action = NS_CHE_ACTION_NONE;
         } else {
            change->action = NS_CHE_ACTION_RESTART;
         }
         break;
      default:
         /* Ignore everything else, though there is space for optimalization because
          *  it's not needed to restart whole group or module everytime */
         break;
   }
}

static inline int ns_change_proc_restart(sr_session_ctx_t *sess, ns_change_t * change)
{
   int rc = 0;

   switch (change->type) {
      case NS_CHE_T_INST:
         VERBOSE(V3, "Action restart for instance '%s'", change->inst_name)
         instance_stop_remove_by_name(change->inst_name);
         rc = instance_load_by_name(sess, change->grp_name,
                                    change->mod_name, change->inst_name);
         break;
      case NS_CHE_T_MOD:
         VERBOSE(V3, "Action restart for module '%s'", change->mod_name)
         module_stop_remove_by_name(change->mod_name);
         rc = module_load_by_name(sess, change->grp_name, change->mod_name);
         break;
      default: // NS_CHE_T_GRP
         VERBOSE(V3, "Action restart for group '%s'", change->grp_name)
         module_group_stop_remove_by_name(change->grp_name);
         rc = module_group_load_by_name(sess, change->grp_name);
         break;
   }

   return rc;
}

static inline void ns_change_proc_delete(sr_session_ctx_t *sess, ns_change_t *change)
{
   switch (change->type) {
      case NS_CHE_T_INST:
         VERBOSE(V3, "Stopping instance '%s'", change->inst_name)
         instance_stop_remove_by_name(change->inst_name);
         break;
      case NS_CHE_T_MOD:
         VERBOSE(V3, "Stopping module '%s'", change->mod_name)
         module_stop_remove_by_name(change->mod_name);
         break;
      default: // NS_CHE_T_GRP
         VERBOSE(V3, "Stopping module group '%s'", change->grp_name)
         module_group_stop_remove_by_name(change->grp_name);
         break;
   }
}

static inline int ns_change_proc_reg_chgs(sr_session_ctx_t *sess, vector_t *reg_chgs)
{
   int rc;
   ns_change_t *change;

   VERBOSE(V3, "Processing %d registered changes", reg_chgs->total)
   // Go from back so that if there is error, not processed elements get freed
   for (long i = reg_chgs->total - 1; i >= 0; i--) {
      change = reg_chgs->items[i];
      if (change->action == NS_CHE_ACTION_RESTART) {
         rc = ns_change_proc_restart(sess, change);

         if (rc != SR_ERR_OK) {
            VERBOSE(N_ERR, "Failed to restart module-group=%s/module=%s/instance=%s/",
                    change->grp_name, change->mod_name, change->inst_name)
            reg_chgs->total = (uint32_t) (i == 0 ? 0 : i) ;
            ns_change_free(&change);
            return rc;
         }
      } else if (change->action == NS_CHE_ACTION_DELETE) {
         ns_change_proc_delete(sess, change);
      } else {
         VERBOSE(V3, "Change ignored")
      }

      ns_change_free(&change);
   }
   vector_free(reg_chgs);

   return SR_ERR_OK;
}

static ns_change_t * ns_change_load(sr_change_oper_t op,
                                    sr_val_t *old_val, sr_val_t *new_val)
{
   char *xpath;
   sr_xpath_ctx_t state = {0};
   char *res = NULL;
   char *next_node = NULL;
   ns_change_t *change = NULL;

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
         break;
      default:
         xpath = NULL;
   }

   change = (ns_change_t *) calloc(1, sizeof(ns_change_t));
   if (change == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }
   change->action = NS_CHE_ACTION_NONE;

   res = sr_xpath_next_node(xpath, &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to parse ns_change on line %d", __LINE__)
      goto err_cleanup;
   }

   res = sr_xpath_next_node(NULL, &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to parse ns_change on line %d", __LINE__)
      goto err_cleanup;
   }

   res = sr_xpath_node_key_value(NULL, "name", &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to parse ns_change on line %d", __LINE__)
      goto err_cleanup;
   }
   change->grp_name = strdup(res);
   if (change->grp_name == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   res = sr_xpath_next_node(NULL, &state);
   if (res == NULL) {
      change->type = NS_CHE_T_GRP;
      sr_xpath_recover(&state);
      return change;
   }
   next_node = strdup(res);
   if (next_node == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   { // Load module list key name or group node name
      res = sr_xpath_node_key_value(NULL, "name", &state);
      if (res == NULL) {
         change->type = NS_CHE_T_GRP;
         change->node_name = next_node;
         sr_xpath_recover(&state);
         return change;
      }
      change->mod_name = strdup(res);
      if (change->mod_name == NULL) {
         NO_MEM_ERR
         goto err_cleanup;
      }
      NULLP_TEST_AND_FREE(next_node)
   }

   res = sr_xpath_next_node(NULL, &state);
   if (res == NULL) {
      change->type = NS_CHE_T_MOD;
      sr_xpath_recover(&state);
      return change;
   }
   next_node = strdup(res);
   if (next_node == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   { // Load instance list key name or module node name
      res = sr_xpath_node_key_value(NULL, "name", &state);
      if (res == NULL) {
         change->type = NS_CHE_T_MOD;
         change->node_name = next_node;
         sr_xpath_recover(&state);
         return change;
      }
      change->inst_name = strdup(res);
      if (change->inst_name == NULL) {
         NO_MEM_ERR
         goto err_cleanup;
      }
      NULLP_TEST_AND_FREE(next_node)
   }

   res = sr_xpath_next_node(NULL, &state);
   if (res == NULL) {
      change->type = NS_CHE_T_INST;
      sr_xpath_recover(&state);
      return change;
   }
   next_node = strdup(res);
   if (next_node == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }


   change->type = NS_CHE_T_INST;
   change->node_name = next_node;
   sr_xpath_recover(&state);


   return change;

err_cleanup:
   ns_change_free(&change);
   sr_xpath_recover(&state);
   VERBOSE(N_ERR, "Failed while parsing XPATH=%s", xpath)

   return NULL;
}

static inline void ns_change_free(ns_change_t **elem)
{
   NULLP_TEST_AND_FREE((*elem)->grp_name)
   NULLP_TEST_AND_FREE((*elem)->mod_name)
   NULLP_TEST_AND_FREE((*elem)->inst_name)
   NULLP_TEST_AND_FREE((*elem)->node_name)
   NULLP_TEST_AND_FREE((*elem))
}

static char * ns_change_type_str(ns_change_type_t type)
{
   switch(type) {
      case NS_CHE_T_GRP:
         return "GROUP NODE";
      case NS_CHE_T_MOD:
         return "MODULE NODE";
      case NS_CHE_T_INST:
         return "INSTANCE NODE";
      default: // NS_CHE_T_INVAL
         return "INVALID";
   }
}

static void ns_change_print(ns_change_t *change)
{
   VERBOSE(V3, "(%s)grp[%s]/mod[%s]/inst[%s]/%s", ns_change_type_str(change->type),
           change->grp_name, change->mod_name, change->inst_name, change->node_name)
}
/* --END local fns-- */
