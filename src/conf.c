#include <string.h>
#include <sysrepo/xpath.h>
#include "conf.h"
#include "utils.h"
#include "module.h"

/*--BEGIN superglobal vars--*/
extern const char * ns_root_sr_path;
/*--END superglobal vars--*/

/*--BEGIN local #define--*/

#define FOUND_AND_ERR(rc) ((rc) != SR_ERR_NOT_FOUND && (rc) != SR_ERR_OK)

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
 * *//*
static int module_group_load(sr_session_ctx_t *sess,
                             module_group_t *group,
                             sr_node_t *node);

*//**
 * TODO
 * *//*
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
static int
run_module_load(sr_session_ctx_t *sess, char *xpath, run_module_t *inst);

static int
av_module_load(sr_session_ctx_t *sess, char *xpath, av_module_t *amod);

/**
 * @brief Loads new interface_t and if successful assigns it to given module.
 * @details Parses interface container in YANG schema.
 * @see load_module_group(sr_node_t *group_node)
 * @param sess
 * @param inst Module to which the new interface belongs to.
 * */
static int
interface_load(sr_session_ctx_t *sess, char *xpath, run_module_t *inst);


// TODO
static inline int interface_tcp_params_load(sr_session_ctx_t *sess, char *base_xpath,
                                            interface_t *ifc);

/**
 * TODO
 * */
static inline int
interface_tcp_tls_params_load(sr_session_ctx_t *sess, char *base_xpath, interface_t *ifc);

/**
 * TODO
 * */
static inline int
interface_unix_params_load(sr_session_ctx_t *sess, char *base_xpath, interface_t *ifc);

/**
 * TODO
 * */
static inline int
interface_file_params_load(sr_session_ctx_t *sess, char *base_xpath, interface_t *ifc);

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
                                 run_module_t *inst,
                                 sr_session_ctx_t *sess);


static int load_sr_str(sr_session_ctx_t *sess, char *base_xpath, char *node_xpath,
                       char **where);
static int load_sr_num(sr_session_ctx_t *sess, char *base_xpath, char *node_xpath,
                       void *where, sr_type_t data_type);
/* --END full fns prototypes-- */

/* --BEGIN superglobal fns-- */


int ns_startup_config_load(sr_session_ctx_t *sess)
{
   int rc;
   sr_val_t *vals = NULL;
   size_t vals_cnt = 0;
   char av_mods_xpath[] = NS_ROOT_XPATH"/available-module";
   char run_mods_xpath[] = NS_ROOT_XPATH"/module";

   { // load /available-modules
      rc = sr_get_items(sess, av_mods_xpath, &vals, &vals_cnt);
      if (rc != SR_ERR_OK) {
         VERBOSE(N_ERR, "Failed to load %s. Error: %s", av_mods_xpath, sr_strerror(rc))
         goto err_cleanup;
      }

      for (int i = 0; i < vals_cnt; i++) {
         rc = av_module_load(sess, vals[i].xpath, NULL);
         if (rc != SR_ERR_OK) {
            goto err_cleanup;
         }
      }
      sr_free_values(vals, vals_cnt);
      vals = NULL;
      vals_cnt = 0;
   }


   { // load /modules
      rc = sr_get_items(sess, run_mods_xpath, &vals, &vals_cnt);
      if (rc != SR_ERR_OK) {
         VERBOSE(N_ERR, "Failed to load %s. Error: %s", av_mods_xpath, sr_strerror(rc))
         goto err_cleanup;
      }

      for (int i = 0; i < vals_cnt; i++) {
         rc = run_module_load(sess, vals[i].xpath, NULL);
         if (rc != SR_ERR_OK) {
            goto err_cleanup;
         }
      }
      sr_free_values(vals, vals_cnt);
      vals = NULL;
      vals_cnt = 0;
   }

   return 0;

err_cleanup:
   if (vals != NULL && vals_cnt != 0) {
      sr_free_values(vals, vals_cnt);
   }
   VERBOSE(N_ERR, "Failed to load startup configuration.")

   return rc;
}

/*int module_group_load_by_name(sr_session_ctx_t *sess, const char *name)
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
}*/

/*int module_load_by_name(sr_session_ctx_t *sess,
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
}*/

/*int instance_load_by_name(sr_session_ctx_t *sess,
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

   rc = run_module_load(sess, NULL, module, node->first_child);
   sr_free_tree(node);

   if (rc != 0) {
      VERBOSE(N_ERR, "Failed to load new module configuration from fetched"
            " sysrepo subtree")
      return rc;
   }

   return SR_ERR_OK;

}*/
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

