#include <sysrepo.h>
#include <pthread.h>

#include "testing_utils.h"
#include "../src/module.h"
#include "../src/conf.h"
#include "../src/stats.c"

const char *ns_root_sr_path = "/nemea-tests-1:nemea-supervisor";
bool sv_thread_running = false;
int stats_callback_cnt = 0;

/**
 * @brief Mocked sr_conn_link_t from sup.c
 * */
typedef struct sr_conn_link_s {
   sr_conn_ctx_t *conn;
   sr_session_ctx_t *sess;
   sr_subscription_ctx_t *subscr;
} sr_conn_link_t;
sr_conn_link_t sr_conn_link;

void connect_to_sr()
{
   int rc;

   // Connect to sysrepo
   rc = sr_connect("test_stats.c", SR_CONN_DEFAULT, &sr_conn_link.conn);
   IF_SR_ERR_FAIL(rc)

   rc = sr_session_start(sr_conn_link.conn, SR_DS_STARTUP, SR_SESS_CONFIG_ONLY,
                         &sr_conn_link.sess);
   IF_SR_ERR_FAIL(rc)


}
void disconnect_sr()
{
   if (sr_conn_link.subscr != NULL) {
      sr_unsubscribe(sr_conn_link.sess, sr_conn_link.subscr);
   }

   if (sr_conn_link.sess != NULL) {
      sr_session_stop(sr_conn_link.sess);
   }

   if (sr_conn_link.conn != NULL) {
      sr_disconnect(sr_conn_link.conn);
   }
}

void load_config()
{
   int rc;
   sr_node_t *tree = NULL;

   connect_to_sr();

   rc = sr_get_subtree(sr_conn_link.sess, ns_root_sr_path, SR_GET_SUBTREE_DEFAULT, &tree);
   IF_SR_ERR_FAIL(rc)

   if (vector_init(&insts_v, 10) != 0) {
      fail_msg("Failed to allocate memory for modules vector");
   }

   if (vector_init(&mgrps_v, 5) != 0) {
      fail_msg("Failed to allocate memory for module groups vector");
   }

   if (ns_config_load(sr_conn_link.sess) != 0) {
      fail_msg("Failed to load configuration");
   }

   rc = sr_session_switch_ds(sr_conn_link.sess, SR_DS_RUNNING);
   IF_SR_ERR_FAIL(rc)
}


static inline void fake_sv_routine()
{
   sv_thread_running = true;
   stats_callback_cnt = 0;
   while (sv_thread_running) {
      pthread_mutex_lock(&config_lock);
      usleep(15000); // Simulate operations
      pthread_mutex_unlock(&config_lock);
      usleep(15000); // Give time for other threads
   }
   VERBOSE(V3, "Leaving supervisor routine");
}

int inst_get_stats_cb_wrapper(const char *xpath,
                              sr_val_t **values,
                              size_t *values_cnt,
                              void *private_ctx)
{
   int rc;
   rc = inst_get_stats_cb(xpath, values, values_cnt, private_ctx);

   sv_thread_running = false;

   return rc;
}

int get_interface_stats_cb_wrapper(const char *xpath,
                                       sr_val_t **values,
                                       size_t *values_cnt,
                                       void *private_ctx)
{
   int rc;
   rc = interface_get_stats_cb(xpath, values, values_cnt, private_ctx);

   sv_thread_running = false;

   return rc;
}

void * request_stats_async(void * stats_type)
{
   int *rc = (int *) calloc(1, sizeof(int));
   IF_NO_MEM_FAIL(rc)

   char *cmd = (char *) calloc(1, sizeof(char) * (strlen(stats_type) + 16));
   IF_NO_MEM_FAIL(cmd)
   sprintf(cmd, "./async_stats.py %s", (char *)stats_type);

   *rc = WEXITSTATUS(system(cmd));
   if (*rc != 0) {
      sv_thread_running = false;
      return rc;
   }

   return rc;
}

pthread_t request_stats_async_thread(char * stats_type)
{
   pthread_t t;
   if (pthread_create(&t, NULL, request_stats_async, stats_type)) {
      fail_msg("Failed to create thread for asynch_stats");
   }

   return t;
}


