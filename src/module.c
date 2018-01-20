#include <stdarg.h>
#include <libtrap/trap.h>
#include <sysrepo/xpath.h>
#include "module.h"
#include "utils.h"

/*--BEGIN superglobal vars--*/
pthread_mutex_t config_lock; ///< Mutex for operations on m_groups_ll and modules_ll
/* Module groups variables */
//module_group_t *m_groups_ll = NULL; ///< Linled list of module groups loaded from sysrepo
//uint32_t loaded_profile_cnt = 0;
/* Modules variables */
//instance_t *modules_ll = NULL;  ///< Linked list of modules loaded from sysrepo configuration
//uint32_t running_modules_cnt = 0;  ///< Current size of running_modules array.
//uint32_t loaded_modules_cnt = 0; ///< Current number of loaded modules.
//uint32_t loaded_module_groups_cnt = 0;

vector_t insts_v = {.total = 0, .capacity = 0, .items = NULL};
vector_t mods_v = {.total = 0, .capacity = 0, .items = NULL};
vector_t mgrps_v = {.total = 0, .capacity = 0, .items = NULL};

vector_t avmods_v = {.total = 0, .capacity = 0, .items = NULL};
vector_t rnmods_v = {.total = 0, .capacity = 0, .items = NULL};

/*--END superglobal vars--*/

/*--BEGIN local #define--*/

/*--END local #define--*/

/*--BEGIN local typedef--*/
/*--END local typedef--*/

/* --BEGIN local vars-- */
/* --END local vars-- */

/* --BEGIN full fns prototypes-- */
static char * tcp_ifc_to_cli_arg(const interface_t *ifc);
static char * tcp_tls_ifc_to_cli_arg(const interface_t *ifc);
static char * unix_ifc_to_cli_arg(const interface_t *ifc);
static char * file_ifc_to_cli_arg(const interface_t *ifc);
static char * bh_ifc_to_cli_arg(const interface_t *ifc);
static inline char * module_get_ifcs_as_arg(run_module_t *module);

char * strcat_many(uint8_t cnt, ...);

static char **module_params_to_ifc_arr(const run_module_t *module, uint32_t *params_num,
                                       int *rc);
static char *ifc_param_concat(char *base, char *param_s, uint16_t param_u);
static inline void free_interface_specific_params(interface_t *ifc);
/* --END full fns prototypes-- */

/* --BEGIN superglobal fns-- */

void tree_path_init(tree_path_t *path)
{
   path->ifc = NULL;
   path->inst = NULL;
   path->module = NULL;
   path->group = NULL;
}

void tree_path_free(tree_path_t *node)
{
   NULLP_TEST_AND_FREE(node->ifc)
   NULLP_TEST_AND_FREE(node->inst)
   NULLP_TEST_AND_FREE(node->module)
   NULLP_TEST_AND_FREE(node->group)
}