/*static int module_group_load(sr_session_ctx_t *sess,
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
         rc = run_module_load(sess, NULL, mod, node->first_child);
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
*/
static int
run_module_load(sr_session_ctx_t *sess, char *xpath, run_module_t *inst)
{
   int rc;
   pid_t last_pid = 0;
   char *mod_kind = NULL;
   char *ifc_xpath = NULL;
   size_t ifc_cnt = 0;
   sr_val_t *ifces = NULL;

   if (inst == NULL) {
      inst = run_module_alloc();
      IF_NO_MEM_INT_ERR(inst)

      if (vector_add(&rnmods_v, inst) != 0) {
         NO_MEM_ERR
         return SR_ERR_NOMEM;
      }
   }

   rc = load_sr_str(sess, xpath, "/name", &(inst->name));
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_str(sess, xpath, "/module-kind", &mod_kind);
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_num(sess, xpath, "/enabled", &(inst->enabled), SR_BOOL_T);
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }

   rc = load_sr_num(sess, xpath, "/max-restarts-per-minute",
                    &(inst->max_restarts_minute), SR_UINT8_T);
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_str(sess, xpath, "/params", &(inst->params));
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_num(sess, xpath, "/last-pid", &last_pid, SR_UINT32_T);
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }

   { // load interfaces
      ifc_xpath = calloc(1, (strlen(xpath) + 11) * sizeof(char));
      if (ifc_xpath == NULL) {
         NO_MEM_ERR
         goto err_cleanup;
      }
      sprintf(ifc_xpath, "%s/interface", xpath);

      rc = sr_get_items(sess, ifc_xpath, &ifces, &ifc_cnt);
      if (rc != SR_ERR_OK) {
         // TODO msg
         goto err_cleanup;
      }

      for (size_t i = 0; i < ifc_cnt; i++) {
         interface_load(sess, ifces[i].xpath, inst);
      }
      sr_free_values(ifces, ifc_cnt);
   }

   rc = module_gen_exec_args(inst);
   if (rc != 0) {
      /* hopefully this would be removed in the future due to sysrepo per
       * module configuration */
      VERBOSE(N_ERR, "Failed to prepare exec_args")
      NULLP_TEST_AND_FREE(mod_kind)
      return SR_ERR_NOMEM;
   }



   if (last_pid > 0) {
      VERBOSE(V3, "Restoring PID=%d for %s", last_pid, inst->name)
      // TODO TOD TOTOD
      //instance_pid_restore(last_pid, inst, sess);
   }

   return SR_ERR_OK;

err_cleanup:
   // TODO
   if (ifces != NULL) {
      sr_free_values(ifces, ifc_cnt);
   }

   return rc;
}

static int
av_module_load(sr_session_ctx_t *sess, char *xpath, av_module_t *amod)
{
   if (amod == NULL) {
      amod = av_module_alloc();
      IF_NO_MEM_INT_ERR(amod)

      if (vector_add(&avmods_v, amod) != 0) {
         NO_MEM_ERR
         return -1;
      }
   }

   int rc;

   rc = load_sr_str(sess, xpath, "/name", &(amod->name));
   if (rc != 0) {
      goto err_cleanup;
   }
   rc = load_sr_str(sess, xpath, "/path", &(amod->path));
   if (rc != 0) {
      goto err_cleanup;
   }
   rc = load_sr_num(sess, xpath, "/is-nemea-module", &(amod->is_nemea), SR_BOOL_T);
   if (rc != 0) {
      goto err_cleanup;
   }
   rc = load_sr_num(sess, xpath, "/is-sysrepo-ready", &(amod->is_sr_en), SR_BOOL_T);
   if (rc != 0) {
      goto err_cleanup;
   }

   return 0;

err_cleanup:
   VERBOSE(N_ERR, "Failed to load module from xpath %s", xpath)
   return -1;
}

