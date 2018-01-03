#include <string.h>
#include <sysrepo/xpath.h>
#include "conf.h"
#include "utils.h"
#include "module.h"

/*--BEGIN superglobal vars--*/
bool daemon_flag = false; ///< CLI startup option to tell whether to start as daemon
char *logs_path = NULL; ///< Path to where logs directory should reside
extern const char * ns_root_sr_path;
/*--END superglobal vars--*/

/*--BEGIN local #define--*/

#define if_node_fetch_str(node, node_name, dest) if (1) { \
   if (strcmp((node)->name, (node_name)) == 0) { \
      (dest) = strdup((node)->data.string_val); \
      if ((dest) == NULL) { \
         NO_MEM_ERR \
         return -1; \
      } \
      (node) = (node)->next; \
      continue; \
   } \
}

#define if_node_fetch_bool(node, node_name, dest) if (1) { \
   if (strcmp((node)->name, node_name) == 0) { \
      (dest) = (node)->data.bool_val ? true : false; \
      (node) = (node)->next; \
      continue; \
   } \
}

#define if_node_fetch_uint8(node, node_name, dest) if (1) { \
   if (strcmp((node)->name, node_name) == 0) { \
      (dest) = (node)->data.uint8_val; \
      (node) = (node)->next; \
      continue; \
   } \
}

#define if_node_fetch_uint16(node, node_name, dest) if (1) { \
   if (strcmp((node)->name, node_name) == 0) { \
      (dest) = (node)->data.uint16_val; \
      (node) = (node)->next; \
      continue; \
   } \
}

#define if_node_fetch_uint32(node, node_name, dest) if (1) { \
   if (strcmp((node)->name, node_name) == 0) { \
      (dest) = (node)->data.uint32_val; \
      (node) = (node)->next; \
      continue; \
   } \
}

#define if_node_dig_down(node, node_name, fn, stru) if (1) { \
   if (strcmp((node)->name, (node_name)) == 0 ) { \
      if ((node)->first_child != NULL) { \
         if ((fn)((node)->first_child, stru) != 0) { \
            return -1; \
         } \
         (node) = (node)->next; \
         continue; \
      } \
   } \
}

#define next_sr_node(node) { next_iter = false; (node) = (node)->next; }

/*--END local #define--*/

/*--BEGIN local typedef--*/


/*--END local typedef--*/

/* --BEGIN local vars-- */
/* --END local vars-- */

/* --BEGIN full fns prototypes-- */

/**
 * @brief Loads module group configuration from sysrepo.
 * @details Loads content of all siblings and descendants of group_node into
 *  module_group_t and pushes new structure into m_groups_ll. Parses module-group
 *  in YANG schema.
 * @param sess Sysrepo session to use
 * @param group Module group structure to load configuration data to
 * @param node First of leaves or nodes of loaded group.
 * */
static int module_group_load(sr_session_ctx_t *sess,
                             module_group_t *group,
                             sr_node_t *node);

/**
 * TODO
 * */
static int module_load(sr_session_ctx_t *sess,
                       module_t *mod,
                       module_group_t *grp,
                       sr_node_t *node);
/**
 * @brief Same thing as load_module_group but for modules.
 * @details On top of the same functionality as load_module_group but for modules,
 *  it links new module to group given by parameter group. Parses module container
 *  in YANG schema.
 * @see load_module_group(sr_node_t *group_node)
 * @param sess Sysrepo session to use
 * @param inst Module structure to load configuration to
 * @param node First of leaves or nodes of loaded module.
 * @param mod Group to which new module will be linked.
 * */
static int instance_load(sr_session_ctx_t *sess, instance_t *inst, module_t *mod,
                         sr_node_t *node);

/**
 * @brief Loads new interface_t and if successful assigns it to given module.
 * @details Parses interface container in YANG schema.
 * @see load_module_group(sr_node_t *group_node)
 * @param node
 * @param inst Module to which the new interface belongs to.
 * */
static int interface_load(sr_node_t *node, instance_t *inst);

