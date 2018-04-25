#include <stdarg.h>
#include <libtrap/trap.h>
#include <sysrepo/xpath.h>
#include "module.h"

pthread_mutex_t config_lock; ///< Mutex for operations on m_groups_ll and modules_ll

vector_t avmods_v = {.total = 0, .capacity = 0, .items = NULL};
vector_t insts_v = {.total = 0, .capacity = 0, .items = NULL};

static char * tcp_ifc_to_cli_arg(const interface_t *ifc);
static char * tcp_tls_ifc_to_cli_arg(const interface_t *ifc);
static char * unix_ifc_to_cli_arg(const interface_t *ifc);
static char * file_ifc_to_cli_arg(const interface_t *ifc);
static char * bh_ifc_to_cli_arg(const interface_t *ifc);
static inline char * module_get_ifcs_as_arg(inst_t *inst);

static char * strcat_many(uint8_t cnt, ...);

static char **module_params_to_ifc_arr(const inst_t *module, uint32_t *params_num,
                                       int *rc);
static char *ifc_param_concat(char *base, char *param_s, uint16_t param_u);
static inline void interface_specific_params_free(interface_t *ifc);


int inst_interface_add(inst_t *inst, interface_t *ifc)
{
   int rc;

   if (ifc->direction == NS_IF_DIR_IN) {
      rc = vector_add(&inst->in_ifces, ifc);
   } else {
      rc = vector_add(&inst->out_ifces, ifc);
   }

   if (rc != 0) {
      NO_MEM_ERR
      return rc;
   }

   return 0;
}

void av_module_print(const av_module_t *mod)
{
   VERBOSE(V3, "Module: %s (path=%s)", mod->name, mod->path)
   VERBOSE(V3, " trap monitorable=%d", mod->trap_mon)
   VERBOSE(V3, " use trap interfaces=%d", mod->use_trap_ifces)
   VERBOSE(V3, " sysrepo ready=%d", mod->sr_rdy)
   VERBOSE(V3, " sysrepo custom model=%s", mod->sr_model)
}
void inst_print(inst_t *inst)
{
   VERBOSE(V3, "Module instance: %s of %s", inst->name, inst->mod_ref->name)
   VERBOSE(V3, " params=%s", inst->params)
   VERBOSE(V3, " pid=%d", inst->pid)
   VERBOSE(V3, " enabled=%s", inst->enabled ? "true" : "false")
   VERBOSE(V3, " running=%s", inst->running ? "true" : "false")
   VERBOSE(V3, " restart_cnt=%d", inst->restarts_cnt)
   VERBOSE(V3, " max_restarts_minute=%d", inst->max_restarts_minute)

   for (uint32_t i = 0; i < inst->in_ifces.total; i++) {
      print_ifc(inst->in_ifces.items[i]);
   }
}

void print_ifc(interface_t *ifc)
{
   VERBOSE(V3, " IFC DIR=%d TYPE=%d", ifc->direction, ifc->type)
   VERBOSE(V3, "  buffer=%s", ifc->buffer)
   VERBOSE(V3, "  autoflush=%s", ifc->autoflush)
   VERBOSE(V3, "  timeout=%s", ifc->timeout)

   switch (ifc->type) {
      case NS_IF_TYPE_TCP:
         VERBOSE(V3, "  host=%s", ifc->specific_params.tcp->host)
         VERBOSE(V3, "  max_clients=%d", ifc->specific_params.tcp->max_clients)
         VERBOSE(V3, "  port=%d", ifc->specific_params.tcp->port)
         break;
      case NS_IF_TYPE_TCP_TLS:
         VERBOSE(V3, "  host=%s", ifc->specific_params.tcp_tls->host)
         VERBOSE(V3, "  max_clients=%d", ifc->specific_params.tcp_tls->max_clients)
         VERBOSE(V3, "  port=%d", ifc->specific_params.tcp_tls->port)
         VERBOSE(V3, "  certfile=%s", ifc->specific_params.tcp_tls->certfile)
         VERBOSE(V3, "  keyfile=%s", ifc->specific_params.tcp_tls->keyfile)
         VERBOSE(V3, "  cafile=%s", ifc->specific_params.tcp_tls->cafile)
         VERBOSE(V3, "  ")
         break;
      case NS_IF_TYPE_FILE:
         VERBOSE(V3, "  name=%s", ifc->specific_params.file->name)
         VERBOSE(V3, "  mode=%s", ifc->specific_params.file->mode)
         VERBOSE(V3, "  size=%d", ifc->specific_params.file->size)
         VERBOSE(V3, "  time=%d", ifc->specific_params.file->time)
         break;
      case NS_IF_TYPE_UNIX:
         VERBOSE(V3, "  socket_name=%s", ifc->specific_params.nix->socket_name)
         VERBOSE(V3, "  max_clients=%d", ifc->specific_params.nix->max_clients)
         break;
      case NS_IF_TYPE_BH:
         break;
   }

}