static int
interface_load(sr_session_ctx_t *sess, char *xpath, run_module_t *inst)
{
   int rc;
   interface_t *ifc;
   char *ifc_type = NULL;
   char *ifc_dir = NULL;


   ifc = interface_alloc();
   IF_NO_MEM_INT_ERR(ifc)


   rc = load_sr_str(sess, xpath, "/name", &(ifc->name));
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }

   { // load interface type and type specific parameters
      rc = load_sr_str(sess, xpath, "/type", &ifc_type);
      if (FOUND_AND_ERR(rc)) {
         goto err_cleanup;
      }
      if (strcmp(ifc_type, "TCP") == 0) {
         ifc->type = NS_IF_TYPE_TCP;
         if (interface_specific_params_alloc(ifc) != 0) {
            return SR_ERR_NOMEM;
         }
         rc = interface_tcp_params_load(sess, xpath, ifc);
      } else if (strcmp(ifc_type, "TCP-TLS") == 0) {
         ifc->type = NS_IF_TYPE_TCP_TLS;
         if (interface_specific_params_alloc(ifc) != 0) {
            return SR_ERR_NOMEM;
         }
         rc = interface_tcp_tls_params_load(sess, xpath, ifc);
      } else if (strcmp(ifc_type, "UNIXSOCKET") == 0) {
         ifc->type = NS_IF_TYPE_UNIX;
         if (interface_specific_params_alloc(ifc) != 0) {
            return SR_ERR_NOMEM;
         }
         rc = interface_unix_params_load(sess, xpath, ifc);
      } else if (strcmp(ifc_type, "FILE") == 0) {
         ifc->type = NS_IF_TYPE_FILE;
         if (interface_specific_params_alloc(ifc) != 0) {
            return SR_ERR_NOMEM;
         }
         rc = interface_file_params_load(sess, xpath, ifc);
      } else if (strcmp(ifc_type, "BLACKHOLE") == 0) {
         ifc->type = NS_IF_TYPE_BH;
         if (interface_specific_params_alloc(ifc) != 0) {
            return SR_ERR_NOMEM;
         }
      } else {
         VERBOSE(N_ERR, "Invalid interface type found! Supervisor version is "
               "possibly not matching configuration schema version.")
         return SR_ERR_VERSION_MISMATCH;
      }
      if (rc != SR_ERR_OK) {
         // TODO
         goto err_cleanup;
      }
      NULLP_TEST_AND_FREE(ifc_type)
   }


   rc = load_sr_str(sess, xpath, "/timeout", &(ifc->timeout));
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_str(sess, xpath, "/buffer", &(ifc->buffer));
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_str(sess, xpath, "/autoflush", &(ifc->autoflush));
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }

   { // load interface direction
      rc = load_sr_str(sess, xpath, "/direction", &(ifc_dir));
      if (FOUND_AND_ERR(rc)) {
         goto err_cleanup;
      }
      if (strcmp(ifc_dir, "IN") == 0) {
         ifc->direction = NS_IF_DIR_IN;
      } else if (strcmp(ifc_dir, "OUT") == 0) {
         ifc->direction = NS_IF_DIR_OUT;
      } else {
         VERBOSE(N_ERR, "Invalid interface direction found! Supervisor version is "
               "possibly not matching configuration schema version.")
         return SR_ERR_VERSION_MISMATCH;
      }

      NULLP_TEST_AND_FREE(ifc_dir)
   }

   // Direction should be allocated by now, it's safe to allocate stats structs
   if (interface_stats_alloc(ifc) != 0) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   if (run_module_interface_add(inst, ifc) != 0) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   return SR_ERR_OK;

err_cleanup:
   NULLP_TEST_AND_FREE(ifc_type)
   NULLP_TEST_AND_FREE(ifc_dir)

   return rc;
}

static inline int
interface_tcp_params_load(sr_session_ctx_t *sess, char *base_xpath, interface_t *ifc)
{
   int rc;
   char * xpath = NULL;

   xpath = calloc(1, (strlen(base_xpath) + 12) * sizeof(char));
   if (xpath == NULL) {
      NO_MEM_ERR
      rc = SR_ERR_NOMEM;
      goto err_cleanup;
   }
   sprintf(xpath, "%s/tcp-params", base_xpath);

   rc = load_sr_str(sess, xpath, "/host", &(ifc->specific_params.tcp->host));
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_num(sess, xpath, "/port", &(ifc->specific_params.tcp->port), SR_UINT16_T);
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_num(sess, xpath, "/max-clients",
                    &(ifc->specific_params.tcp->max_clients), SR_UINT16_T);
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }

   return SR_ERR_OK;

err_cleanup:
   // TODO
   return rc;
}

static inline int
interface_tcp_tls_params_load(sr_session_ctx_t *sess, char *base_xpath, interface_t *ifc)
{
   int rc;
   char * xpath = NULL;

   xpath = calloc(1, (strlen(base_xpath) + 16) * sizeof(char));
   if (xpath == NULL) {
      NO_MEM_ERR
      rc = SR_ERR_NOMEM;
      goto err_cleanup;
   }
   sprintf(xpath, "%s/tcp-tls-params", base_xpath);

   rc = load_sr_str(sess, xpath, "/host", &(ifc->specific_params.tcp_tls->host));
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_str(sess, xpath, "/keyfile", &(ifc->specific_params.tcp_tls->keyfile));
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_str(sess, xpath, "/certfile", &(ifc->specific_params.tcp_tls->certfile));
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_str(sess, xpath, "/cafile", &(ifc->specific_params.tcp_tls->cafile));
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_num(sess, xpath, "/port",
                    &(ifc->specific_params.tcp_tls->port), SR_UINT16_T);
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_num(sess, xpath, "/max-clients",
                    &(ifc->specific_params.tcp_tls->max_clients), SR_UINT16_T);
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }

   return SR_ERR_OK;