/**
 * TODO
 * */
static inline int interface_tcp_params_load(sr_node_t *node, interface_t *ifc);

/**
 * TODO
 * */
static inline int interface_tcp_tls_params_load(sr_node_t *node, interface_t *ifc);

/**
 * TODO
 * */
static inline int interface_unix_params_load(sr_node_t *node, interface_t *ifc);

/**
 * TODO
 * */
static inline int interface_file_params_load(sr_node_t *node, interface_t *ifc);

/**
 * @brief Restores PID for already running module.
 * @details When supervisor exits and keeps modules running, PIDs of running modules are
 *           stored inside last_pid node in sysrepo. This function stores this PID inside
 *           module structure in case the process of same exec path is running.
 * @param last_pid Last PID detected for this module
 * @param inst Module for which last_pid should be reassigned
 * @param sess Sysrepo session to use for last_pid node removal
 * */
static void instance_pid_restore(pid_t last_pid,
                                 instance_t *inst,
                                 sr_session_ctx_t *sess);

/* --END full fns prototypes-- */

/* --BEGIN superglobal fns-- */


int ns_config_load(sr_session_ctx_t *sess)
{
   int rc;
   sr_node_t *tree = NULL;
   sr_node_t *node = NULL;

   rc = sr_get_subtree(sess, ns_root_sr_path, SR_GET_SUBTREE_DEFAULT, &tree);

   if (rc != SR_ERR_OK) {
      VERBOSE(N_ERR, "Failed to fetch sysrepo config tree: %s", sr_strerror(rc))
      sr_free_tree(tree);
      return -1;
   }

   node = tree;

   //init_if_node_loop_iter, not needed this time
   // Root level
   if (node->first_child == NULL) {
      VERBOSE(N_ERR, "Empty nemea config tree")
      sr_free_tree(tree);
      return -1;
   }

   // Descend from root to module groups root level
   node = node->first_child;

   // Module group roots level
   while (node != NULL) {
      if (node->first_child != NULL) {
         if (module_group_load(sess, NULL, node->first_child) != 0) {
            sr_free_tree(tree);
            return -1;
         }
      }
      node = node->next;
   }

   sr_free_tree(tree);

   return 0;
}

int module_group_load_by_name(sr_session_ctx_t *sess, const char *name)
{

   int rc;
   sr_node_t *node = NULL;
   char * xpath = NULL;

   xpath = (char *) calloc(1, sizeof(char) * (strlen(ns_root_sr_path) + 46));
   IF_NO_MEM_INT_ERR(xpath)
   sprintf(xpath, "%s/module-group[name='%s']", ns_root_sr_path, name);

   rc = sr_get_subtree(sess, xpath, SR_GET_SUBTREE_DEFAULT, &node);
   if (rc != SR_ERR_OK) {
      VERBOSE(N_ERR, "Failed to load new module group configuration subtree for "
            "XPATH '%s'. Sysrepo error: %s", xpath, sr_strerror(rc))
      return rc;
   }

   if (node == NULL || node->first_child == NULL) {
      VERBOSE(N_ERR, "Failed to load new module group configuration for "
            "group '%s'. Node is missing in sysrepo tree.", name)
      sr_free_tree(node);
      return SR_ERR_DATA_MISSING;
   }

   rc = module_group_load(sess, NULL, node->first_child);
   sr_free_tree(node);
   if (rc != 0) {
      return rc;
   }

   return SR_ERR_OK;
}

