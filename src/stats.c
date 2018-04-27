
#include "stats.h"
#include "module.h"
#include <sysrepo/values.h>
#include <sysrepo/xpath.h>

typedef struct tree_path_s {
   char *inst;
   char *ifc;
} tree_path_t;

/**
 * TODO
 * */
static int set_new_sr_val(sr_val_t *new_sr_val,
                          const char *stat_xpath,
                          const char *stat_leaf_name,
                          sr_type_t val_type,
                          void *val_data);

static interface_t * interface_get_by_xpath(const char *xpath);

static void tree_path_free(tree_path_t * tpath);
static tree_path_t * tree_path_load(const char *xpath);


int interface_get_stats_cb(const char *xpath,
                           sr_val_t **values,
                           size_t *values_cnt,
                           void *private_ctx)
{
   VERBOSE(V3, "Request for interface stats at xpath=%s", xpath)

   int rc;
   uint8_t vals_cnt;
   interface_t *ifc = NULL;
   sr_val_t *new_vals = NULL;

   ifc = interface_get_by_xpath(xpath);
   if (ifc == NULL) {
      VERBOSE(N_ERR, "Interface at '%s' is not loaded", xpath)
      rc = SR_ERR_NOT_FOUND;
      goto err_cleanup;
   }

   if (ifc->direction == NS_IF_DIR_IN) {
      vals_cnt = 2;
      ifc_in_stats_t *stats = ifc->stats;

      rc = sr_new_values(vals_cnt, &new_vals);
      if (rc != SR_ERR_OK) {
         VERBOSE(N_ERR, "Failed create stats output values: %s", sr_strerror(rc));
         goto err_cleanup;
      }

      rc = set_new_sr_val(&new_vals[0], xpath, "recv-msg-cnt", SR_UINT64_T,
                          &stats->recv_msg_cnt);
      if (rc != 0) {
         VERBOSE(N_ERR, "Setting node value for /recv-msg-cnt failed")
         goto err_cleanup;
      }

      rc = set_new_sr_val(&new_vals[1], xpath, "recv-buff-cnt", SR_UINT64_T,
                          &stats->recv_buff_cnt);
      if (rc != 0) {
         VERBOSE(N_ERR, "Setting node value for /recv-buff-cnt failed")
         goto err_cleanup;
      }
   } else { // ifc->direction == NS_IF_DIR_OUT
      vals_cnt = 4;
      ifc_out_stats_t *stats = ifc->stats;

      rc = sr_new_values(vals_cnt, &new_vals);
      if (rc != SR_ERR_OK) {
         VERBOSE(N_ERR, "Failed create stats output values: %s", sr_strerror(rc));
         goto err_cleanup;
      }

      rc = set_new_sr_val(&new_vals[0], xpath, "sent-msg-cnt", SR_UINT64_T,
                          &stats->sent_msg_cnt);
      if (rc != 0) {
         VERBOSE(N_ERR, "Setting node value for /sent-msg-cnt failed")
         goto err_cleanup;
      }

      rc = set_new_sr_val(&new_vals[1], xpath, "sent-buff-cnt", SR_UINT64_T,
                          &stats->sent_buff_cnt);
      if (rc != 0) {
         VERBOSE(N_ERR, "Setting node value for /sent-buff-cnt failed")
         goto err_cleanup;
      }

      rc = set_new_sr_val(&new_vals[2], xpath, "dropped-msg-cnt", SR_UINT64_T,
                          &stats->dropped_msg_cnt);
      if (rc != 0) {
         VERBOSE(N_ERR, "Setting node value for /dropped-msg-cnt failed")
         goto err_cleanup;
      }

      rc = set_new_sr_val(&new_vals[3], xpath, "autoflush-cnt", SR_UINT64_T,
                          &stats->autoflush_cnt);
      if (rc != 0) {
         VERBOSE(N_ERR, "Setting node value for /autoflush-cnt failed")
         goto err_cleanup;
      }
   }

   *values_cnt = vals_cnt;
   *values = new_vals;

   VERBOSE(V3, "Successfully leaving interface_get_stats_cb")
   return SR_ERR_OK;

err_cleanup:
   if (new_vals != NULL) {
      sr_free_values(new_vals, *values_cnt);
   }

   VERBOSE(N_ERR, "Retrieving stats for xpath=%s failed.", xpath)
   return rc;
}