err_cleanup:
   // TODO
   return rc;
}

static inline int
interface_unix_params_load(sr_session_ctx_t *sess, char *base_xpath, interface_t *ifc)
{
   int rc;
   char * xpath = NULL;

   xpath = calloc(1, (strlen(base_xpath) + 13) * sizeof(char));
   if (xpath == NULL) {
      NO_MEM_ERR
      rc = SR_ERR_NOMEM;
      goto err_cleanup;
   }
   sprintf(xpath, "%s/unix-params", base_xpath);

   rc = load_sr_str(sess, xpath, "/socket-name", &(ifc->specific_params.nix->socket_name));
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_num(sess, xpath, "/max-clients",
                    &(ifc->specific_params.nix->max_clients), SR_UINT16_T);
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }


   return SR_ERR_OK;

err_cleanup:
   // TODO
   return rc;
}

static inline int
interface_file_params_load(sr_session_ctx_t *sess, char *base_xpath, interface_t *ifc)
{
   int rc;
   char * xpath = NULL;

   xpath = calloc(1, (strlen(base_xpath) + 13) * sizeof(char));
   if (xpath == NULL) {
      NO_MEM_ERR
      rc = SR_ERR_NOMEM;
      goto err_cleanup;
   }
   sprintf(xpath, "%s/file-params", base_xpath);

   rc = load_sr_str(sess, xpath, "/name", &(ifc->specific_params.file->name));
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_str(sess, xpath, "/mode", &(ifc->specific_params.file->mode));
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_num(sess, xpath, "/time",
                    &(ifc->specific_params.file->time), SR_UINT16_T);
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_num(sess, xpath, "/size",
                    &(ifc->specific_params.file->size), SR_UINT16_T);
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }


   return SR_ERR_OK;

err_cleanup:
   // TODO
   return rc;
}

static void
instance_pid_restore(pid_t last_pid, run_module_t *inst,
                                 sr_session_ctx_t *sess)
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


   if (strcmp(run_path, inst->mod_kind->path) == 0) {
      // Process under PID last_pid is really this inst
      inst->pid = last_pid;
      inst->running = true;
      inst->is_my_child = false;
   }

   goto clean_pid;

clean_pid:
   /* Try to remove last_pid node from sysrepo but we don't really
    *  care if it fails */
   pid_xpath_len = NS_ROOT_XPATH_LEN
                   + strlen(inst->name) + 26;
   pid_xpath = (char *) calloc(1, sizeof(char) * (pid_xpath_len));
   if (pid_xpath != NULL) {
      sprintf(pid_xpath,
              NS_ROOT_XPATH"/module[name='%s']/last-pid",
              inst->name);
      rc = sr_delete_item(sess, pid_xpath, SR_EDIT_NON_RECURSIVE);
      if (rc == SR_ERR_OK) {
         sr_commit(sess);
      }
      NULLP_TEST_AND_FREE(pid_xpath)
   }
}

static int
load_sr_num(sr_session_ctx_t *sess, char *base_xpath, char *node_xpath,
                       void *where, sr_type_t data_type)
{
   int rc;
   char xpath[4096];
   sr_val_t * val = NULL;
   memset(xpath, 0, 4096);

   sprintf(xpath, "%s%s", base_xpath, node_xpath);
   rc = sr_get_item(sess, xpath, &val);
   if (rc != SR_ERR_OK) {
      return rc;
   }

   switch (data_type) {
      case SR_BOOL_T:
         *((bool *) where) = val->data.bool_val;
         break;
      case SR_UINT8_T:
         *((uint8_t *) where) = val->data.uint8_val;
         break;
      case SR_UINT16_T:
         *((uint16_t *) where) = val->data.uint16_val;
         break;
      case SR_UINT32_T:
         *((uint32_t *) where) = val->data.uint32_val;
         break;

      default:
         // TODO err
         sr_free_val(val);
         return -1;
   }
   sr_free_val(val);

   return SR_ERR_OK;
}

static int
load_sr_str(sr_session_ctx_t *sess, char *base_xpath, char *node_xpath,
                       char **where)
{
   int rc;
   char xpath[4096];
   sr_val_t * val = NULL;
   memset(xpath, 0, 4096);

   sprintf(xpath, "%s%s", base_xpath, node_xpath);
   rc = sr_get_item(sess, xpath, &val);
   if (rc != SR_ERR_OK) {
      return rc;
   }

   *where = strdup(val->data.string_val);
   sr_free_val(val);

   if (*where == NULL) {
      // TODO err
      NO_MEM_ERR
      return SR_ERR_NOMEM;
   }

   return SR_ERR_OK;
}
/* --END local fns-- */