int module_load_by_name(sr_session_ctx_t *sess,
                        const char *group_name,
                        const char *module_name)
{
   int rc;
   sr_node_t *node = NULL;
   char * xpath = NULL;
   uint32_t xpath_len = (uint32_t) (strlen(ns_root_sr_path)
                                    + strlen(group_name) + strlen(module_name) + 39);

   xpath = (char *) calloc(1,sizeof(char) * (xpath_len));
   IF_NO_MEM_INT_ERR(xpath)
   sprintf(xpath, "%s/module-group[name='%s']/module[name='%s']",
           ns_root_sr_path, group_name, module_name);

   rc = sr_get_subtree(sess, xpath, SR_GET_SUBTREE_DEFAULT, &node);
   if (rc != SR_ERR_OK) {
      VERBOSE(N_ERR, "Failed to load new module configuration subtree for"
            " XPATH '%s'. Sysrepo error: %s", xpath, sr_strerror(rc))
      return rc;
   }

   if (node == NULL || node->first_child == NULL) {
      VERBOSE(N_ERR, "Failed to load new module configuration for"
            " XPATH '%s'. Node is missing in sysrepo tree.", xpath)
      if (node != NULL) {
         sr_free_tree(node);
      }
      return SR_ERR_DATA_MISSING;
   }

   module_group_t *group = module_group_get_by_name(group_name, NULL);
   if (group == NULL) {
      // Can't think of case it would happen, but one can not know...
      VERBOSE(V3, "Failed to load new module configuration for XPATH '%s'."
            " Module group is not loaded in supervisor.", xpath)
      return SR_ERR_DATA_MISSING;
   }

   rc = module_load(sess, NULL, group, node->first_child);
   sr_free_tree(node);
   if (rc != 0) {
      VERBOSE(N_ERR, "Failed to load new module configuration from fetched"
            " sysrepo subtree")
      return rc;
   }

   return SR_ERR_OK;
}

int instance_load_by_name(sr_session_ctx_t *sess,
                        const char *group_name,
                        const char *module_name,
                          const char *inst_name)
{
   int rc;
   sr_node_t *node = NULL;
   char * xpath = NULL;
   uint32_t xpath_len = (uint32_t) (strlen(ns_root_sr_path)
                                    + strlen(group_name) + strlen(module_name)
                                    + strlen(inst_name) + 57);


   xpath = (char *) calloc(1, sizeof(char) * (xpath_len));
   IF_NO_MEM_INT_ERR(xpath)
   sprintf(xpath, "%s/module-group[name='%s']/module[name='%s']/instance[name='%s']",
           ns_root_sr_path, group_name, module_name, inst_name);

   rc = sr_get_subtree(sess, xpath, SR_GET_SUBTREE_DEFAULT, &node);
   if (rc != SR_ERR_OK) {
      VERBOSE(N_ERR, "Failed to load new instance configuration subtree for"
            " XPATH '%s'. Sysrepo error: %s", xpath, sr_strerror(rc))
      return rc;
   }

   if (node == NULL || node->first_child == NULL) {
      VERBOSE(N_ERR, "Failed to load new module configuration for"
            " XPATH '%s'. Node is missing in sysrepo tree.", xpath)
      if (node != NULL) {
         sr_free_tree(node);
      }
      return SR_ERR_DATA_MISSING;
   }

   module_t *module = module_get_by_name(module_name, NULL);
   if (module == NULL) {
      // Can't think of case it would happen, but one can not know...
      VERBOSE(V3, "Failed to load new module configuration for XPATH '%s'."
            " Module group is not loaded in supervisor.", xpath)
      return SR_ERR_DATA_MISSING;
   }

   rc = instance_load(sess, NULL, module, node->first_child);
   sr_free_tree(node);

   if (rc != 0) {
      VERBOSE(N_ERR, "Failed to load new module configuration from fetched"
            " sysrepo subtree")
      return rc;
   }

   return SR_ERR_OK;

}
/* --END superglobal fns-- */

/* --BEGIN local fns-- */