int inst_get_stats_cb(const char *xpath,
                      sr_val_t **values,
                      size_t *values_cnt,
                      void *private_ctx)
{
   VERBOSE(V3, "Request for instance stats at xpath=%s", xpath)

   int rc;
   uint8_t vals_cnt = 6;
   tree_path_t *tpath = NULL;
   inst_t *inst = NULL;
   sr_val_t *new_vals = NULL;
   time_t time_now;
   uint8_t restarts_cnt = 0;

   tpath = tree_path_load(xpath);
   if (tpath == NULL) {
      NO_MEM_ERR
      rc = SR_ERR_NOMEM;
      goto err_cleanup;
   }

   rc = sr_new_values(vals_cnt, &new_vals);
   if (rc != SR_ERR_OK) {
      VERBOSE(N_ERR, "Failed create stats output values: %s", sr_strerror(rc));
      goto err_cleanup;
   }

   VERBOSE(V3, "Stats requested for inst '%s'", tpath->inst)

   inst = inst_get_by_name(tpath->inst, NULL);
   if (inst == NULL) {
      VERBOSE(N_ERR, "Instance '%s' was not found for stats data.", tpath->inst)
      rc = SR_ERR_NOT_FOUND;
      goto err_cleanup;
   }
   tree_path_free(tpath);

   rc = set_new_sr_val(&new_vals[0], xpath, "running", SR_BOOL_T, &inst->running);
   if (rc != 0) {
      VERBOSE(N_ERR, "Setting node value for /running failed")
      goto err_cleanup;
   }

   {
      /* Find out whether it was more than a minute since last restart and if it was,
       * reset restarts_cnt to provide correct value to the caller */
      time(&time_now);
      if (time_now - inst->restart_time <= 60) {
         restarts_cnt = inst->restarts_cnt;
      }
   }

   rc = set_new_sr_val(&new_vals[1], xpath, "restart-counter", SR_UINT8_T,
                       &restarts_cnt);
   if (rc != 0) {
      VERBOSE(N_ERR, "Setting node value for /restart-counter failed")
      goto err_cleanup;
   }

   rc = set_new_sr_val(&new_vals[2], xpath, "cpu-user", SR_UINT64_T,
                       &inst->last_cpu_perc_umode);
   if (rc != 0) {
      VERBOSE(N_ERR, "Setting node value for /cpu-user failed")
      goto err_cleanup;
   }

   rc = set_new_sr_val(&new_vals[3], xpath, "cpu-kern", SR_UINT64_T,
                       &inst->last_cpu_perc_kmode);
   if (rc != 0) {
      VERBOSE(N_ERR, "Setting node value for /cpu-kern failed")
      goto err_cleanup;
   }

   rc = set_new_sr_val(&new_vals[4], xpath, "mem-vms", SR_UINT64_T, &inst->mem_vms);
   if (rc != 0) {
      VERBOSE(N_ERR, "Setting node value for /mem-vms failed")
      goto err_cleanup;
   }

   rc = set_new_sr_val(&new_vals[5], xpath, "mem-rss", SR_UINT64_T, &inst->mem_rss);
   if (rc != 0) {
      VERBOSE(N_ERR, "Setting node value for /mem-rss failed")
      goto err_cleanup;
   }

   *values_cnt = vals_cnt;
   *values = new_vals;
   VERBOSE(V3, "Successfully leaving inst_get_stats_cb")
   return SR_ERR_OK;

err_cleanup:
   if (new_vals != NULL) {
      sr_free_values(new_vals, *values_cnt);
   }
   tree_path_free(tpath);

   VERBOSE(N_ERR, "Retrieving stats for xpath=%s failed.", xpath)

   return rc;
}

