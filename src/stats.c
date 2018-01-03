
#include "stats.h"
#include "module.h"
#include "utils.h"
#include <sysrepo/values.h>
#include <sysrepo/xpath.h>

/*
in_ifces [{type,
 ID, modules_ll[x].in_ifces_data[y].id
 "is-conn", modules_ll[x].in_ifces_data[y].state
 "messages", recv_msg_cnt
 "buffers" recv_buff_cnt
 }]
sssssisIsI

out_ifces [{type,
 ID, id modules_ll[x].in_ifces_data[y].id
 cli-num, num_clients
 sent-msg, sent_msg_cnt
 drop-msg, dropped_msg_cnt
 buffers, sent_buff_cnt
 autoflush autoflush_cnt
 }]
 sssssisIsIsIsI

module [{idx,
 params,
 path,
 status,
 CPU-u, last_period_percent_cpu_usage_user_mode
 CPU-s, last_period_percent_cpu_usage_kernel_mode
 MEM-vms, mem_vms
 MEM-rss, mem_rss * 1024
 inputs:[in_ifces],
 outputs:[out_ifces]}]
 sisssssssisisIsIsoso

module [{CPU-u, last_period_percent_cpu_usage_user_mode
 CPU-s, last_period_percent_cpu_usage_kernel_mode
 MEM-vms, mem_vms
 MEM-rss, mem_rss * 1024
 inputs:[in_ifces], outputs:[out_ifces]}]
 sisisIsIsoso

{
 "module_name": module
}





 */

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
 * TODO
 * */
static int set_new_sr_val(sr_val_t *new_sr_val,
                          const char *stat_xpath,
                          const char *stat_leaf_name,
                          sr_type_t val_type,
                          void *val_data);
/* --END full fns prototypes-- */

/* --BEGIN superglobal fns-- */


int interface_get_stats_cb(const char *xpath,
                           sr_val_t **values,
                           size_t *values_cnt,
                           void *private_ctx)
{
   VERBOSE(V3, "Entering interface_get_stats_cb")
   VERBOSE(V3, "xpath=%s", xpath)

   int rc;
   tree_path_t path;
   uint8_t vals_cnt;
   interface_t *ifc = NULL;
   sr_val_t *new_vals = NULL;

   tree_path_init(&path);
   rc = tree_path_load_from_xpath(&path, xpath);
   if (rc != 0) {
      VERBOSE(N_ERR, "") // TODO
      goto err_cleanup;
   }

   ifc = interface_get_by_path(&path);
   if (ifc == NULL) {
      VERBOSE(N_ERR, "Interface at %s is not loaded", xpath)
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
         VERBOSE(N_ERR, "") // TODO
         goto err_cleanup;
      }

      rc = set_new_sr_val(&new_vals[1], xpath, "recv-buff-cnt", SR_UINT64_T,
                          &stats->recv_buff_cnt);
      if (rc != 0) {
         VERBOSE(N_ERR, "") // TODO
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
         VERBOSE(N_ERR, "") // TODO
         goto err_cleanup;
      }

      rc = set_new_sr_val(&new_vals[1], xpath, "sent-buff-cnt", SR_UINT64_T,
                          &stats->sent_buff_cnt);
      if (rc != 0) {
         VERBOSE(N_ERR, "") // TODO
         goto err_cleanup;
      }

      rc = set_new_sr_val(&new_vals[2], xpath, "dropped-msg-cnt", SR_UINT64_T,
                          &stats->dropped_msg_cnt);
      if (rc != 0) {
         VERBOSE(N_ERR, "") // TODO
         goto err_cleanup;
      }

      rc = set_new_sr_val(&new_vals[3], xpath, "autoflush-cnt", SR_UINT64_T,
                          &stats->autoflush_cnt);
      if (rc != 0) {
         VERBOSE(N_ERR, "") // TODO
         goto err_cleanup;
      }
   }

   *values_cnt = vals_cnt;
   *values = new_vals;

   tree_path_free(&path);

   VERBOSE(V3, "Successfully leaving inst_get_stats_cb")
   return SR_ERR_OK;