/*static int load_module_group2(sr_session_ctx_t *sess, module_group_t *group, char * name)
{
   int rc;
   char * xpath = NULL;
   sr_val_t *val = NULL;
   sr_val_iter_t *iter = NULL;
   char * grp_root_xpath = "%s/module-group[name='%s']/";

   if (group == NULL) {
      group = module_group_alloc();
      if (group == NULL) {
         NO_MEM_ERR
         rc = SR_ERR_NOMEM;
         goto err_cleanup;
      }

      module_group_add(group);
   }

   {
      xpath = calloc(1, sizeof(strlen(ns_root_sr_path) + strlen(name) + 31));
      sprintf(xpath, "%s/module-group[name='%s']/enabled", name);

      rc = sr_get_item(sess, xpath, &val);

      // TODO macro
      if (rc != SR_ERR_OK) {
         VERBOSE(N_ERR, "Failed to fetch value at xpath=%s", xpath)
         goto err_cleanup;
      }
      group->enabled = val->data.bool_val;
      NULLP_TEST_AND_FREE(xpath)
      sr_free_val(val);
   }

   {
      xpath = calloc(1, sizeof(strlen(ns_root_sr_path) + strlen(name) + 34));
      sprintf(xpath, "%s/module-group[name='%s']/module/name", name);

      rc = sr_get_items_iter(sess, xpath, &iter);
      while ((rc = sr_get_item(sess, xpath, &val)) == SR_ERR_OK) {
         VERBOSE(DEBUG, "module=%s", val->data.string_val)
         // pass to module
         sr_free_val(val);
      }
      VERBOSE(DEBUG, "rc=%d, %s", rc, sr_strerror(rc))
      // TODO find error type
      //if (rc != SR_ERR_)

      sr_free_val(val);
      sr_free_val_iter(val);
      NULLP_TEST_AND_FREE(xpath)
   }

   // load module names

err_cleanup:
   NULLP_TEST_AND_FREE(xpath)
   if (iter != NULL) {
      sr_free_val_iter(iter);
   }

   return rc;
}*/

static int module_group_load(sr_session_ctx_t *sess,
                             module_group_t *group,
                             sr_node_t *node)
{
   int rc;

   if (group == NULL) {
      group = module_group_alloc();
      IF_NO_MEM_INT_ERR(group)

      if (module_group_add(group) != 0) {
         NO_MEM_ERR
         return -1;
      }
   }

   while (node != NULL) {
      if_node_fetch_str(node, "name", group->name)
      if_node_fetch_bool(node, "enabled", group->enabled)

      if (strcmp(node->name, "module") == 0 && node->first_child != NULL) {
         rc = module_load(sess, NULL, group, node->first_child);
         if (rc != 0) {
            module_group_remove_by_name(group->name);
            module_group_free(group);
            return rc;
         }
      }

      node = node->next;
   }

   return 0;
}

static int module_load(sr_session_ctx_t *sess,
                       module_t *mod,
                       module_group_t *grp,
                       sr_node_t *node)
{
   int rc;

   if (mod == NULL) {
      mod = module_alloc();
      IF_NO_MEM_INT_ERR(mod)

      mod->group = grp;
      if (module_add(mod) != 0) {
         NO_MEM_ERR
         return -1;
      }
   }

   while(node != NULL) {
      if_node_fetch_str(node, "name", mod->name)
      if_node_fetch_str(node, "path", mod->path)

      if (strcmp(node->name, "instance") == 0 && node->first_child != NULL) {
         rc = instance_load(sess, NULL, mod, node->first_child);
         if (rc != 0) {
            module_remove_by_name(mod->name);
            module_free(mod);
            return rc;
         }
      }

      node = node->next;
   }

   return 0;
}

