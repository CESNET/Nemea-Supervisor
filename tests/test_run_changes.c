#include <sysrepo.h>
#include <pthread.h>

#include "testing_utils.h"
#include "../src/conf.h"
#include "../src/module.h"
#include "../src/utils.h"
#include "../src/run_changes.c"

const char *ns_root_sr_path = "/nemea-tests-1:nemea-supervisor";

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
   rc = sr_connect("conf_test.c", SR_CONN_DEFAULT, &sr_conn_link.conn);
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
   int rc;
   sr_node_t *tree = NULL;

   connect_to_sr();

   rc = sr_get_subtree(sr_conn_link.sess, ns_root_sr_path, SR_GET_SUBTREE_DEFAULT, &tree);
   IF_SR_ERR_FAIL(rc)

   if (vector_init(&insts_v, 10) != 0) {
      fail_msg("Failed to allocate memory for instances vector");
   }

   if (vector_init(&mods_v, 10) != 0) {
      fail_msg("Failed to allocate memory for modules vector");
   }

   if (vector_init(&mgrps_v, 5) != 0) {
      fail_msg("Failed to allocate memory for module groups vector");
   }

   return ns_config_load(sr_conn_link.sess);
}

static void start_intable_module(instance_t *module, char *faked_name)
{
   module->pid = fork();
   if (module->pid == -1) {
      fail_msg("Fork: could not fork supervisor process");
   }

   if (module->pid != 0) {
      module->is_my_child = true;
      module->running = true;
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
   int rc = load_config();
   assert_int_equal(rc, 0);

   VERBOSE(DEBUG, "Subscribing")
   rc = sr_subtree_change_subscribe(sr_conn_link.sess,
                                    ns_root_sr_path,
                                    ns_config_change_cb_wrapper, NULL, 0,
                                    SR_SUBSCR_DEFAULT | SR_SUBSCR_APPLY_ONLY,
                                    &sr_conn_link.subscr_chges);
   IF_SR_ERR_FAIL(rc)
   VERBOSE(DEBUG, "Subscribed")

   rc = sr_session_switch_ds(sr_conn_link.sess, SR_DS_RUNNING);
   IF_SR_ERR_FAIL(rc)
}

static void disconnect_and_unload_config()
{
   disconnect_sr();
   vector_free(&insts_v);
   vector_free(&mods_v);
   vector_free(&mgrps_v);
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

/*
static void config_change_cb_test_group_with_modules_created(void **state)
{
   sr_subscription_ctx_t *sub = NULL;
   int rc = load_config();
   assert_int_equal(rc, 0);

   VERBOSE(DEBUG, "Subscribing")
   rc = sr_module_change_subscribe(sr_conn_link.sess, "nemea-tests-1", config_change_cb_wrapper, NULL, 0, SR_SUBSCR_DEFAULT | SR_SUBSCR_APPLY_ONLY, &sub);
   //rc = sr_subtree_change_subscribe(sr_conn_link.sess, SV_TESTING_ROOT_XPATH, config_change_cb_wrapper, &config_lock, 0, SR_SUBSCR_DEFAULT | SR_SUBSCR_APPLY_ONLY, &sub);
   assert_int_equal(rc, SR_ERR_OK);
   VERBOSE(DEBUG, "Subscribed")

   pid_t pid = fork();
   assert_int_not_equal(pid, -1);

   if (pid == 0) {
      // Child
      char *params[] = {"async_change.py", "create_group_with_modules", NULL};
      execv("./create_group_with_modules.py", params);
      printf("Failed to exec create_group_with_modules.py\n");
      return;
   }

   while (sv_thread_running) {
      pthread_mutex_lock(&config_lock);
      usleep(15000); // Simulate operations
      pthread_mutex_unlock(&config_lock);
      usleep(15000); // Give time for other threads
   }
}
*/

void test_ns_config_change_cb_with_module_created(void **state)
{
   system("helpers/import_conf.sh -s nemea-tests-1-startup-config-3.xml");
   load_config_and_subscribe_to_change();
   assert_int_equal(insts_v.total, 4);
   assert_int_equal(mods_v.total, 4);
   assert_int_equal(mgrps_v.total, 5);

   VERBOSE(DEBUG, "Making async change")
   make_async_change("create_module");
   fake_sv_routine();


   bool new_found = false;
   module_t *m_iter = NULL;
   FOR_EACH_IN_VEC(mods_v, m_iter) {
      if (strcmp(m_iter->name, "m1") == 0) {
         new_found = true;
         assert_string_equal(m_iter->path, "/m1path");
      }
   }
   // 4 already defined in yang/nemea-tests-1-startup-config-1.xml
   assert_int_equal(mods_v.total, 5);
   assert_int_equal(new_found, true);
   disconnect_and_unload_config();
}

void test_ns_config_change_cb_with_module_group_created(void **state)
{
   system("helpers/import_conf.sh -s nemea-tests-1-startup-config-3.xml");
   load_config_and_subscribe_to_change();
   assert_int_equal(insts_v.total, 4);
   assert_int_equal(mods_v.total, 4);
   assert_int_equal(mgrps_v.total, 5);

   VERBOSE(DEBUG, "Making async change")
   make_async_change("create_group_with_modules_and_instances");
   fake_sv_routine();

   module_group_t *gx = NULL;
   FOR_EACH_IN_VEC(mgrps_v, gx) {
      print_module_group(gx);
   }

   module_group_t *new_group = mgrps_v.items[mgrps_v.total - 1];
   assert_string_equal(new_group->name, "g1");
   assert_int_equal(new_group->enabled, 1);

   {
      assert_int_equal(insts_v.total, 5);
      instance_t *inst = insts_v.items[4];
      assert_non_null(inst->module);
      assert_ptr_equal(inst->module, mods_v.items[5]);
      assert_string_equal(inst->module->name, "m2");
      assert_string_equal(inst->module->path, "/m2path");
      assert_string_equal(inst->name, "i1");
      assert_int_equal(inst->enabled, true);
      assert_ptr_equal(inst->module->group, new_group);
   }
   {
      assert_int_equal(mods_v.total, 6);
      module_t *mod = mods_v.items[4];
      assert_string_equal(mod->name, "m1");
      assert_string_equal(mod->path, "/m1path");
      assert_ptr_equal(mod->group, new_group);
   }

   disconnect_and_unload_config();
}

void test_ns_config_change_cb_with_module_group_deleted(void **state)
{
   load_config_and_subscribe_to_change();
   assert_int_equal(insts_v.total, 4);
   assert_int_equal(mods_v.total, 4);
   assert_int_equal(mgrps_v.total, 5);

   VERBOSE(DEBUG, "Starting fake module")
   instance_t * intable_module = insts_v.items[insts_v.total - 1];
   start_intable_module(intable_module, "intable_module");

   VERBOSE(DEBUG, "Making async change")
   make_async_change("delete_group_with_module_and_instance");
   VERBOSE(DEBUG, "Inside supervisor routine")
   fake_sv_routine();

   assert_int_equal(insts_v.total, 3);
   assert_int_equal(mods_v.total, 3);
   assert_int_equal(mgrps_v.total, 4);
   disconnect_and_unload_config();
}

void test_ns_config_change_cb_with_module_deleted(void **state)
{
   load_config_and_subscribe_to_change();

   assert_int_equal(insts_v.total, 4);
   assert_int_equal(mods_v.total, 4);
   assert_int_equal(mgrps_v.total, 5);

   VERBOSE(DEBUG, "Starting fake module")
   instance_t * intable_module = insts_v.items[insts_v.total - 1];
   start_intable_module(intable_module, "intable_module");

   VERBOSE(DEBUG, "Making async change")
   make_async_change("delete_module");
   VERBOSE(DEBUG, "Inside supervisor routine")
   fake_sv_routine();

   assert_int_equal(insts_v.total, 3);
   assert_int_equal(mods_v.total, 3);
   assert_int_equal(mgrps_v.total, 5);
   disconnect_and_unload_config();
}

void test_ns_config_change_cb_with_group_modified_1(void **state)
{
   load_config_and_subscribe_to_change();

   module_group_t * group = mgrps_v.items[mgrps_v.total - 1];
   assert_int_equal(group->enabled, true);

   VERBOSE(DEBUG, "Making async change")
   make_async_change("group_modified_1");
   VERBOSE(DEBUG, "Inside supervisor routine")
   fake_sv_routine();

   group = mgrps_v.items[mgrps_v.total - 1];
   assert_int_equal(group->enabled, false);

   disconnect_and_unload_config();
}

void test_ns_config_change_cb_with_module_modified_1(void **state)
{
   load_config_and_subscribe_to_change();
   module_t *mod = NULL;

   assert_int_equal(mods_v.total, 4);
   mod = mods_v.items[mods_v.total - 1];
   assert_string_equal(mod->path, "/asdasd");

   VERBOSE(DEBUG, "Making async change")
   make_async_change("module_modified_1");
   VERBOSE(DEBUG, "Inside supervisor routine")
   fake_sv_routine();

   assert_int_equal(mods_v.total, 4);
   mod = mods_v.items[mods_v.total - 1];
   assert_string_equal(mod->path, "/a/b/cc");

   disconnect_and_unload_config();
}

void test_ns_config_change_cb_with_instance_modified_1(void **state)
{
   load_config_and_subscribe_to_change();

   instance_t * inst = insts_v.items[insts_v.total - 1];
   assert_null(inst->params);
   assert_int_equal(inst->in_ifces.total, 0);
   start_intable_module(inst, "inst");
   pid_t old_pid = inst->pid;

   VERBOSE(DEBUG, "Making async change")
   make_async_change("instance_modified_1");
   VERBOSE(DEBUG, "Inside supervisor routine")
   fake_sv_routine();

   inst = insts_v.items[insts_v.total - 1];
   assert_string_equal(inst->name, "intable_module1");
   assert_int_equal(inst->in_ifces.total, 1);

   interface_t *ifc = inst->in_ifces.items[0];
   assert_int_equal(ifc->type, NS_IF_TYPE_UNIX);
   // Module should be restarted and therefore have new PID
   assert_int_not_equal(old_pid, inst->pid);

   disconnect_and_unload_config();
}

void test_ns_change_load(void **state)
{
   ns_change_t *change = NULL;
   sr_val_t val= {0};

   {
      val.xpath = strdup("/nemea:nemea-supervisor/module-group[name='grp1']"
                               "/module[name='mod1']/instance[name='inst1']/enabled");
      IF_NO_MEM_FAIL(val.xpath)

      change = ns_change_load(SR_OP_CREATED, NULL, &val);
      assert_non_null(change);
      assert_int_equal(change->type, NS_CHE_T_INST);
      assert_string_equal(change->inst_name, "inst1");
      assert_string_equal(change->grp_name, "grp1");
      assert_string_equal(change->mod_name, "mod1");
      assert_string_equal(change->node_name, "enabled");
      NULLP_TEST_AND_FREE(val.xpath)
   }

   {
      val.xpath = strdup("/nemea:nemea-supervisor/module-group[name='grp1']"
                               "/module[name='mod1']/path");
      IF_NO_MEM_FAIL(val.xpath)

      change = ns_change_load(SR_OP_CREATED, NULL, &val);
      assert_non_null(change);
      assert_int_equal(change->type, NS_CHE_T_MOD);
      assert_null(change->inst_name);
      assert_string_equal(change->grp_name, "grp1");
      assert_string_equal(change->mod_name, "mod1");
      assert_string_equal(change->node_name, "path");
      NULLP_TEST_AND_FREE(val.xpath)
   }


   {
      val.xpath = strdup("/nemea:nemea-supervisor/module-group[name='grp1']");
      IF_NO_MEM_FAIL(val.xpath)

      change = ns_change_load(SR_OP_CREATED, NULL, &val);
      assert_non_null(change);
      assert_int_equal(change->type, NS_CHE_T_GRP);
      assert_null(change->inst_name);
      assert_null(change->mod_name);
      assert_null(change->node_name);
      assert_string_equal(change->grp_name, "grp1");
      NULLP_TEST_AND_FREE(val.xpath)
   }

   {
      val.xpath = strdup("/nemea:nemea-supervisor/module-group[name='grp1']/enabled");
      IF_NO_MEM_FAIL(val.xpath)

      change = ns_change_load(SR_OP_CREATED, NULL, &val);
      assert_non_null(change);
      assert_int_equal(change->type, NS_CHE_T_GRP);
      assert_null(change->inst_name);
      assert_null(change->mod_name);
      assert_string_equal(change->grp_name, "grp1");
      assert_string_equal(change->node_name, "enabled");
      NULLP_TEST_AND_FREE(val.xpath)
   }


}

int main(void)
{

   const struct CMUnitTest tests[] = {
         cmocka_unit_test(test_ns_config_change_cb_with_module_modified_1),
         cmocka_unit_test(test_ns_config_change_cb_with_instance_modified_1),
         cmocka_unit_test(test_ns_config_change_cb_with_group_modified_1),
         cmocka_unit_test(test_ns_config_change_cb_with_module_deleted),
         cmocka_unit_test(test_ns_config_change_cb_with_module_group_deleted),
         cmocka_unit_test(test_ns_config_change_cb_with_module_group_created),
         cmocka_unit_test(test_ns_config_change_cb_with_module_created),
         cmocka_unit_test(test_ns_change_load),
   };

   disconnect_sr();

   return cmocka_run_group_tests(tests, NULL, NULL);
}