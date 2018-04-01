#include <string.h>
#include <sysrepo/xpath.h>
#include "conf.h"
#include "module.h"

/*--BEGIN superglobal vars--*/
bool daemon_flag = false;
char *logs_path = NULL;
/*--END superglobal vars--*/

/*--BEGIN local #define--*/

#define FOUND_AND_ERR(rc) ((rc) != SR_ERR_NOT_FOUND && (rc) != SR_ERR_OK)

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
/*static int module_group_load(sr_session_ctx_t *sess,
                             module_group_t *group,
                             sr_node_t *node);*/

/**
 * TODO
 * */
/*static int module_load(sr_session_ctx_t *sess,
                       module_t *mod,
                       module_group_t *grp,
                       sr_node_t *node);*/
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
run_module_load(sr_session_ctx_t *sess, char *xpath);

static int
av_module_load(sr_session_ctx_t *sess, char *xpath);

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

static char * module_name_from_xpath(char *xpath);
/* --END full fns prototypes-- */

/* --BEGIN superglobal fns-- */


int ns_startup_config_load(sr_session_ctx_t *sess)
{
   int rc;
   sr_val_t *vals = NULL;
   size_t vals_cnt = 0;
   char av_mods_xpath[] = NS_ROOT_XPATH"/available-module";
   char run_mods_xpath[] = NS_ROOT_XPATH"/instance";

   { // load /available-modules
      rc = sr_get_items(sess, av_mods_xpath, &vals, &vals_cnt);
      if (rc != SR_ERR_OK) {
         VERBOSE(N_ERR, "Failed to load %s. Error: %s", av_mods_xpath, sr_strerror(rc))
         goto err_cleanup;
      }

      for (int i = 0; i < vals_cnt; i++) {
         rc = av_module_load(sess, vals[i].xpath);
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
         rc = run_module_load(sess, vals[i].xpath);
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

int av_module_load_by_name(sr_session_ctx_t *sess, const char *module_name)
{
   int rc;
   sr_val_t *insts = NULL;
   size_t insts_cnt = 0;
   char * inst_name = NULL;

// 255 is maximum for name key, 37 is format string with reserve
#define XPATH_LEN (NS_ROOT_XPATH_LEN + 255 + 37)
   char xpath[XPATH_LEN];

   memset(xpath, 0, XPATH_LEN);
   sprintf(xpath, NS_ROOT_XPATH"/available-module[name='%s']", module_name);

   rc = av_module_load(sess, xpath);
   if (rc != SR_ERR_OK) {
      VERBOSE(N_ERR, "Failed to load new module configuration")
      return rc;
   }


   memset(xpath, 0, XPATH_LEN);
   sprintf(xpath, NS_ROOT_XPATH"/instance/module-ref");

   rc = sr_get_items(sess, xpath, &insts, &insts_cnt);
   if (rc != SR_ERR_OK) {
      VERBOSE(N_ERR, "Failed to load xpath %s. Error: %s", xpath, sr_strerror(rc))
      goto err_cleanup;
   }

   run_module_t * x;

   for (size_t i = 0; i < insts_cnt; i++) {
      if (strcmp(insts[i].data.string_val, module_name) == 0) {
         inst_name = module_name_from_xpath(insts[i].xpath);
         if (inst_name == NULL) {
            NO_MEM_ERR
            rc = SR_ERR_NOMEM;
            goto err_cleanup;
         }
         memset(xpath, 0, XPATH_LEN);
         sprintf(xpath, NS_ROOT_XPATH"/instance[name='%s']", inst_name);
         rc = run_module_load(sess, xpath);
         if (rc != SR_ERR_OK) {
            VERBOSE(N_ERR, "Failed to reload all instances of module '%s'."
                  " Failed at instance '%s'", module_name, inst_name)
            goto err_cleanup;
         }
         x = rnmods_v.items[rnmods_v.total - 1];
         VERBOSE(DEBUG, "for module %s is pid %d", inst_name, x->pid)
      }
   }

   return SR_ERR_OK;

err_cleanup:
   if (insts != NULL && insts_cnt != 0) {
      sr_free_values(insts, insts_cnt);
   }

   return rc;
}

int run_module_load_by_name(sr_session_ctx_t *sess, const char *inst_name)
{
   int rc;
   char * xpath = NULL;
   uint32_t xpath_len = (uint32_t) (NS_ROOT_XPATH_LEN + strlen(inst_name) + 17);


   xpath = (char *) calloc(1, sizeof(char) * (xpath_len));
   IF_NO_MEM_INT_ERR(xpath)
   sprintf(xpath, NS_ROOT_XPATH"/module[name='%s']", inst_name);

   rc = run_module_load(sess, xpath);
   if (rc != 0) {
      VERBOSE(N_ERR, "Failed to load new module configuration from fetched"
            " sysrepo subtree")
      return rc;
   }

   return SR_ERR_OK;
}
/* --END superglobal fns-- */

/* --BEGIN local fns-- */

static int
run_module_load(sr_session_ctx_t *sess, char *xpath)
{
   int rc;
   pid_t last_pid = 0;
   char *mod_kind = NULL;
   char *ifc_xpath = NULL;
   size_t ifc_cnt = 0;
   sr_val_t *ifces = NULL;

   run_module_t *inst = run_module_alloc();
   if (inst == NULL) {
      NO_MEM_ERR
      return SR_ERR_NOMEM;
   }
   IF_NO_MEM_INT_ERR(inst)

   if (vector_add(&rnmods_v, inst) != 0) {
      NO_MEM_ERR
      return SR_ERR_NOMEM;
   }

   rc = load_sr_str(sess, xpath, "/name", &(inst->name));
   if (FOUND_AND_ERR(rc)) {
      VERBOSE(N_ERR, "Failed to load xpath %s/name", xpath)
      goto err_cleanup;
   }
   rc = load_sr_str(sess, xpath, "/module-ref", &mod_kind);
   if (FOUND_AND_ERR(rc)) {
      VERBOSE(N_ERR, "Failed to load xpath %s/module-ref", xpath)
      goto err_cleanup;
   }
   rc = load_sr_num(sess, xpath, "/enabled", &(inst->enabled), SR_BOOL_T);
   if (FOUND_AND_ERR(rc)) {
      VERBOSE(N_ERR, "Failed to load xpath %s/enabled", xpath)
      goto err_cleanup;
   }
   rc = load_sr_num(sess, xpath, "/max-restarts-per-min",
                    &(inst->max_restarts_minute), SR_UINT8_T);
   if (FOUND_AND_ERR(rc)) {
      VERBOSE(N_ERR, "Failed to load xpath %s/max-restarts-per-min", xpath)
      goto err_cleanup;
   }
   rc = load_sr_str(sess, xpath, "/params", &(inst->params));
   if (FOUND_AND_ERR(rc)) {
      VERBOSE(N_ERR, "Failed to load xpath %s/params", xpath)
      goto err_cleanup;
   }
   rc = load_sr_num(sess, xpath, "/last-pid", &last_pid, SR_UINT32_T);
   if (FOUND_AND_ERR(rc)) {
      VERBOSE(N_ERR, "Failed to load xpath %s/last-pid", xpath)
      goto err_cleanup;
   }

   { // assign available-module name to pointer
      av_module_t *mod;
      for (int i = 0; i < avmods_v.total; i++) {
         mod = avmods_v.items[i];
         if (strcmp(mod->name, mod_kind) == 0) {
            inst->mod_kind = mod;
            break;
         }
      }
      if (inst->mod_kind == NULL) {
         VERBOSE(N_ERR, "Failed to load module '%s' since it's available module "
               "'%s' is not loaded.", inst->name, mod_kind)
         goto err_cleanup;
      }
   }

   { // load interfaces
      ifc_xpath = calloc(1, (strlen(xpath) + 11) * sizeof(char));
      if (ifc_xpath == NULL) {
         NO_MEM_ERR
         goto err_cleanup;
      }
      sprintf(ifc_xpath, "%s/interface", xpath);

      rc = sr_get_items(sess, ifc_xpath, &ifces, &ifc_cnt);
      if (FOUND_AND_ERR(rc)) {
         VERBOSE(N_ERR, "Failed to get items for xpath=%s/interface. Error: %s",
                 xpath, sr_strerror(rc))
         goto err_cleanup;
      }

      for (size_t i = 0; i < ifc_cnt; i++) {
         interface_load(sess, ifces[i].xpath, inst);
      }
      sr_free_values(ifces, ifc_cnt);
      NULLP_TEST_AND_FREE(ifc_xpath)
   }

   rc = run_module_gen_exec_args(inst);
   if (rc != 0) {
      /* hopefully this would be removed in the future due to sysrepo per
       * module configuration */
      VERBOSE(N_ERR, "Failed to prepare exec_args")
      rc = SR_ERR_NOMEM;
      goto err_cleanup;
   }

   if (last_pid > 0) {
      VERBOSE(V3, "Restoring PID=%d for %s", last_pid, inst->name)
      instance_pid_restore(last_pid, inst, sess);
   }

   return SR_ERR_OK;

err_cleanup:
   VERBOSE(N_ERR, "Failed to load module instance from xpath=%s", xpath)
   if (ifces != NULL) {
      sr_free_values(ifces, ifc_cnt);
   }
   NULLP_TEST_AND_FREE(ifc_xpath)
   NULLP_TEST_AND_FREE(mod_kind)
   run_module_free(inst);

   return rc;
}

static int
av_module_load(sr_session_ctx_t *sess, char *xpath)
{
   av_module_t *amod = av_module_alloc();
   IF_NO_MEM_INT_ERR(amod)

   if (vector_add(&avmods_v, amod) != 0) {
      NO_MEM_ERR
      return SR_ERR_NOMEM;
   }

   int rc;

   rc = load_sr_str(sess, xpath, "/name", &(amod->name));
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_str(sess, xpath, "/path", &(amod->path));
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_num(sess, xpath, "/is-nemea-module", &(amod->is_nemea), SR_BOOL_T);
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }
   rc = load_sr_num(sess, xpath, "/is-sysrepo-ready", &(amod->is_sr_en), SR_BOOL_T);
   if (FOUND_AND_ERR(rc)) {
      goto err_cleanup;
   }

   return 0;

err_cleanup:
   VERBOSE(N_ERR, "Failed to load module from xpath %s", xpath)
   return rc;
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
              NS_ROOT_XPATH"/instance[name='%s']/last-pid",
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

static char * module_name_from_xpath(char *xpath)
{
   char *res = NULL;
   char *name = NULL;
   sr_xpath_ctx_t state = {0};

   // /nemea:supervisor
   res = sr_xpath_next_node(xpath, &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to parse module name from xpath on line %d", __LINE__)
      goto err_cleanup;
   }
   // /module
   res = sr_xpath_next_node(NULL, &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to parse module name from xpath on line %d", __LINE__)
      goto err_cleanup;
   }

   // fetch key 'name'
   res = sr_xpath_node_key_value_idx(NULL, 0, &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to parse module name from xpath on line %d", __LINE__)
      goto err_cleanup;
   }

   name = strdup(res);
   if (name == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   sr_xpath_recover(&state);

   return name;

err_cleanup:
   sr_xpath_recover(&state);
   return NULL;
}
/* --END local fns-- */