static int instance_load(sr_session_ctx_t *sess,
                         instance_t *inst,
                         module_t *mod,
                         sr_node_t *node)
{
   int rc;
   pid_t last_pid = 0;

   if (inst == NULL) {
      inst = instance_alloc();
      IF_NO_MEM_INT_ERR(inst)

      inst->module = mod;
      if (instance_add(inst) != 0) {
         NO_MEM_ERR
         return -1;
      }
   }

   while (node != NULL) {
      if_node_fetch_str(node, "name", inst->name)
      if_node_fetch_str(node, "params", inst->params)
      if_node_fetch_bool(node, "enabled", inst->enabled)
      if_node_fetch_uint8(node, "max-restarts-per-min", inst->max_restarts_minute)
      if_node_fetch_uint32(node, "last-pid", last_pid)

      if (strcmp(node->name, "interface") == 0 && node->first_child != NULL) {
         rc = interface_load(node->first_child, inst);
         // TODO rc error handling
      }

      node = node->next;
   }

   rc = instance_gen_exec_args(inst);
   if (rc != 0) {
      /* hopefully this would be removed in the future due to sysrepo per
       * module configuration */
      VERBOSE(N_ERR, "Failed to prepare exec_args")
      return -1;
   }

   if (last_pid > 0) {
      VERBOSE(V3, "Restoring PID=%d for (%s)", last_pid, instance_tree_path(inst))
      instance_pid_restore(last_pid, inst, sess);
   }

   return 0;
}

static int interface_load(sr_node_t *node, instance_t *inst)
{
   sr_node_t *node_iter;
   interface_t *ifc;


   ifc = interface_alloc();
   IF_NO_MEM_INT_ERR(ifc)

   /*
    * Determine interface type first before fetching more data to guarantee
    * knowledge of the interface type.
    * */
   node_iter = node;
   while (node_iter != NULL) {
      if (strcmp(node_iter->name, "type") == 0) {
         if (strcmp(node_iter->data.enum_val, "TCP") == 0) {
            ifc->type = NS_IF_TYPE_TCP;
            break;
         } else if (strcmp(node_iter->data.enum_val, "TCP-TLS") == 0) {
            ifc->type = NS_IF_TYPE_TCP_TLS;
            break;
         } else if (strcmp(node_iter->data.enum_val, "UNIXSOCKET") == 0) {
            ifc->type = NS_IF_TYPE_UNIX;
            break;
         } else if (strcmp(node_iter->data.enum_val, "FILE") == 0) {
            ifc->type = NS_IF_TYPE_FILE;
            break;
         } else if (strcmp(node_iter->data.enum_val, "BLACKHOLE") == 0) {
            ifc->type = NS_IF_TYPE_BH;
            break;
         } else {
            VERBOSE(N_ERR, "Invalid interface type found! Supervisor version is "
                  "possibly not matching configuration schema version.")
            return -1;
         }
      }
      node_iter = node_iter->next;
   }

   if (interface_specific_params_alloc(ifc) != 0) {
      return -1;
   }
   
   // Browse nodes by passed node once again
   node_iter = node;
   while (node_iter != NULL) {
      if_node_fetch_str(node_iter, "name", ifc->name)
      if_node_fetch_str(node_iter, "timeout", ifc->timeout)
      if_node_fetch_str(node_iter, "buffer", ifc->buffer)
      if_node_fetch_str(node_iter, "autoflush", ifc->autoflush)

      if_node_dig_down(node_iter, "tcp-params", interface_tcp_params_load, ifc)
      if_node_dig_down(node_iter, "tcp-tls-params", interface_tcp_tls_params_load, ifc)
      if_node_dig_down(node_iter, "unix-params", interface_unix_params_load, ifc)
      if_node_dig_down(node_iter, "file-params", interface_file_params_load, ifc)

      if (strcmp(node_iter->name, "direction") == 0) {
         if (strcmp(node_iter->data.enum_val, "IN") == 0) {
            ifc->direction = NS_IF_DIR_IN;
         } else if (strcmp(node_iter->data.enum_val, "OUT") == 0) {
            ifc->direction = NS_IF_DIR_OUT;
         } else {
            VERBOSE(N_ERR, "Invalid interface direction found! Supervisor version is "
                  "possibly not matching configuration schema version.")
            return -1;
         }
      }
      node_iter = node_iter->next;
   }

   // Direction should be allocated by now, it's safe to allocate stats structs
   if (interface_stats_alloc(ifc) != 0) {
      NO_MEM_ERR
      return -1; // TODO cleanup???
   }

   if (instance_interface_add(inst, ifc) != 0) {
      NO_MEM_ERR
      return -1;
   }

   return 0;
}

