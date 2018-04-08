#include <sysrepo.h>
#include <pthread.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

#include "testing_utils.h"
#include "../src/run_changes.c"

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

void cleanup_structs_and_vectors()
{
   inst_t *inst = NULL;
   for (uint32_t i = 0; i < insts_v.total; i++) {
      inst = insts_v.items[i];
      inst_free(inst);
   }
   vector_free(&insts_v);


   av_module_t *avmod = NULL;
   for (uint32_t i = 0; i < avmods_v.total; i++) {
      avmod = avmods_v.items[i];
      av_module_free(avmod);
   }
   vector_free(&avmods_v);
}


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

int ns_config_change_cb_wrapper(sr_session_ctx_t *sess,
                                const char *sr_module_name,
                                sr_notif_event_t evnt, void *priv_ctx)
{
   int rc = run_config_change_cb(sess, sr_module_name, evnt, priv_ctx);
   sv_thread_running = false;

   return rc;
}

static void load_config_and_subscribe_to_change()
{
   int rc;
   assert_int_equal(load_config(), 0);

   VERBOSE(V3, "Subscribing to %s", NS_ROOT_XPATH)
   rc = sr_subtree_change_subscribe(sr_conn_link.sess,
                                    NS_ROOT_XPATH,
                                    ns_config_change_cb_wrapper, NULL, 0,
                                    SR_SUBSCR_DEFAULT | SR_SUBSCR_APPLY_ONLY,
                                    &sr_conn_link.subscr_chges);
   IF_SR_ERR_FAIL(rc)
   VERBOSE(V3, "Subscribed")

   rc = sr_session_switch_ds(sr_conn_link.sess, SR_DS_RUNNING);
   IF_SR_ERR_FAIL(rc)
}

static void disconnect_and_unload_config()
{
   disconnect_sr();
   cleanup_structs_and_vectors();
}

static void make_async_change(char * change)
{
   pid_t pid = fork();
   assert_int_not_equal(pid, -1);

   if (pid == 0) {
      // Child
      char *params[] = {"async_change.py", change, NULL};
      execv("./async_change.py", params);
      fail_msg("Failed to execv async_change.py");
   }
}

static inline void fake_sv_routine()
{
   sv_thread_running = true;
   while (sv_thread_running) {
      pthread_mutex_lock(&config_lock);
      usleep(15000); // Simulate operations
      pthread_mutex_unlock(&config_lock);
      usleep(15000); // Give time for other threads
   }
   VERBOSE(V3, "Leaving supervisor routine")
}

///////////////////////////TESTS
///////////////////////////TESTS
///////////////////////////TESTS
///////////////////////////TESTS

void test_ns_config_change_cb_with_module_created(void **state)
{
   system("helpers/import_conf.sh -s nemea-test-1-startup-4.data.json");
   load_config_and_subscribe_to_change();
   assert_int_equal(insts_v.total, 4);
   assert_int_equal(avmods_v.total, 2);

   make_async_change("create_available_module");
   fake_sv_routine();


   bool new_found = false;
   av_module_t *m_iter = NULL;

   for (uint32_t i = 0; i < avmods_v.total; i++) {
      m_iter = avmods_v.items[i];

      if (strcmp(m_iter->name, "m1") == 0) {
         new_found = true;
         assert_string_equal(m_iter->path, "/m1path");
      }
   }

   assert_int_equal(avmods_v.total, 3);
   assert_int_equal(new_found, true);
   disconnect_and_unload_config();
}

void test_ns_config_change_cb_with_module_deleted(void **state)
{
   system("helpers/import_conf.sh -s nemea-test-1-startup-4.data.json");
   load_config_and_subscribe_to_change();
   assert_int_equal(insts_v.total, 4);
   assert_int_equal(avmods_v.total, 2);

   VERBOSE(V3, "Starting fake module")
   inst_t *intable_module = insts_v.items[2];
   start_intable_module(intable_module, "intable_module");

   VERBOSE(V3, "Making async change")
   make_async_change("delete_available_module");
   VERBOSE(V3, "Inside supervisor routine")
   fake_sv_routine();

   assert_int_equal(insts_v.total, 3);
   assert_int_equal(avmods_v.total, 1);
   disconnect_and_unload_config();
}