void subscribe_xpath_stats(const char *xpath,
                                  int (fn)(const char *, sr_val_t **, size_t *, void *),
                                  sr_subscr_flag_t subscr_flag)
{
   int rc = sr_dp_get_items_subscribe(sr_conn_link.sess,
                                  xpath,
                                  fn,
                                  NULL,
                                  subscr_flag,
                                  &sr_conn_link.subscr);
   IF_SR_ERR_FAIL(rc)
}

void run_async_caller_and_sv_routine(char * async_option)
{
   pthread_t async_thread;
   int *retval;

   retval = (int *) calloc(1, sizeof(int));
   if (retval == NULL) { fail_msg("No memory for int *retval"); }
   async_thread = request_stats_async_thread(async_option);
   fake_sv_routine();


   if (pthread_join(async_thread, (void **) &retval)) {
      fail_msg("Failed to join assync thread");
   }

   if (*retval != 0) {
      fail_msg("async_stats.py returned error code=%d", *retval);
   }
}

///////////////////////////TESTS
///////////////////////////TESTS
///////////////////////////TESTS
///////////////////////////TESTS

void test_get_intable_interface_stats_cb(void **state)
{
   system("helpers/import_conf.sh -s nemea-tests-1-startup-config-stats-1.xml");
   connect_to_sr();
   load_config();
   subscribe_xpath_stats("/nemea-tests-1:nemea-supervisor/module-group/module"
                               "/instance/interface/stats",
                         get_interface_stats_cb_wrapper,
                         SR_SUBSCR_CTX_REUSE);

   {
      // Load dummy out_stats
      instance_t *inst = insts_v.items[0];
      interface_t *ifc = inst->out_ifces.items[0];
      ifc_out_stats_t *out_stats = ifc->stats;
      out_stats->sent_msg_cnt = 11111;
      out_stats->sent_buff_cnt = 11112;
      out_stats->dropped_msg_cnt = 11113;
      out_stats->autoflush_cnt = 11114;

      ifc = inst->in_ifces.items[0];
      ifc_in_stats_t *in_stats = ifc->stats;
      in_stats->recv_msg_cnt = 12333;
      in_stats->recv_buff_cnt = 12334;
   }

   run_async_caller_and_sv_routine("intable_interfaces_stats");
   disconnect_sr();
}

void test_get_intable_module_stats(void **state)
{
   system("helpers/import_conf.sh -s nemea-tests-1-startup-config-stats-1.xml");
   connect_to_sr();
   load_config();
   subscribe_xpath_stats("/nemea-tests-1:nemea-supervisor/module-group/module/instance",
                         inst_get_stats_cb_wrapper,
                         SR_SUBSCR_CTX_REUSE);

   {
      // Load dummy stats
      instance_t *inst = insts_v.items[0];
      inst->running = false;
      time(&inst->restart_time);
      inst->restarts_cnt = 2;
      inst->last_cpu_umode = 9999;
      inst->last_cpu_kmode = 8888;
      inst->mem_vms = 7777;
      inst->mem_rss = 6666;
   }

   run_async_caller_and_sv_routine("intable_module_stats");
   disconnect_sr();
}

void test_get_intable_inst_stats_including_ifc_stats(void **state)
{
   system("helpers/import_conf.sh -s nemea-tests-1-startup-config-stats-1.xml");
   connect_to_sr();
   load_config();
   subscribe_xpath_stats("/nemea-tests-1:nemea-supervisor/module-group/module"
                               "/instance/interface/stats",
                         get_interface_stats_cb_wrapper,
                         SR_SUBSCR_CTX_REUSE);
   subscribe_xpath_stats("/nemea-tests-1:nemea-supervisor/module-group/module/instance",
                         inst_get_stats_cb_wrapper,
                         SR_SUBSCR_CTX_REUSE);
}

int main(void)
{

   const struct CMUnitTest tests[] = {
         cmocka_unit_test(test_get_intable_inst_stats_including_ifc_stats),
/*
         cmocka_unit_test(test_get_intable_interface_stats_cb),
         cmocka_unit_test(test_get_intable_module_stats),
*/
   };

   disconnect_sr();

   return cmocka_run_group_tests(tests, NULL, NULL);
}