static inline int interface_tcp_params_load(sr_node_t *node, interface_t *ifc)
{
   while (node != NULL) {
      if_node_fetch_str(node, "host", ifc->specific_params.tcp->host)
      if_node_fetch_uint16(node, "port", ifc->specific_params.tcp->port)
      if_node_fetch_uint16(node, "max-clients", ifc->specific_params.tcp->max_clients)
      node = node->next;
   }
   return 0;
}

static inline int interface_tcp_tls_params_load(sr_node_t *node, interface_t *ifc)
{
   while (node != NULL) {
      if_node_fetch_str(node, "host", ifc->specific_params.tcp_tls->host)
      if_node_fetch_uint16(node, "port", ifc->specific_params.tcp_tls->port)
      if_node_fetch_uint16(node, "max-clients", ifc->specific_params.tcp_tls->max_clients)
      if_node_fetch_str(node, "keyfile", ifc->specific_params.tcp_tls->keyfile)
      if_node_fetch_str(node, "certfile", ifc->specific_params.tcp_tls->certfile)
      if_node_fetch_str(node, "cafile", ifc->specific_params.tcp_tls->cafile)
      node = node->next;
   }

   return 0;
}

static inline int interface_unix_params_load(sr_node_t *node, interface_t *ifc)
{
   while (node != NULL) {
      if_node_fetch_str(node, "socket-name", ifc->specific_params.nix->socket_name)
      if_node_fetch_uint16(node, "max-clients", ifc->specific_params.nix->max_clients)
      node = node->next;
   }

   return 0;
}

static inline int interface_file_params_load(sr_node_t *node, interface_t *ifc)
{
   while (node != NULL) {
      if_node_fetch_str(node, "name", ifc->specific_params.file->name)
      if_node_fetch_str(node, "mode", ifc->specific_params.file->mode)
      if_node_fetch_uint16(node, "time", ifc->specific_params.file->time)
      if_node_fetch_uint16(node, "size", ifc->specific_params.file->size)
      node = node->next;
   }

   return 0;
}

static void instance_pid_restore(pid_t last_pid, instance_t *inst, sr_session_ctx_t *sess)
{
   int rc;
   //pid_t result;
   size_t pid_xpath_len;
   char *pid_xpath;

   rc = kill(last_pid, 0);
   //result = waitpid(last_pid, NULL, WNOHANG);
   if (rc != 0) {
      goto clean_pid;
   }

   // Process is running
   char exe_file[1024]; // It's way too much
   memset(exe_file, 0, 1024);
   sprintf(exe_file, "/proc/%d/exe", last_pid);

   char run_path[4096]; // Path of process that runs under PID last_pid
   memset(run_path, 0, 4096);

   if (readlink(exe_file, run_path, sizeof(run_path) -1) == -1) {
      goto clean_pid;
   }


   if (strcmp(run_path, inst->module->path) == 0) {
      // Process under PID last_pid is really this inst
      inst->pid = last_pid;
      inst->running = true;
      inst->is_my_child = false;
   }

   goto clean_pid;

clean_pid:
   /* Try to remove last_pid node from sysrepo but we don't really
    *  care if it fails */
   pid_xpath_len = strlen(ns_root_sr_path)
                   + strlen(inst->module->group->name)
                   + strlen(inst->module->name)
                   + strlen(inst->name) + 66;
   pid_xpath = (char *) calloc(1, sizeof(char) * (pid_xpath_len));
   if (pid_xpath != NULL) {
      sprintf(pid_xpath,
              "%s/module-group[name='%s']/module[name='%s']/instance[name='%s']/last-pid",
              ns_root_sr_path,
              inst->module->group->name,
              inst->module->name,
              inst->name);
      rc = sr_delete_item(sess, pid_xpath, SR_EDIT_NON_RECURSIVE);
      if (rc == SR_ERR_OK) {
         sr_commit(sess);
      }
      NULLP_TEST_AND_FREE(pid_xpath)
   }
}
/* --END local fns-- */