void test_ns_config_change_cb_with_module_modified_1(void **state)
{
   system("helpers/import_conf.sh -s nemea-test-1-startup-4.data.json");
   load_config_and_subscribe_to_change();
   assert_int_equal(insts_v.total, 4);
   assert_int_equal(avmods_v.total, 2);
   av_module_t *mod = NULL;
   inst_t *inst = NULL;

   mod = avmods_v.items[1];
   assert_string_equal(mod->path, "/a/b");
   { // fake pid in instance m3
      inst = insts_v.items[2];
      assert_string_equal(inst->name, "m3");
      inst->pid = 123;
   }

   VERBOSE(V3, "Making async change")
   make_async_change("available_module_modified_1");
   VERBOSE(V3, "Inside supervisor routine")
   fake_sv_routine();

   assert_int_equal(avmods_v.total, 2);
   mod = avmods_v.items[1];
   assert_string_equal(mod->path, "/a/b/cc");
   assert_int_equal(insts_v.total, 4);
   /* since module instance was reloaded, it should have default PID=0
    *  and be last in the vector */
   inst = insts_v.items[3];

   assert_string_equal(inst->name, "m3");
   assert_int_equal(inst->pid, 0);

   disconnect_and_unload_config();
}

void test_ns_config_change_cb_with_inst_modified_1(void **state)
{
   system("helpers/import_conf.sh -s nemea-test-1-startup-4.data.json");
   load_config_and_subscribe_to_change();

   inst_t * inst = insts_v.items[insts_v.total - 1];
   assert_null(inst->params);
   assert_int_equal(inst->in_ifces.total, 0);
   assert_int_equal(inst->out_ifces.total, 1);
   start_intable_module(inst, "inst");
   pid_t old_pid = inst->pid;

   VERBOSE(V3, "Making async change")
   make_async_change("instance_modified_1");
   VERBOSE(V3, "Inside supervisor routine")
   fake_sv_routine();

   inst = insts_v.items[insts_v.total - 1];
   assert_string_equal(inst->name, "m4");
   assert_int_equal(inst->out_ifces.total, 1);
   assert_int_equal(inst->in_ifces.total, 1);

   interface_t *ifc = inst->in_ifces.items[0];
   assert_int_equal(ifc->type, NS_IF_TYPE_UNIX);
   assert_string_equal(ifc->name, "tcp-in-4-1");
   // Module should be stopped by the configuration and therefore have default PID 0
   assert_int_not_equal(old_pid, inst->pid);

   disconnect_and_unload_config();
}

void test_ns_change_load(void **state)
{
   run_change_t *change = NULL;
   sr_val_t val= {0};

   {
      val.xpath = strdup(NS_ROOT_XPATH"/available-module[name='Module A']"
                               "/path");
      IF_NO_MEM_FAIL(val.xpath)

      change = run_change_load(SR_OP_CREATED, NULL, &val);
      assert_non_null(change);
      assert_int_equal(change->type, RUN_CHE_T_MOD);
      assert_null(change->inst_name);
      assert_string_equal(change->mod_name, "Module A");
      assert_string_equal(change->node_name, "path");
      NULLP_TEST_AND_FREE(val.xpath)
      run_change_free(&change);
   }

   {
      val.xpath = strdup(NS_ROOT_XPATH"/instance[name='test module 1']"
                               "/interface[name='tcp-out']/type");
      IF_NO_MEM_FAIL(val.xpath)

      change = run_change_load(SR_OP_CREATED, NULL, &val);
      assert_non_null(change);
      assert_int_equal(change->type, RUN_CHE_T_INST);
      assert_null(change->mod_name);
      assert_string_equal(change->inst_name, "test module 1");
      assert_string_equal(change->node_name, "interface");
      NULLP_TEST_AND_FREE(val.xpath)
      run_change_free(&change);
   }
}

int main(void)
{

   //verbosity_level = V3;
   const struct CMUnitTest tests[] = {
         cmocka_unit_test(test_ns_config_change_cb_with_module_modified_1),
         cmocka_unit_test(test_ns_config_change_cb_with_module_created),
         cmocka_unit_test(test_ns_change_load),
         cmocka_unit_test(test_ns_config_change_cb_with_module_deleted),
         cmocka_unit_test(test_ns_config_change_cb_with_inst_modified_1),
   };

   disconnect_sr();

   return cmocka_run_group_tests(tests, NULL, NULL);
}