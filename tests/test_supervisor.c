#include <sysrepo.h>
#include <pthread.h>

#define TEST 1

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
   system("helpers/import_conf.sh -s nemea-tests-1-startup-config-4.xml");
   connect_to_sr();

   int rc;
   char xpath[4096];
   pid_t fetched_pid;
   instance_t *inst = NULL;
   module_group_t *grp = NULL;
   module_t *mod = NULL;
   sr_val_t *value = NULL;

   { // Fake loaded group
      grp = module_group_alloc();
      IF_NO_MEM_FAIL(grp)
      grp->name = "Detectors";
   }
   { // Fake loaded module
      mod = module_alloc();
      IF_NO_MEM_FAIL(mod)
      mod->name = "mod1";
      mod->group = grp;
   }
   { // Fake loaded instance
      inst = run_module_alloc();
      IF_NO_MEM_FAIL(inst)
      inst->name = "inst1";
      inst->running = true;
      inst->module = mod;
      inst->pid = 12312;
      memset(xpath, 0, 4096);
      sprintf(xpath, "%s/module-group[name='Detectors']/module[name='mod1']"
                    "/instance[name='inst1']/last-pid",
              ns_root_sr_path);
      instance_add(inst);
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
   NULLP_TEST_AND_FREE(grp)
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