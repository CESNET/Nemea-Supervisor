#include <sysrepo.h>
#include <pthread.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

#include "testing_utils.h"
#include "../src/conf.h"
#include "src/inst_control.c"

/**
 * @brief Mocked sr_conn_link_t from sup.c
 * */
typedef struct sr_conn_link_s {
   sr_conn_ctx_t *conn;
   sr_session_ctx_t *sess;
   sr_subscription_ctx_t *subscr_chges;
} sr_conn_link_t;

sr_conn_link_t sr_conn_link;
bool sv_thread_running = true;


///////////////////////////HELPERS
///////////////////////////HELPERS
///////////////////////////HELPERS
///////////////////////////HELPERS

static void connect_to_sr()
{
   int rc;

   // Connect to sysrepo
   output_fd = stdout;
   rc = sr_connect("test_run_changes.c", SR_CONN_DEFAULT, &sr_conn_link.conn);
   IF_SR_ERR_FAIL(rc)

   rc = sr_session_start(sr_conn_link.conn, SR_DS_STARTUP, SR_SESS_CONFIG_ONLY,
                         &sr_conn_link.sess);
   IF_SR_ERR_FAIL(rc)
}
static void disconnect_sr()
{
   if (sr_conn_link.subscr_chges != NULL) {
      sr_unsubscribe(sr_conn_link.sess, sr_conn_link.subscr_chges);
      sr_conn_link.subscr_chges = NULL;
   }

   if (sr_conn_link.sess != NULL) {
      sr_session_stop(sr_conn_link.sess);
   }

   if (sr_conn_link.conn != NULL) {
      sr_disconnect(sr_conn_link.conn);
   }
}

static int load_config()
{
   connect_to_sr();

   if (vector_init(&insts_v, 10) != 0) {
      fail_msg("Failed to allocate memory for instances vector");
   }

   if (vector_init(&avmods_v, 10) != 0) {
      fail_msg("Failed to allocate memory for modules vector");
   }

   return ns_startup_config_load(sr_conn_link.sess);
}

void start_intable_module(inst_t *inst, char *faked_name)
{
   inst->pid = fork();
   if (inst->pid == -1) {
      fail_msg("Fork: could not fork supervisor process");
   }

   if (inst->pid != 0) {
      inst->is_my_child = true;
      inst->running = true;
   } else {
      setsid();
      char *exec_args[2] = {faked_name, NULL};
      execv("./intable_module", exec_args);

      fail_msg("Failed to execute ./intable_module with execv errno=%d", errno);
   }
}

static void disconnect_and_unload_config()
{
   disconnect_sr();
   vector_free(&avmods_v);
   vector_free(&insts_v);
}

///////////////////////////TESTS
///////////////////////////TESTS
///////////////////////////TESTS
///////////////////////////TESTS


void test_av_module_stop_remove_by_name(void **state)
{
   system("../../tests/helpers/import_conf.sh -s nemea-test-1-startup-5.json");
   assert_int_equal(load_config(), 0);
   assert_int_equal(insts_v.total, 4);
   assert_int_equal(avmods_v.total, 2);

/*   VERBOSE(DEBUG, "Starting fake module")
   inst_t *intable_module = insts_v.items[2];
   start_intable_module(intable_module, "intable_module");*/

   inst_t *intable_module = insts_v.items[2];
   intable_module->pid = 123;

   av_module_stop_remove_by_name("module B");

   assert_int_equal(insts_v.total, 3);
   assert_int_equal(avmods_v.total, 1);
   disconnect_and_unload_config();
}

int main(void)
{

   const struct CMUnitTest tests[] = {
         cmocka_unit_test(test_av_module_stop_remove_by_name),
   };

   return cmocka_run_group_tests(tests, NULL, NULL);
}