av_module_t * av_module_alloc()
{
   av_module_t * mod = (av_module_t *) calloc(1, sizeof(av_module_t));
   IF_NO_MEM_NULL_ERR(mod)

   mod->name = NULL;
   mod->path = NULL;
   mod->sr_model = NULL;
   mod->sr_rdy = false;
   mod->trap_mon = false;
   mod->use_trap_ifces = false;

   return mod;
}

inst_t * inst_alloc()
{
   inst_t * inst = (inst_t *) calloc(1, sizeof(inst_t));
   IF_NO_MEM_NULL_ERR(inst)

   inst->enabled = false;
   inst->use_sysrepo = false;
   inst->running = false;
   inst->should_die = false;
   inst->root_perm_needed = false;
   inst->is_my_child = false;
   inst->sigint_sent = false;
   inst->service_ifc_connected = false;
   inst->name = NULL;
   inst->params = NULL;
   inst->exec_args = NULL;
   inst->mod_ref = NULL;
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

void av_modules_free()
{
   for (uint32_t i = 0; i < avmods_v.total; i++) {
      av_module_free(avmods_v.items[i]);
   }
   vector_free(&avmods_v);
}

void insts_free()
{
   for (uint32_t i = 0; i < insts_v.total; i++) {
      inst_free(insts_v.items[i]);
   }
   vector_free(&insts_v);
}

void inst_clear_socks(inst_t *inst)
{
   // Clean inst's socket files
   char buffer[DEFAULT_SIZE_OF_BUFFER];
   char service_sock_spec[14];
   interface_t *ifc = NULL;

   if (inst->mod_ref->use_trap_ifces) {
      for (uint32_t i = 0; i < inst->out_ifces.total; i++) {
         ifc = inst->out_ifces.items[i];

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
   }

   // Delete unix-socket created by module's service interface
   if (inst->mod_ref->trap_mon && inst->pid > 0) {
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
   NULLP_TEST_AND_FREE(mod->sr_model)
   NULLP_TEST_AND_FREE(mod)
}

void inst_free(inst_t *inst)
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

void interfaces_free(inst_t *module)
{
   interface_t *cur_ifc = NULL;
   vector_t *ifces_vec[2] = { &module->in_ifces, &module->out_ifces};

   for (int j = 0; j < 2; j++) {
      for (int i = 0; i < ifces_vec[j]->total; i++) {
         cur_ifc = ifces_vec[j]->items[i];
         interface_free(cur_ifc);
      }
      vector_free(ifces_vec[j]);
   }
}

void interface_free(interface_t *ifc)
{
   if (ifc != NULL) {
      NULLP_TEST_AND_FREE(ifc->name);
      NULLP_TEST_AND_FREE(ifc->buffer);
      NULLP_TEST_AND_FREE(ifc->autoflush);
      NULLP_TEST_AND_FREE(ifc->timeout);
      ifc->ifc_to_cli_arg_fn = NULL;
      interface_stats_free(ifc);
      interface_specific_params_free(ifc);

      NULLP_TEST_AND_FREE(ifc);
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

inst_t * inst_get_by_name(const char *name, uint32_t *index)
{
   uint32_t fi; // Index of found instance
   inst_t *inst = NULL;

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

static inline void interface_specific_params_free(interface_t *ifc)
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
int inst_gen_exec_args(inst_t *inst)
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
   if (inst->mod_ref->use_trap_ifces && total_ifc_cnt > 0) {
      exec_args_cnt += 2;
   }

   if (inst->use_sysrepo) {
      // prepare space for '-x' and 'XPATH' arguments
      exec_args_cnt += 2;
   } else if (inst->params != NULL) {
      // if the inst has non-empty params, try to parse them
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

   if (inst->use_sysrepo) {
      char *xpath = NULL;
      size_t xpath_len = strlen(inst->mod_ref->sr_model) + strlen(inst->name) + 20;

      // add -x "XPATH_TO_CUSTOM_SYSREPO_MODEL_INSTANCE_CONFIGURATION"
      exec_args[exec_args_pos] = strdup("-x"); // It's easier for freeing later
      if (exec_args[exec_args_pos] == NULL) {
         NO_MEM_ERR
         goto err_cleanup;
      }
      exec_args_pos++;

      xpath = calloc(1, (xpath_len) * sizeof(char));
      if (xpath == NULL) {
         NO_MEM_ERR
         goto err_cleanup;
      }
      sprintf(xpath, "/%s:instance[name='%s']", inst->mod_ref->sr_model, inst->name);
      exec_args[exec_args_pos] = xpath;
      exec_args_pos++;
   } else {
      // copy already allocated inst params strings returned by module_params_to_ifc_arr function
      if (cfg_params != NULL && module_params_num > 0) {
         for (int i = 0; i < module_params_num; i++) {
            exec_args[exec_args_pos] = cfg_params[i];
            exec_args_pos++;
         }
         NULLP_TEST_AND_FREE(cfg_params)
      }
   }

   // Do not generate -i args
   if (inst->mod_ref->use_trap_ifces == false || total_ifc_cnt == 0) {
      inst->exec_args = exec_args;
      return 0;
   }

   ifc_spec = module_get_ifcs_as_arg(inst);
   if (ifc_spec == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   exec_args[exec_args_pos] = strdup("-i"); // It's easier for freeing later
   if (exec_args[exec_args_pos] == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }
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


static inline char * module_get_ifcs_as_arg(inst_t *inst)
{
   char *ifc_spec = NULL;
   char *new_ifc_spec = NULL;
   char *cur_ifc_params = NULL;
   char *tmp = NULL;
   interface_t *cur_ifc;

   vector_t *ifces_vec[2] = { &(inst->in_ifces), &(inst->out_ifces) };

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

// Only param_s or param_u should be passed, base is freed
static char * ifc_param_concat(char *base, char *param_s, uint16_t param_u)
{
   char *result = NULL;
   char *param = NULL; // Converted parameter to be concatenated
   uint16_t len;
   bool integer_passed = (param_s == NULL);

   if (integer_passed) {
      len = (uint16_t) snprintf(NULL, 0, "%d", param_u);
      param = (char *) calloc(len + 1, sizeof(char));
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

static char * strcat_many(uint8_t cnt, ...)
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


static char **module_params_to_ifc_arr(const inst_t *module,
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
      VERBOSE(V2, "Empty string in '%s' params element.", module->name)
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
               VERBOSE(V2, "Bad format of '%s' params element - used \'\'\' in the middle of the word.", module->name);
               goto err_cleanup;
            }

            for (y = (x + 1); y < params_len; y++) {
               if (module->params[y] == '\'') { // parameter in apostrophes MATCH
                  if (act_param_len == 0) { // check for empty apostrophes
                     VERBOSE(V2, "Bad format of '%s' params element - used empty apostrophes.", module->name);
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
            VERBOSE(V2, "Bad format of '%s' params element - used single \'\'\'.\n", module->name);
            goto err_cleanup;
            break;
         }

         // parameter in quotes
         case '\"':
         {
            if (act_param_len > 0) { // check whether the '"' character is not in the middle of the word
               VERBOSE(V2, "Bad format of '%s' params element - used \'\"\' in the middle of the word.", module->name);
               goto err_cleanup;
            }

            for (y = (x + 1); y < params_len; y++) {
               if (module->params[y] == '\"') { // parameter in quotes MATCH
                  if (act_param_len == 0) { // check for empty quotes
                     VERBOSE(V2, "Bad format of '%s' params element - used empty quotes.\n", module->name);
                     goto err_cleanup;
                  }
                  x = y;
                  goto add_param;
               } else if (module->params[y] != '\'') { // add character to parameter in quotes
                  buffer[act_param_len] = module->params[y];
                  act_param_len++;
               } else {
                  VERBOSE(V2, "Found apostrophe in '%s' params element in quotes.", module->name);
                  goto err_cleanup;
               }
            }
            // the terminating '"' was not found
            VERBOSE(V2, "Bad format of '%s' params element - used single \'\"\'.", module->name)
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
