#include <sysrepo.h>
#include <pthread.h>
#include "testing_utils.h"
#include "../src/module.h"
#include "../src/supervisor.c"

const char *ns_root_sr_path = "/nemea-tests-1:nemea-supervisor";

static void connect_to_sr()
{
   int rc;

   //sr_log_stderr(SR_LL_DBG);
   // Connect to sysrepo
   rc = sr_connect("conf_tests.c", SR_CONN_DEFAULT, &sr_conn_link.conn);
   if (SR_ERR_OK != rc) {
      fail_msg("Failed to connect to sysrepo: %s", sr_strerror(rc));
   }

   rc = sr_session_start(sr_conn_link.conn, SR_DS_STARTUP, SR_SESS_CONFIG_ONLY,
                         &sr_conn_link.sess);
   if (SR_ERR_OK != rc) {
      fail_msg("Failed to create sysrepo session", sr_strerror(rc));
   }
}
static void disconnect_sr()
{
   if (sr_conn_link.sess != NULL) {
      sr_session_stop(sr_conn_link.sess);
   }

   if (sr_conn_link.conn != NULL) {
      sr_disconnect(sr_conn_link.conn);
   }
}

///////////////////////////TESTS
///////////////////////////TESTS
///////////////////////////TESTS
///////////////////////////TESTS


void test_insts_save_running_pids(void **state)
{
   system("../../tests/helpers/import_conf.sh -s nemea-test-1-startup-6.data.json");
   connect_to_sr();

   int rc;
   inst_t *inst = NULL;
   av_module_t *mod = NULL;
   sr_val_t *value = NULL;
   char xpath[] = NS_ROOT_XPATH"/instance[name='inst1']/last-pid";

   { // Fake loaded module
      mod = av_module_alloc();
      IF_NO_MEM_FAIL(mod)
      mod->name = "module A";
   }
   { // Fake loaded instance
      inst = inst_alloc();
      IF_NO_MEM_FAIL(inst)
      inst->name = "inst1";
      inst->running = true;
      inst->mod_ref = mod;
      inst->pid = 12312;
      vector_add(&insts_v, inst);
   }

   { // tests PID is not in sysrepo
      rc = sr_get_item(sr_conn_link.sess, xpath, &value);
      assert_int_equal(rc, SR_ERR_NOT_FOUND);
      sr_free_val(value);
   }

   insts_save_running_pids();

   { // tests PID was successfuly stored in sysrepo
      rc = sr_get_item(sr_conn_link.sess, xpath, &value);
      IF_SR_ERR_FAIL(rc)
      assert_int_equal(inst->pid, value->data.uint32_val);
      sr_free_val(value);
   }

   NULLP_TEST_AND_FREE(inst)
   NULLP_TEST_AND_FREE(mod)
   vector_free(&insts_v);
   disconnect_sr();
}

int main(void)
{

   sr_conn_link.conn = NULL;
   sr_conn_link.sess = NULL;

   const struct CMUnitTest tests[] = {
         cmocka_unit_test(test_insts_save_running_pids),
   };


   return cmocka_run_group_tests(tests, NULL, NULL);
}