static int set_new_sr_val(sr_val_t *new_sr_val,
                          const char *stat_xpath,
                          const char *stat_leaf_name,
                          sr_type_t val_type,
                          void *val_data)
{
   int rc;
   char *stat_leaf_xpath = NULL;
   uint64_t stat_xpath_len = strlen(stat_xpath) + strlen(stat_leaf_name) + 2;

   stat_leaf_xpath = (char *) calloc(stat_xpath_len, sizeof(char));
   if (stat_leaf_xpath == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   sprintf(stat_leaf_xpath, "%s/%s", stat_xpath, stat_leaf_name);
   rc = sr_val_set_xpath(new_sr_val, stat_leaf_xpath);
   if (rc != 0) {
      VERBOSE(N_ERR, "Failed to set output stats value xpath=%s", stat_leaf_xpath)
      goto err_cleanup;
   }

   switch (val_type) {
      case SR_BOOL_T:
         new_sr_val->type = SR_BOOL_T;
         new_sr_val->data.bool_val = *(bool *)  val_data;
         break;
      case SR_UINT8_T:
         new_sr_val->type = SR_UINT8_T;
         new_sr_val->data.uint8_val = *(uint8_t *) val_data;
         break;
      case SR_UINT64_T:
         new_sr_val->type = SR_UINT64_T;
         new_sr_val->data.uint64_val = *(uint64_t *) val_data;
         break;

      default:
         break;
   }

   return 0;

err_cleanup:
   NULLP_TEST_AND_FREE(stat_leaf_xpath)
   VERBOSE(N_ERR, "set_new_sr_val failed")

   return -1;
}

static tree_path_t * tree_path_load(const char *xpath)
{
   char * dyn_xpath;
   char * res;
   tree_path_t *tpath = NULL;
   sr_xpath_ctx_t state = {0};

   // sysrepo xpath functions require xpath to be dynamically allocated
   dyn_xpath = strdup(xpath);
   if (dyn_xpath == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   tpath = calloc(1, sizeof(tree_path_t));
   if (tpath == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }
   tpath->inst = NULL;
   tpath->ifc = NULL;

   res = sr_xpath_next_node(dyn_xpath, &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to parse tree_path_load on line %d", __LINE__)
      goto err_cleanup;
   }

   res = sr_xpath_next_node(NULL, &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to parse tree_path_load on line %d", __LINE__)
      goto err_cleanup;
   }

   res = sr_xpath_node_key_value(NULL, "name", &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to parse tree_path_load on line %d", __LINE__)
      goto err_cleanup;
      return NULL;
   }
   tpath->inst = strdup(res);
   if (tpath->inst == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   // /stats OR /interface[name='xxxx']
   res = sr_xpath_next_node(NULL, &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to parse tree_path_load on line %d", __LINE__)
      goto err_cleanup;
   }

   if (strcmp(res, "stats") == 0) {
      // No /interface node, return
      sr_xpath_recover(&state);
      NULLP_TEST_AND_FREE(dyn_xpath)
      return tpath;
   }

   res = sr_xpath_node_key_value(NULL, "name", &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to parse tree_path_load on line %d", __LINE__)
      goto err_cleanup;
      return NULL;
   }
   tpath->ifc = strdup(res);
   if (tpath->ifc == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   return tpath;

err_cleanup:
   sr_xpath_recover(&state);
   NULLP_TEST_AND_FREE(dyn_xpath)
   tree_path_free(tpath);
   VERBOSE(N_ERR, "Failed tree_path_load with XPATH=%s", xpath)

   return NULL;
}

static void tree_path_free(tree_path_t * tpath)
{
   if (tpath != NULL) {
      NULLP_TEST_AND_FREE(tpath->inst)
      NULLP_TEST_AND_FREE(tpath->ifc)
      NULLP_TEST_AND_FREE(tpath)
   }
}

static interface_t * interface_get_by_xpath(const char *xpath)
{
   tree_path_t *tpath = NULL;
   inst_t *inst = NULL;
   interface_t *ifc = NULL;

   tpath = tree_path_load(xpath);
   if (tpath == NULL) {
      goto err_cleanup;
   }
   if (tpath->ifc == NULL) {
      VERBOSE(V2, "No interface parsed from given XPATH")
      goto err_cleanup;
   }

   inst = inst_get_by_name(tpath->inst, NULL);
   if (inst == NULL) {
      VERBOSE(V2, "No instance named '%s' is loaded in supervisor", tpath->inst)
      goto err_cleanup;
   }

   vector_t *ifces_vec[2] = { &(inst->in_ifces), &(inst->out_ifces) };

   for (uint32_t j = 0; j < 2; j++) {
      for (uint32_t i = 0; i < ifces_vec[j]->total; i++) {
         ifc = ifces_vec[j]->items[i];

         if (strcmp(ifc->name, tpath->ifc) == 0) {
            tree_path_free(tpath);
            return ifc;
         }
      }
   }

   return ifc;

err_cleanup:
   tree_path_free(tpath);
   VERBOSE(N_ERR, "Failed interface_get_by_xpath with XPATH=%s", xpath)

   return NULL;
}