err_cleanup:
   tree_path_free(&path);
   return rc;
}

int inst_get_stats_cb(const char *xpath,
                      sr_val_t **values,
                      size_t *values_cnt,
                      void *private_ctx)
{
   VERBOSE(V3, "Entering inst_get_stats_cb")
   VERBOSE(V3, "xpath=%s", xpath)

   int rc;
   uint8_t vals_cnt = 6;
   instance_t *inst = NULL;
   tree_path_t tree_path;
   sr_val_t *new_vals = NULL;
   time_t time_now;

   tree_path_init(&tree_path);

   rc = sr_new_values(vals_cnt, &new_vals);
   if (rc != SR_ERR_OK) {
      VERBOSE(N_ERR, "Failed create stats output values: %s", sr_strerror(rc));
      goto err_cleanup;
   }

   rc = tree_path_load_from_xpath(&tree_path, xpath);
   if (rc != 0) {
      goto err_cleanup;
   }
   VERBOSE(V3, "Stats requested for inst '%s'", tree_path.inst)

   inst = instance_get_by_name(tree_path.inst, NULL);
   if (inst == NULL) {
      VERBOSE(N_ERR, "Instance '%s' was not found for stats data.", tree_path.inst)
      goto err_cleanup;
   }
   tree_path_free(&tree_path);


   rc = set_new_sr_val(&new_vals[0], xpath, "running", SR_BOOL_T, &inst->running);
   if (rc != 0) {
      VERBOSE(N_ERR, "") // TODO
      goto err_cleanup;
   }

   {
      /* Find out whether it was more than a minute since last restart and if it was,
       * reset restarts_cnt to provide correct value to the caller */
      time(&time_now);
      if (time_now - inst->restart_time > 60) {
         inst->restarts_cnt = 0;
      }
   }

   rc = set_new_sr_val(&new_vals[1], xpath, "restart-counter", SR_UINT8_T,
                       &inst->restarts_cnt);
   if (rc != 0) {
      VERBOSE(N_ERR, "")
      goto err_cleanup;
   }

   rc = set_new_sr_val(&new_vals[2], xpath, "cpu-user", SR_UINT64_T,
                       &inst->last_cpu_umode);
   if (rc != 0) {
      VERBOSE(N_ERR, "")
      goto err_cleanup;
   }

   rc = set_new_sr_val(&new_vals[3], xpath, "cpu-kern", SR_UINT64_T,
                       &inst->last_cpu_kmode);
   if (rc != 0) {
      VERBOSE(N_ERR, "")
      goto err_cleanup;
   }

   rc = set_new_sr_val(&new_vals[4], xpath, "mem-vms", SR_UINT64_T, &inst->mem_vms);
   if (rc != 0) {
      VERBOSE(N_ERR, "")
      goto err_cleanup;
   }

   rc = set_new_sr_val(&new_vals[5], xpath, "mem-rss", SR_UINT64_T, &inst->mem_rss);
   if (rc != 0) {
      VERBOSE(N_ERR, "")
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

   tree_path_free(&tree_path);

   return rc;
}
/* --END superglobal fns-- */

/* --BEGIN local fns-- */
static int set_new_sr_val(sr_val_t *new_sr_val,
                          const char *stat_xpath,
                          const char *stat_leaf_name,
                          sr_type_t val_type,
                          void *val_data)
{
   int rc;
   char *stat_leaf_xpath = NULL;
   uint64_t stat_xpath_len = strlen(stat_xpath) + strlen(stat_leaf_name) + 2;

   stat_leaf_xpath = (char *) calloc(1, sizeof(char) * (stat_xpath_len));
   if (stat_leaf_xpath == NULL) {
      NO_MEM_ERR
      goto err_cleanup;
   }

   sprintf(stat_leaf_xpath, "%s/%s", stat_xpath, stat_leaf_name);
   VERBOSE(V3, "stat_leaf_xpath=%s", stat_leaf_xpath)
   rc = sr_val_set_xpath(new_sr_val, stat_leaf_xpath);
   if (rc != 0) {
      VERBOSE(N_ERR, "") // TODO
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

   return -1;
}


/* --END local fns-- */