int tree_path_load_from_xpath(tree_path_t *node, const char *xpath)
{

   char *res;
   sr_xpath_ctx_t state = {0};
   node->group = NULL;
   node->module = NULL;
   node->inst = NULL;
   node->ifc = NULL;

   // sr_xpath_* functions require dynamically allocated char * argument
   char *xpath_d = strdup(xpath);
   IF_NO_MEM_INT_ERR(xpath_d)

   // Move to next XPATH node (YANG module name)
   res = sr_xpath_next_node(xpath_d, &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to get next node from xpath=%s at line: %d",
              xpath, __LINE__)
      goto err_cleanup;
   }

   // Move to next XPATH node (module-group list)
   res = sr_xpath_next_node(NULL, &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to get next node from xpath=%s at line: %d",
              xpath, __LINE__)
      goto err_cleanup;
   }

   // Fetch module group name from module-group list key
   res = sr_xpath_node_key_value(NULL, "name", &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to get next node from xpath=%s at line: %d",
              xpath, __LINE__)
      goto err_cleanup;
   }
   node->group = strdup(res);
   if (node->group == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   // Move to next XPATH node (module list)
   res = sr_xpath_next_node(NULL, &state);
   if (res == NULL) {
      sr_xpath_recover(&state);
      VERBOSE(N_ERR, "Failed to get next node from xpath=%s at line: %d",
              xpath, __LINE__)
      goto err_cleanup;
   }

   // Test whether this mode is module list node
   if (strcmp(res, "module") != 0) {
      node->module = NULL;
      node->ifc = NULL;
      sr_xpath_recover(&state);
      return 0;
   }

   // Get module name from module list key
   res = sr_xpath_node_key_value(NULL, "name", &state);
   if (res == NULL) {
      sr_xpath_recover(&state);
      goto err_cleanup;
   }
   node->module = strdup(res);
   if (node->module == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   // Move to next XPATH node (instance list)
   res = sr_xpath_next_node(NULL, &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to get next node from xpath=%s at line: %d",
              xpath, __LINE__)
      goto err_cleanup;
   }

   // Test whether this mode is instance list node
   if (strcmp(res, "instance") != 0) {
      node->inst = NULL;
      sr_xpath_recover(&state);
      return 0;
   }

   // Get interface name from interface list key
   res = sr_xpath_node_key_value(NULL, "name", &state);
   if (res == NULL) {
      sr_xpath_recover(&state);
      goto err_cleanup;
   }
   node->inst = strdup(res);
   if (node->inst == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   // Move to next XPATH node (interface list)
   res = sr_xpath_next_node(NULL, &state);
   if (res == NULL) {
      sr_xpath_recover(&state);
      VERBOSE(N_ERR, "Failed to get next node from xpath=%s at line: %d",
              xpath, __LINE__)
      goto err_cleanup;
   }

   // Test whether this mode is interface list node
   if (strcmp(res, "interface") != 0) {
      node->ifc = NULL;
      sr_xpath_recover(&state);
      return 0;
   }

   // Get interface name from interface list key
   res = sr_xpath_node_key_value(NULL, "name", &state);
   if (res == NULL) {
      sr_xpath_recover(&state);
      goto err_cleanup;
   }
   node->ifc = strdup(res);
   if (node->ifc == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   sr_xpath_recover(&state);
   NULLP_TEST_AND_FREE(xpath_d)

   return 0;

err_cleanup:
   sr_xpath_recover(&state);
   NULLP_TEST_AND_FREE(xpath_d)
   NULLP_TEST_AND_FREE(node->group)
   NULLP_TEST_AND_FREE(node->module)
   NULLP_TEST_AND_FREE(node->ifc)
   return -1;
}

/*int module_group_add(module_group_t *group)
{
   return vector_add(&mgrps_v, group);
}

void module_group_remove_at(uint32_t index)
{
   vector_delete(&mgrps_v, index);
}

void module_group_remove_by_name(const char * name)
{
   module_group_t *grp = NULL;
   for (uint32_t i = 0; i < mods_v.total; i++) {
      grp = mods_v.items[i];
      if (strcmp(grp->name, name) == 0) {
         module_group_remove_at(i);
         return;
      }
   }
}

int module_add(module_t *mod)
{
   return vector_add(&mods_v, mod);
}

void module_remove_at(uint32_t index)
{
   vector_delete(&mods_v, index);
}

void module_remove_by_name(const char * name)
{
   module_t *mod = NULL;
   for (uint32_t i = 0; i < mods_v.total; i++) {
      mod = mods_v.items[i];
      if (strcmp(mod->name, name) == 0) {
         module_remove_at(i);
         return;
      }
   }
}

int instance_add(instance_t *inst)
{
   int rc;

*//*   rc = vector_add(&inst->module->insts, inst);
   if (rc != 0) {
      return rc;
   }*//*

   rc = vector_add(&insts_v, inst);
   if (rc != 0) {
      return rc;
   }

   // TODO inst->module->group->insts_cnt++;

   return 0;
}*/


void av_module_remove_at(uint32_t index)
{/*
   instance_t *inst = insts_v.items[index];*/
   vector_delete(&avmods_v, index);
   // TODO inst->modle->group->insts_cnt--;
}

void run_module_remove_at(uint32_t index)
{/*
   instance_t *inst = insts_v.items[index];*/
   vector_delete(&rnmods_v, index);
   // TODO inst->module->group->insts_cnt--;
}

/*void instance_remove_by_name(const char * name)
{
   instance_t *inst = NULL;
   for (uint32_t i = 0; i < insts_v.total; i++) {
      inst = insts_v.items[i];
      if (strcmp(inst->name, name) == 0) {
         run_module_remove_at(i);
         return;
      }
   }
}*/

int run_module_interface_add(run_module_t *mod, interface_t *ifc)
{
   int rc;

   if (ifc->direction == NS_IF_DIR_IN) {
      rc = vector_add(&mod->in_ifces, ifc);
   } else {
      rc = vector_add(&mod->out_ifces, ifc);
   }

   if (rc != 0) {
      NO_MEM_ERR
      return rc;
   }

   return 0;
}
/*
void print_module_group(const module_group_t *group)
{
   VERBOSE(DEBUG, "Group: %s", group->name)
   VERBOSE(DEBUG, " enabled: %d", group->enabled)
}

void print_module(const module_t *mod)
{
   VERBOSE(DEBUG, "Module: %s", mod->name)
   VERBOSE(DEBUG, " path: %s", mod->path)
}

void print_instance(instance_t *inst)
{
   VERBOSE(DEBUG, "Module instance: %s", inst->name)
   VERBOSE(DEBUG, " inst=%s", inst->name)
   VERBOSE(DEBUG, " params=%s", inst->params)
   VERBOSE(DEBUG, " enabled=%s", inst->enabled ? "true" : "false")
   VERBOSE(DEBUG, " running=%s", inst->running ? "true" : "false")
   VERBOSE(DEBUG, " restart_cnt=%d", inst->restarts_cnt)
   VERBOSE(DEBUG, " max_restarts_minute=%d", inst->max_restarts_minute)

   interface_t *ifc;
   FOR_EACH_IN_VEC(inst->in_ifces, ifc) {
      print_ifc(ifc);
   }
}*/

void print_ifc_stats()
{

}

void print_ifc(interface_t *ifc)
{
   VERBOSE(DEBUG, " IFC DIR=%d TYPE=%d", ifc->direction, ifc->type)
   VERBOSE(DEBUG, "  buffer=%s", ifc->buffer)
   VERBOSE(DEBUG, "  autoflush=%s", ifc->autoflush)
   VERBOSE(DEBUG, "  timeout=%s", ifc->timeout)

   switch (ifc->type) {
      case NS_IF_TYPE_TCP:
         VERBOSE(DEBUG, "  host=%s", ifc->specific_params.tcp->host)
         VERBOSE(DEBUG, "  max_clients=%d", ifc->specific_params.tcp->max_clients)
         VERBOSE(DEBUG, "  port=%d", ifc->specific_params.tcp->port)
         break;
      case NS_IF_TYPE_TCP_TLS:
         VERBOSE(DEBUG, "  host=%s", ifc->specific_params.tcp_tls->host)
         VERBOSE(DEBUG, "  max_clients=%d", ifc->specific_params.tcp_tls->max_clients)
         VERBOSE(DEBUG, "  port=%d", ifc->specific_params.tcp_tls->port)
         VERBOSE(DEBUG, "  certfile=%s", ifc->specific_params.tcp_tls->certfile)
         VERBOSE(DEBUG, "  keyfile=%s", ifc->specific_params.tcp_tls->keyfile)
         VERBOSE(DEBUG, "  cafile=%s", ifc->specific_params.tcp_tls->cafile)
         VERBOSE(DEBUG, "  ")
         break;
      case NS_IF_TYPE_FILE:
         VERBOSE(DEBUG, "  name=%s", ifc->specific_params.file->name)
         VERBOSE(DEBUG, "  mode=%s", ifc->specific_params.file->mode)
         VERBOSE(DEBUG, "  size=%d", ifc->specific_params.file->size)
         VERBOSE(DEBUG, "  time=%d", ifc->specific_params.file->time)
         break;
      case NS_IF_TYPE_UNIX:
         VERBOSE(DEBUG, "  socket_name=%s", ifc->specific_params.nix->socket_name)
         VERBOSE(DEBUG, "  max_clients=%d", ifc->specific_params.nix->max_clients)
         break;
      case NS_IF_TYPE_BH:
         break;
   }

}

/*module_group_t * module_group_alloc()
{
   module_group_t * group = (module_group_t *) calloc(1, sizeof(module_group_t));
   if (NULL == group) {
      VERBOSE(N_ERR, "Failed to callocate new module module group")
      return NULL;
   }

   group->name = NULL;
   group->enabled = false;
   // TODO group->insts_cnt = 0;
   VERBOSE(DEBUG, "Allocated new module group")

   return group;
}

module_t * module_alloc()
{
   module_t * mod = (module_t *) calloc(1, sizeof(module_t));
   IF_NO_MEM_NULL_ERR(mod)

   mod->name = NULL;
   mod->path = NULL;
   mod->group = NULL;

*//*   int rc;
   rc = vector_init(&mod->insts, 1);
   if (rc != 0) {
      NO_MEM_ERR
      free(mod);
      return NULL;
   }*//*

   return mod;
}*/

av_module_t * av_module_alloc()
{
   av_module_t * mod = (av_module_t *) calloc(1, sizeof(av_module_t));
   IF_NO_MEM_NULL_ERR(mod)

   mod->name = NULL;
   mod->path = NULL;
   mod->is_sr_en = false;
   mod->is_nemea = false;

   return mod;
}

run_module_t * run_module_alloc()
{
   run_module_t * inst = (run_module_t *) calloc(1, sizeof(run_module_t));
   IF_NO_MEM_NULL_ERR(inst)

   inst->enabled = false;
   inst->running = false;
   inst->should_die = false;
   inst->root_perm_needed = false;
   inst->is_my_child = false;
   inst->sigint_sent = false;
   inst->service_ifc_connected = false;
   inst->name = NULL;
   inst->params = NULL;
   inst->exec_args = NULL;
   inst->mod_kind = NULL;
   inst->restarts_cnt = 0;
   inst->max_restarts_minute = 0;
   inst->restart_time = 0;
   inst->pid = 0;
   inst->mem_vms = 0;
   inst->mem_rss = 0;
   inst->last_cpu_kmode = 0;
   inst->last_cpu_perc_kmode = 0;
   inst->last_cpu_umode = 0;
   inst->last_cpu_perc_umode = 0;
   inst->service_ifc_conn_timer = 0;
   inst->service_sd = -1;

   int rc;

   rc = vector_init(&inst->in_ifces, 1);
   if (rc != 0) {
      NO_MEM_ERR
      free(inst);
      return NULL;
   }
   rc = vector_init(&inst->out_ifces, 1);
   if (rc != 0) {
      NO_MEM_ERR
      free(inst);
      return NULL;
   }

   VERBOSE(V3, "Allocated new inst")

   return inst;
}

interface_t * interface_alloc()
{
   interface_t *interface = (interface_t *) calloc(1, sizeof(interface_t));
   if (NULL == interface) {
      VERBOSE(N_ERR, "Failed to callocate new interface")
      return NULL;
   }

   interface->name = NULL;
   interface->buffer = NULL;
   interface->autoflush = NULL;
   interface->timeout = NULL;
   interface->stats = NULL;
   VERBOSE(V3, "Allocated new instance interface")

   return interface;
}

int interface_specific_params_alloc(interface_t *ifc)
{
   switch (ifc->type) {
      case NS_IF_TYPE_TCP:
         ifc->specific_params.tcp =
               (tcp_ifc_params_t *) calloc(1, sizeof(tcp_ifc_params_t));
         if (ifc->specific_params.tcp == NULL) {
            return -1;
         }
         ifc->specific_params.tcp->host = NULL;
         ifc->specific_params.tcp->port = 0;
         ifc->specific_params.tcp->max_clients = 0;
         ifc->ifc_to_cli_arg_fn = &tcp_ifc_to_cli_arg;
         break;

      case NS_IF_TYPE_TCP_TLS:
         ifc->specific_params.tcp_tls =
               (tcp_tls_ifc_params_t *) calloc(1, sizeof(tcp_tls_ifc_params_t));
         if (ifc->specific_params.tcp_tls == NULL) {
            return -1;
         }
         ifc->specific_params.tcp_tls->host = NULL;
         ifc->specific_params.tcp_tls->keyfile = NULL;
         ifc->specific_params.tcp_tls->certfile = NULL;
         ifc->specific_params.tcp_tls->cafile = NULL;
         ifc->specific_params.tcp_tls->port = 0;
         ifc->specific_params.tcp_tls->max_clients = 0;
         ifc->ifc_to_cli_arg_fn = &tcp_tls_ifc_to_cli_arg;
         break;

      case NS_IF_TYPE_UNIX:
         ifc->specific_params.nix =
               (unix_ifc_params_t *) calloc(1, sizeof(unix_ifc_params_t));
         if (ifc->specific_params.nix == NULL) {
            return -1;
         }
         ifc->specific_params.nix->socket_name = NULL;
         ifc->specific_params.nix->max_clients = 0;
         ifc->ifc_to_cli_arg_fn = &unix_ifc_to_cli_arg;
         break;

      case NS_IF_TYPE_FILE:
         ifc->specific_params.file =
               (file_ifc_params_t *) calloc(1, sizeof(file_ifc_params_t));
         if (ifc->specific_params.file == NULL) {
            return -1;
         }
         ifc->specific_params.file->name = NULL;
         ifc->specific_params.file->mode = NULL;
         ifc->specific_params.file->time = 0;
         ifc->specific_params.file->size = 0;
         ifc->ifc_to_cli_arg_fn = &file_ifc_to_cli_arg;
         break;

      case NS_IF_TYPE_BH:
         ifc->ifc_to_cli_arg_fn = &bh_ifc_to_cli_arg;
         break;
   }

   return 0;
}


int interface_stats_alloc(interface_t *ifc)
{
   // retrun int error codes
   if (ifc->direction == NS_IF_DIR_IN) {
      ifc->stats = (ifc_in_stats_t *) calloc(1, sizeof(ifc_in_stats_t));
      ifc_in_stats_t *in_stats = ifc->stats;
      IF_NO_MEM_INT_ERR(ifc->stats)

      in_stats->state = 0;
      in_stats->id = NULL;
      in_stats->recv_msg_cnt = 0;
      in_stats->recv_buff_cnt = 0;
   } else if (ifc->direction == NS_IF_DIR_OUT) {
      ifc->stats = (ifc_out_stats_t *) calloc(1, sizeof(ifc_out_stats_t));
      ifc_out_stats_t *out_stats = ifc->stats;
      IF_NO_MEM_INT_ERR(ifc->stats)

      out_stats->id = NULL;
      out_stats->type = 0;
      out_stats->num_clients = 0;
      out_stats->sent_msg_cnt = 0;
      out_stats->dropped_msg_cnt = 0;
      out_stats->sent_buff_cnt = 0;
      out_stats->autoflush_cnt = 0;
   } else {
      VERBOSE(N_ERR, "No interface direction specified while allocating stats struct")
      return -1;
   }

   return 0;
}
/*
void module_groups_free()
{
   if (mgrps_v.total != 0) {
      module_group_t *group;
      FOR_EACH_IN_VEC(mgrps_v, group) {
         module_group_free(group);
      }
   }
   vector_free(&mgrps_v);
}

void module_group_free(module_group_t *group)
{
   VERBOSE(DEBUG, "Freeing of group '%s'", group->name)
   NULLP_TEST_AND_FREE(group->name)
   NULLP_TEST_AND_FREE(group)
}

void module_free(module_t *module)
{
   VERBOSE(DEBUG, "Freeing of module '%s'", module->name)
   NULLP_TEST_AND_FREE(module->name)
   NULLP_TEST_AND_FREE(module->path)
   NULLP_TEST_AND_FREE(module)
}*/

/*void modules_free()
{
   if (mods_v.total != 0) {
      module_t *mod = NULL;
      FOR_EACH_IN_VEC(mods_v, mod) {
         module_free(mod);
      }
   }
   vector_free(&mods_v);
}*/

void run_modules_free()
{
   if (insts_v.total != 0) {
      run_module_t *inst = NULL;
      FOR_EACH_IN_VEC(insts_v, inst) {
         run_module_free(inst);
      }
   }
   vector_free(&insts_v);
}

void run_module_clear_socks(run_module_t *inst)
{
   // Clean inst's socket files
   char buffer[DEFAULT_SIZE_OF_BUFFER];
   char service_sock_spec[14];
   interface_t *ifc = NULL;

   FOR_EACH_IN_VEC(inst->out_ifces, ifc) {
      // Remove all UNIX socket files
      if (ifc->type != NS_IF_TYPE_UNIX ||
          ifc->specific_params.nix->socket_name == NULL) {
         continue;
      }

      memset(buffer, 0, DEFAULT_SIZE_OF_BUFFER);
      sprintf(buffer, trap_default_socket_path_format,
              ifc->specific_params.nix->socket_name);
      VERBOSE(V2, "Deleting socket %s of %s", buffer, inst->name)
      unlink(buffer);

   }

   // Delete unix-socket created by modules service interface
   if (inst->pid > 0) {
      memset(service_sock_spec, 0, 14 * sizeof(char));
      sprintf(service_sock_spec, "service_%d", inst->pid);
      sprintf(buffer, trap_default_socket_path_format, service_sock_spec);
      VERBOSE(V2, "Deleting socket %s of %s", buffer, inst->name)
      unlink(buffer);
   }
}

void av_module_free(av_module_t *mod)
{
   NULLP_TEST_AND_FREE(mod->path)
   NULLP_TEST_AND_FREE(mod->name)
   NULLP_TEST_AND_FREE(mod)
}

void run_module_free(run_module_t *inst)
{

   // TODO where to release sockets for ifc stats???
   NULLP_TEST_AND_FREE(inst->name)
   NULLP_TEST_AND_FREE(inst->params)
   if (inst->exec_args != NULL) {
      for (int i = 0; inst->exec_args[i] != NULL; i++) {
         NULLP_TEST_AND_FREE(inst->exec_args[i])
      }
      NULLP_TEST_AND_FREE(inst->exec_args)
   }
   interfaces_free(inst);
   NULLP_TEST_AND_FREE(inst)
}

void interfaces_free(run_module_t *module)
{
   interface_t *cur_ifc = NULL;
   vector_t *ifces_vec[2] = { &module->in_ifces, &module->out_ifces};

   for (int j = 0; j < 2; j++) {
      for (int i = 0; i < ifces_vec[j]->total; i++) {
         cur_ifc = ifces_vec[j]->items[i];
         NULLP_TEST_AND_FREE(cur_ifc->name);
         NULLP_TEST_AND_FREE(cur_ifc->buffer);
         NULLP_TEST_AND_FREE(cur_ifc->autoflush);
         NULLP_TEST_AND_FREE(cur_ifc->timeout);
         cur_ifc->ifc_to_cli_arg_fn = NULL;
         interface_stats_free(cur_ifc);
         free_interface_specific_params(cur_ifc);

         NULLP_TEST_AND_FREE(cur_ifc);
      }
      vector_free(ifces_vec[j]);
   }
}

void interface_stats_free(interface_t *ifc)
{
   if (ifc->direction == NS_IF_DIR_IN && ifc->stats != NULL) {
      ifc_in_stats_t *in_stats = ifc->stats;
      NULLP_TEST_AND_FREE(in_stats->id)
      NULLP_TEST_AND_FREE(in_stats)
   } else if (ifc->direction == NS_IF_DIR_OUT && ifc->stats != NULL) {
      ifc_out_stats_t *out_stats = ifc->stats;
      NULLP_TEST_AND_FREE(out_stats->id)
      NULLP_TEST_AND_FREE(out_stats)
   }
}

/*module_group_t * module_group_get_by_name(const char *name, uint32_t *index)
{
   uint32_t fi; // Index of found group
   module_group_t *group = NULL;
   for (fi = 0; fi < mgrps_v.total; fi++) {
      group = mgrps_v.items[fi];
      if (strcmp(group->name, name) == 0) {
         // Group was found
         if (index != NULL) {
            *index = fi;
         }
         return group;
      }
   }

   // Group not found
   return NULL;
}

module_t * module_get_by_name(const char * name, uint32_t *index)
{
   uint32_t fi; // Index of found module
   module_t *mod = NULL;

   for (fi = 0; fi < mods_v.total; fi++) {
      mod = mods_v.items[fi];
      if (strcmp(mod->name, name) == 0) {
         // Module was found
         if (index != NULL) {
            *index = fi;
         }
         return mod;
      }
   }

   // Module was not found
   return NULL;
}*/

run_module_t * run_module_get_by_name(const char *name, uint32_t *index)
{
   uint32_t fi; // Index of found instance
   run_module_t *inst = NULL;

   for (fi = 0; fi < insts_v.total; fi++) {
      inst = insts_v.items[fi];
      if (strcmp(inst->name, name) == 0) {
         // Instance was found
         if (index != NULL) {
            *index = fi;
         }
         return inst;
      }
   }

   // Instance was not found
   return NULL;
}

/*
interface_t * interface_get_by_path(const tree_path_t *path)
{
   interface_t *ifc;
   run_module_t *inst = NULL;
   FOR_EACH_IN_VEC(insts_v, inst) {
      if (strcmp(inst->name, path->inst) == 0) {
         if (strcmp(inst->mod_kind->name, path->module) == 0) {
            if (strcmp(inst->mod_kind->group->name, path->group) == 0) {
               ifc = NULL;
               FOR_EACH_IN_VEC(inst->in_ifces, ifc) {
                  if (strcmp(ifc->name, path->ifc) == 0) {
                     return ifc;
                  }
               }
               ifc = NULL;
               FOR_EACH_IN_VEC(inst->out_ifces, ifc) {
                  if (strcmp(ifc->name, path->ifc) == 0) {
                     return ifc;
                  }
               }
               return NULL;
            }
         }
      }
   }

   return NULL;
}
void module_group_clear(module_group_t *group)
{
   NULLP_TEST_AND_FREE(group->name)
   group->enabled = false;
   //TODO group->insts_cnt = 0;
}

void module_clear(module_t *module)
{
   NULLP_TEST_AND_FREE(module->name)
   NULLP_TEST_AND_FREE(module->path)
   module->group = NULL;

*//*   instance_t *inst = NULL;
   FOR_EACH_IN_VEC(module->insts, inst) {
      run_module_free(inst);
   }
   vector_free(&module->insts);*//*
}

void instance_clear(instance_t *inst)
{
   inst->running = false;
   inst->should_die = false;
   inst->root_perm_needed = false;
   inst->is_my_child = false;
   inst->sigint_sent = false;
   inst->service_ifc_connected = false;
   inst->restarts_cnt = 0;
   inst->max_restarts_minute = 0;
   inst->restart_time = 0;
   inst->pid = 0;
   inst->service_ifc_conn_timer = 0;
   inst->service_sd = -1;
   inst->module = NULL;

   NULLP_TEST_AND_FREE(inst->name)
   NULLP_TEST_AND_FREE(inst->params)
   if (inst->exec_args != NULL) {
      for (int i = 0; inst->exec_args[i] != NULL; i++) {
         NULLP_TEST_AND_FREE(inst->exec_args[i])
      }
      NULLP_TEST_AND_FREE(inst->exec_args)
   }
   interfaces_free(inst);
}*/

static inline void free_interface_specific_params(interface_t *ifc)
{
   switch (ifc->type) {
      case NS_IF_TYPE_TCP:
         NULLP_TEST_AND_FREE(ifc->specific_params.tcp->host)
         NULLP_TEST_AND_FREE(ifc->specific_params.tcp)
         break;

      case NS_IF_TYPE_TCP_TLS:
         NULLP_TEST_AND_FREE(ifc->specific_params.tcp_tls->host)
         NULLP_TEST_AND_FREE(ifc->specific_params.tcp_tls->keyfile)
         NULLP_TEST_AND_FREE(ifc->specific_params.tcp_tls->certfile)
         NULLP_TEST_AND_FREE(ifc->specific_params.tcp_tls->cafile)
         NULLP_TEST_AND_FREE(ifc->specific_params.tcp_tls)
         break;

      case NS_IF_TYPE_UNIX:
         NULLP_TEST_AND_FREE(ifc->specific_params.nix->socket_name)
         NULLP_TEST_AND_FREE(ifc->specific_params.nix)
         break;

      case NS_IF_TYPE_FILE:
         NULLP_TEST_AND_FREE(ifc->specific_params.file->name)
         NULLP_TEST_AND_FREE(ifc->specific_params.file->mode)
         NULLP_TEST_AND_FREE(ifc->specific_params.file)
         break;

      case NS_IF_TYPE_BH:
         // Blackhole has no params, this branch is here so that compiler doesn't complain
         break;
   }
}


/*
 * after redefining yang schema, probably not...
 * this is temporar since we want exec_args configuration straight from
 * <param><label>-o<label/><arg>1</arg><arg>33</arg></param>
 */
int module_gen_exec_args(run_module_t *inst)
{

   char **cfg_params = NULL; // All configured params but interface params
   char **exec_args = NULL;
   char *ifc_spec = NULL;
   uint32_t module_params_num = 0;
   uint32_t exec_args_cnt = 2; // at least the name of the future process and terminating NULL pointer
   uint32_t exec_args_pos = 0;
   int rc = 0; // Status of success for module_params_to_ifc_arr function
   uint32_t total_ifc_cnt = inst->in_ifces.total + inst->out_ifces.total;

   // if the inst has trap interfaces, one argument for "-i" and one for interfaces specifier
   if (total_ifc_cnt > 0) {
      exec_args_cnt += 2;
   }

   //* if the inst has non-empty params, try to parse them *//*
   if (inst->params != NULL) {
      cfg_params = module_params_to_ifc_arr(inst, &module_params_num, &rc);
      if (rc == -1) {
         VERBOSE(N_ERR, "Failed to parse inst params")
         return -1;
      }
      // after successful params parsing, increment the number of binary arguments
      exec_args_cnt += module_params_num;
   }

   exec_args = (char **) calloc(exec_args_cnt, sizeof(char *));
   IF_NO_MEM_INT_ERR(exec_args)

   // First argument to execv is a name of the future process
   exec_args[0] = strdup(inst->name);
   if (exec_args[0] == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   // last pointer is NULL because of execv function
   exec_args[exec_args_cnt - 1] = NULL;
   exec_args_pos = 1;

   // copy already allocated inst params strings returned by module_params_to_ifc_arr function
   if (cfg_params != NULL && module_params_num > 0) {
      for (int i = 0; i < module_params_num; i++) {
         exec_args[exec_args_pos] = cfg_params[i];
         exec_args_pos++;
      }
      NULLP_TEST_AND_FREE(cfg_params)
   }

   if (total_ifc_cnt == 0) {
      inst->exec_args = exec_args;
      return 0;
   }

   ifc_spec = module_get_ifcs_as_arg(inst);
   if (ifc_spec == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   exec_args[exec_args_pos] = strdup("-i"); // It's easier for freeing later
   IF_NO_MEM_INT_ERR(exec_args[exec_args_pos])
   exec_args_pos++;
   exec_args[exec_args_pos] = ifc_spec;
   inst->exec_args = exec_args;

   return 0;

err_cleanup:
   if (cfg_params != NULL) {
      for (int i = 0; i < module_params_num; i++) {
         NULLP_TEST_AND_FREE(cfg_params[i]);
      }
      NULLP_TEST_AND_FREE(cfg_params)
   }

   if (exec_args != NULL) {
      for (int i = 0; i < module_params_num; i++) {
         NULLP_TEST_AND_FREE(exec_args[i])
      }
      NULLP_TEST_AND_FREE(exec_args)
   }


   return -1;
}

/*
char * instance_tree_path(const instance_t * inst)
{
// Size for 3 names and slashes
#define TREE_BUFF_LEN (255 * 3 + 3)
   static char buffer[TREE_BUFF_LEN];
   bool module_group_not_null = inst->module != NULL && inst->module->group != NULL;
   memset(buffer, 0, TREE_BUFF_LEN);
   sprintf(buffer, "%s/%s/%s",
           (module_group_not_null) ? inst->module->group->name : "null",
           (inst->module != NULL) ? inst->module->name : "null",
           inst->name);
#undef TREE_BUFF_LEN

   return buffer;
}
*/

/* --END superglobal fns-- */

/* --BEGIN local fns-- */

static inline char * module_get_ifcs_as_arg(run_module_t *module)
{
   char *ifc_spec = NULL;
   char *new_ifc_spec = NULL;
   char *cur_ifc_params = NULL;
   char *tmp = NULL;
   interface_t *cur_ifc;

   vector_t *ifces_vec[2] = { &(module->in_ifces), &(module->out_ifces) };

   // For loop for both interface directions list
   for (int j = 0; j < 2; j++) {
      for (int i = 0; i < ifces_vec[j]->total; i++) {
         cur_ifc = ifces_vec[j]->items[i];
         cur_ifc_params = cur_ifc->ifc_to_cli_arg_fn(cur_ifc);
         if (cur_ifc_params == NULL) {
            NO_MEM_ERR
            goto err_cleanup;
         }

         // Only for OUT type, YANG ensures it won't be set for IN
         if (cur_ifc->buffer != NULL) {
            tmp = strcat_many(2, "buffer=", cur_ifc->buffer);
            if (tmp == NULL) {
               NO_MEM_ERR
               goto err_cleanup;
            }
            cur_ifc_params = ifc_param_concat(cur_ifc_params, tmp, 0);
            NULLP_TEST_AND_FREE(tmp)
            if (cur_ifc_params == NULL) {
               NO_MEM_ERR
               goto err_cleanup;
            }
         }

         // Only for OUT type, YANG ensures it won't be set for IN
         if (cur_ifc->autoflush != NULL) {
            tmp = strcat_many(2, "autoflush=", cur_ifc->autoflush);
            if (tmp == NULL) {
               NO_MEM_ERR
               goto err_cleanup;
            }
            cur_ifc_params = ifc_param_concat(cur_ifc_params, tmp, 0);
            NULLP_TEST_AND_FREE(tmp)
            if (cur_ifc_params == NULL) {
               NO_MEM_ERR
               goto err_cleanup;
            }
         }

         // Common for IN/OUT type
         if (cur_ifc->timeout != NULL) {
            tmp = strcat_many(2, "timeout=", cur_ifc->timeout);
            if (tmp == NULL) {
               NO_MEM_ERR
               goto err_cleanup;
            }
            cur_ifc_params = ifc_param_concat(cur_ifc_params, tmp, 0);
            NULLP_TEST_AND_FREE(tmp)
            if (cur_ifc_params == NULL) {
               NO_MEM_ERR
               goto err_cleanup;
            }
         }

         // Append to ifc_spec
         if (ifc_spec == NULL) {
            ifc_spec = cur_ifc_params;
         } else {
            new_ifc_spec = strcat_many(3, ifc_spec, ",", cur_ifc_params);
            if (new_ifc_spec == NULL) {
               NO_MEM_ERR
               goto err_cleanup;
            }
            NULLP_TEST_AND_FREE(ifc_spec)
            NULLP_TEST_AND_FREE(cur_ifc_params);
            ifc_spec = new_ifc_spec;
            new_ifc_spec = NULL;
         }
      }
   }

   return ifc_spec;

err_cleanup:
   NULLP_TEST_AND_FREE(cur_ifc_params)
   NULLP_TEST_AND_FREE(new_ifc_spec)
   NULLP_TEST_AND_FREE(ifc_spec)

   return NULL;
}

// Only one param shoul be passed, base is cleaned
static char * ifc_param_concat(char *base, char *param_s, uint16_t param_u)
{
   char *result;
   char *param; // Converted parameter to be concatenated
   uint16_t len;
   bool integer_passed = (param_s == NULL);

   if (integer_passed) {
      len = (uint16_t) ((param_u % 10) + 1);
      param = (char *) calloc(len, sizeof(char));
      if (param == NULL) {
         NO_MEM_ERR
         goto err_cleanup;
      }

      sprintf(param, "%u", param_u);
   } else {
      param = param_s;
   }

   result = strcat_many(3, base, ":", param);
   if (result == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   if (integer_passed) {
      // param was created dynamically in this function, clean it
      NULLP_TEST_AND_FREE(param);
   }
   NULLP_TEST_AND_FREE(base);

   return result;

err_cleanup:
   if (integer_passed) {
      NULLP_TEST_AND_FREE(param);
   }
   NULLP_TEST_AND_FREE(result);
   NULLP_TEST_AND_FREE(base);

   return NULL;
}

char * strcat_many(uint8_t cnt, ...)
{
   va_list args;
   char *tmp, *result;
   size_t len = 0;

   // Count overall length of result string
   va_start(args, cnt);
   for (uint8_t i = 0; i < cnt; i++) {
      tmp = va_arg(args, char *);
      len += strlen(tmp);
   }
   va_end(args);

   result = (char *) calloc(len + 1, sizeof(char));
   IF_NO_MEM_NULL_ERR(result)

   va_start(args, cnt);
   for (uint8_t i = 0; i < cnt; i++) {
      tmp = va_arg(args, char *);
      strcat(result, tmp);
   }
   va_end(args);

   return result;
}


/*
 *
 * There is no need to distinguish IN/OUT type since there are values loaded only for
 * IN or only for OUT type.
 * */

static char * tcp_ifc_to_cli_arg(const interface_t *ifc)
{
   char *params = strdup("t"); // strdup used so that ifc_param_concat can be used
   IF_NO_MEM_NULL_ERR(params)

   if (ifc->specific_params.tcp->host != NULL) {
      params = ifc_param_concat(params, ifc->specific_params.tcp->host, 0);
      IF_NO_MEM_NULL_ERR(params)
   }

   params = ifc_param_concat(params, NULL, ifc->specific_params.tcp->port);
   IF_NO_MEM_NULL_ERR(params)

   if (ifc->specific_params.tcp->max_clients > 0) {
      params = ifc_param_concat(params, NULL, ifc->specific_params.tcp->max_clients);
      IF_NO_MEM_NULL_ERR(params)
   }

   return params;
}
static char * tcp_tls_ifc_to_cli_arg(const interface_t *ifc)
{

   char *params = strdup("T");
   IF_NO_MEM_NULL_ERR(params)

   if (ifc->specific_params.tcp_tls->host != NULL) {
      params = ifc_param_concat(params, ifc->specific_params.tcp->host, 0);
      IF_NO_MEM_NULL_ERR(params)
   }
   params = ifc_param_concat(params, NULL, ifc->specific_params.tcp_tls->port);
   IF_NO_MEM_NULL_ERR(params)

   if (ifc->specific_params.tcp_tls->max_clients > 0) {
      params = ifc_param_concat(params, NULL, ifc->specific_params.tcp_tls->max_clients);
      IF_NO_MEM_NULL_ERR(params)
   }

   params = ifc_param_concat(params, ifc->specific_params.tcp_tls->keyfile, 0);
   IF_NO_MEM_NULL_ERR(params)
   params = ifc_param_concat(params, ifc->specific_params.tcp_tls->certfile, 0);
   IF_NO_MEM_NULL_ERR(params)
   params = ifc_param_concat(params, ifc->specific_params.tcp_tls->cafile, 0);
   IF_NO_MEM_NULL_ERR(params)

   return params;
}

static char * unix_ifc_to_cli_arg(const interface_t *ifc)
{
   char *params = strdup("u");
   IF_NO_MEM_NULL_ERR(params)

   params = ifc_param_concat(params, ifc->specific_params.nix->socket_name, 0);
   IF_NO_MEM_NULL_ERR(params)

   if (ifc->specific_params.nix->max_clients > 0) {
      params = ifc_param_concat(params, NULL, ifc->specific_params.nix->max_clients);
      IF_NO_MEM_NULL_ERR(params)
   }

   return params;
}

static char * file_ifc_to_cli_arg(const interface_t *ifc)
{
   char *params = strdup("f");
   IF_NO_MEM_NULL_ERR(params)

   params = ifc_param_concat(params, ifc->specific_params.file->name, 0);
   IF_NO_MEM_NULL_ERR(params)

   if (ifc->specific_params.file->mode != NULL) {
      params = ifc_param_concat(params, ifc->specific_params.file->mode, 0);
      IF_NO_MEM_NULL_ERR(params)
   }

   if (ifc->specific_params.file->size > 0) {
      params = ifc_param_concat(params, NULL, ifc->specific_params.file->size);
      IF_NO_MEM_NULL_ERR(params)
   }

   if (ifc->specific_params.file->time > 0) {
      params = ifc_param_concat(params, NULL, ifc->specific_params.file->time);
      IF_NO_MEM_NULL_ERR(params)
   }

   return params;
}

static char * bh_ifc_to_cli_arg(const interface_t *ifc)
{
   char *params = strdup("b");
   IF_NO_MEM_NULL_ERR(params)

   return params;
}


static char **module_params_to_ifc_arr(const run_module_t *module,
                                       uint32_t *params_num,
                                       int *rc)
{
   uint32_t params_arr_size = 5,
         params_cnt = 0;
   uint32_t x = 0,
         y = 0,
         act_param_len = 0;
   char *buffer = NULL;
   uint32_t params_len = (uint32_t) strlen(module->params);
   char **params = NULL;

   *rc = 0;
   params = (char **) calloc(params_arr_size, sizeof(char *));
   if (params == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   if (params_len < 1) {
      VERBOSE(MOD_EVNT, "[WARNING] Empty string in '%s' params element.", module->name)
      goto err_cleanup;
   }

   buffer = (char *) calloc(params_len + 1, sizeof(char));
   if (buffer == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   for (x = 0; x < params_len; x++) {
      switch(module->params[x]) {
         // parameter in apostrophes
         case '\'':
         {
            if (act_param_len > 0) { // check whether the ''' character is not in the middle of the word
               VERBOSE(MOD_EVNT, "[ERROR] Bad format of '%s' params element - used \'\'\' in the middle of the word.", module->name);
               goto err_cleanup;
            }

            for (y = (x + 1); y < params_len; y++) {
               if (module->params[y] == '\'') { // parameter in apostrophes MATCH
                  if (act_param_len == 0) { // check for empty apostrophes
                     VERBOSE(MOD_EVNT, "[ERROR] Bad format of '%s' params element - used empty apostrophes.", module->name);
                     goto err_cleanup;
                  }
                  x = y;
                  goto add_param;
               } else { // add character to parameter in apostrophes
                  buffer[act_param_len] = module->params[y];
                  act_param_len++;
               }
            }
            // the terminating ''' was not found
            VERBOSE(MOD_EVNT, "[ERROR] Bad format of '%s' params element - used single \'\'\'.\n", module->name);
            goto err_cleanup;
            break;
         }

         // parameter in quotes
         case '\"':
         {
            if (act_param_len > 0) { // check whether the '"' character is not in the middle of the word
               VERBOSE(MOD_EVNT, "[ERROR] Bad format of '%s' params element - used \'\"\' in the middle of the word.", module->name);
               goto err_cleanup;
            }

            for (y = (x + 1); y < params_len; y++) {
               if (module->params[y] == '\"') { // parameter in quotes MATCH
                  if (act_param_len == 0) { // check for empty quotes
                     VERBOSE(MOD_EVNT, "[ERROR] Bad format of '%s' params element - used empty quotes.\n", module->name);
                     goto err_cleanup;
                  }
                  x = y;
                  goto add_param;
               } else if (module->params[y] != '\'') { // add character to parameter in quotes
                  buffer[act_param_len] = module->params[y];
                  act_param_len++;
               } else {
                  VERBOSE(MOD_EVNT, "[ERROR] Found apostrophe in '%s' params element in quotes.", module->name);
                  goto err_cleanup;
               }
            }
            // the terminating '"' was not found
            VERBOSE(MOD_EVNT, "[ERROR] Bad format of '%s' params element - used single \'\"\'.", module->name)
            goto err_cleanup;
            break;
         }

         // parameter delimiter
         case ' ':
         {
            if (act_param_len == 0) {
               continue; // skip white-spaces between parameters
            }

         add_param:
            if (params_cnt == params_arr_size) {
               // if needed, resize the array of parsed parameters
               params_arr_size += params_arr_size;
               params = (char **) realloc(params, sizeof(char *) * params_arr_size);
               memset(params + (params_arr_size / 2), 0, ((params_arr_size / 2) * sizeof(char *)));
            }

            params[params_cnt] = strdup(buffer);
            params_cnt++;
            memset(buffer, 0, (params_len + 1) * sizeof(char));
            act_param_len = 0;
            break;
         }

         // adding one character to parameter out of quotes and apostrophes
         default:
         {
            buffer[act_param_len] = module->params[x];
            act_param_len++;

            if (x == (params_len - 1)) { // if last character of the params element was added, add current module parameter to the params array
               goto add_param;
            }
            break;
         }

      } // end of switch
   }

   if (params_cnt == 0) {
      goto err_cleanup;
   }

   *params_num = params_cnt;
   NULLP_TEST_AND_FREE(buffer);

   return params;

err_cleanup:
   if (params != NULL) {
      for (x = 0; x < params_cnt; x++) {
         NULLP_TEST_AND_FREE(params[x]);
      }
      NULLP_TEST_AND_FREE(params)
   }
   NULLP_TEST_AND_FREE(buffer);
   *params_num = 0;
   *rc = -1;

   return NULL;
}
/* --END local fns-- */