#include <pthread.h>
#include <stdio.h>
#include <sysrepo.h>
#include <sysrepo/values.h>

#include "testing_utils.h"
#include "../src/module.h"
#include "../src/conf.c"

/**
 * @brief Mocked sr_conn_link_t from sup.c
 * */
typedef struct sr_conn_link_s {
   sr_conn_ctx_t *conn;
   sr_session_ctx_t *sess;
} sr_conn_link_t;

sr_conn_link_t sr_conn_link = { .conn = NULL, .sess = NULL };

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

static pid_t start_intable_module(char *faked_name)
{

   pid_t pid = fork();
   if (pid == -1) {
      fail_msg("Fork: could not fork supervisor process");
   }

   if (pid != 0) {
      usleep(9000); // Give execv some time to finish
      return pid;
   } else {
      setsid();
      char *exec_args[2] = {faked_name, NULL};
      execv("./intable_module", exec_args);

      fail_msg("Failed to execute ./intable_module with execv errno=%d", errno);
      return -1;
   }
}

static char * get_intable_run_path()
{
   char cwd[2222];
   char *path;
   memset(cwd, 0, 2222);
   getcwd(cwd, 2222);

   path = (char *) calloc(1, sizeof(char) * (strlen(cwd) + 16));
   if (path == NULL) { fail_msg("Failed to allocate intable_module path"); }

   sprintf(path, "%s/intable_module", cwd);

   return path;
}

static void insert_intable_pid_to_sr(pid_t pid)
{
   int rc;
   sr_val_t *val = NULL;
   char xpath[] = NS_ROOT_XPATH"/module[name='intable_module']/last-pid";

   rc = sr_new_val(NULL, &val);
   if (rc != SR_ERR_OK) { fail_msg("SR error:%s", sr_strerror(rc)); }

   val->type = SR_UINT32_T;
   val->data.uint32_val = (uint32_t) pid;
   val->xpath = strdup(xpath);
   IF_NO_MEM_FAIL(val->xpath)

   // Node must not exist
   rc = sr_set_item(sr_conn_link.sess, xpath, val, SR_EDIT_DEFAULT);
   IF_SR_ERR_FAIL(rc)

   rc = sr_commit(sr_conn_link.sess);
   IF_SR_ERR_FAIL(rc)
}

static int get_intable_pid_from_sr(pid_t *pid)
{
   int rc;
   sr_val_t *val = NULL;
   char xpath[] = NS_ROOT_XPATH"/module[name='intable_module']/last-pid";

   rc = sr_get_item(sr_conn_link.sess, xpath, &val);
   if (rc == SR_ERR_OK) {
      *pid = val->data.uint32_val;
   } else {
      *pid = 0;
   }
   sr_free_val(val);

   return rc;
}


void test_interface_specific_params_are_loaded(const interface_t *ifc)
{
   // from nemea-tests-1-startup-config-2.xml
   if (ifc->type == NS_IF_TYPE_TCP && ifc->direction == NS_IF_DIR_OUT) {
      assert_int_equal(ifc->specific_params.tcp->port, 8989);
      assert_int_equal(ifc->specific_params.tcp->max_clients, 2);
   } else if (ifc->type == NS_IF_TYPE_TCP_TLS && ifc->direction == NS_IF_DIR_OUT) {
      assert_int_equal(ifc->specific_params.tcp_tls->port, 3332);
      assert_string_equal(ifc->specific_params.tcp_tls->keyfile, "/a/b");
      assert_string_equal(ifc->specific_params.tcp_tls->certfile, "/a/b/c");
      assert_string_equal(ifc->specific_params.tcp_tls->cafile, "/a/b/c/d");
   } else if (ifc->type == NS_IF_TYPE_UNIX && ifc->direction == NS_IF_DIR_OUT) {
      assert_string_equal(ifc->specific_params.nix->socket_name, "socket-name");
      assert_int_equal(ifc->specific_params.nix->max_clients, 333);
   } else if (ifc->type == NS_IF_TYPE_FILE && ifc->direction == NS_IF_DIR_OUT) {
      assert_string_equal(ifc->specific_params.file->name, "filename");
      assert_string_equal(ifc->specific_params.file->mode, "a");
      assert_int_equal(ifc->specific_params.file->time, 444);
      assert_int_equal(ifc->specific_params.file->size, 555);
   }
}

void test_interface_out_params_are_loaded(const interface_t *ifc)
{
   // from nemea-tests-1-startup-config-2.xml
   switch (ifc->type) {
      case NS_IF_TYPE_TCP:
         assert_string_equal(ifc->buffer, "on");
         assert_string_equal(ifc->autoflush, "off");
         assert_string_equal(ifc->timeout, "HALF_WAIT");
         break;
      case NS_IF_TYPE_TCP_TLS:
         assert_string_equal(ifc->buffer, "off");
         assert_string_equal(ifc->autoflush, "123");
         assert_string_equal(ifc->timeout, "NO_WAIT");
         break;
      case NS_IF_TYPE_UNIX:
         assert_null(ifc->buffer);
         assert_string_equal(ifc->autoflush, "999");
         assert_string_equal(ifc->timeout, "12312");
         break;
      case NS_IF_TYPE_FILE:
         assert_null(ifc->buffer);
         assert_null(ifc->autoflush);
         assert_null(ifc->timeout);
         break;
      default:
         //asda
         break;
   }
}

void test_interface_is_loaded(const interface_t *ifc)
{
   assert_int_equal(ifc->type, NS_IF_TYPE_TCP);
   assert_int_equal(ifc->direction, NS_IF_DIR_OUT);
   assert_string_equal(ifc->name, "tcp-out");
   test_interface_specific_params_are_loaded(ifc);
   test_interface_out_params_are_loaded(ifc);
}

void test_instance_is_loaded(const run_module_t *inst)
{
   assert_string_equal(inst->name, "intable_module");
   assert_int_equal(inst->enabled, true);
   assert_int_equal(inst->max_restarts_minute, 4);
}

void cleanup_structs_and_vectors()
{
   run_module_t *mod = NULL;
   for (uint32_t i = 0; i < rnmods_v.total; i++) {
      mod = rnmods_v.items[i];
      run_module_free(mod);
   }
   vector_free(&rnmods_v);


   av_module_t *avmod = NULL;
   for (uint32_t i = 0; i < avmods_v.total; i++) {
      avmod = avmods_v.items[i];
      av_module_free(avmod);
   }
   vector_free(&avmods_v);
}

///////////////////////////TESTS
///////////////////////////TESTS
///////////////////////////TESTS
///////////////////////////TESTS


void test_interface_file_params_load(void **state)
{
   system("../../tests/helpers/import_conf.sh -s nemea-test-1-startup-2.xml");
   connect_to_sr();

   int rc;
   char *xpath = NULL;

   interface_t *ifc = interface_alloc();
   IF_NO_MEM_FAIL(ifc)
   ifc->type = NS_IF_TYPE_FILE;

   { // tests direction NS_IF_DIR_OUT
      ifc->direction = NS_IF_DIR_OUT;
      assert_int_equal(interface_specific_params_alloc(ifc), 0);
      xpath = NS_ROOT_XPATH"/module[name='intable_module']/interface[name='file-out']";

      rc = interface_file_params_load(sr_conn_link.sess, xpath, ifc);
      assert_int_equal(rc, 0);
      test_interface_specific_params_are_loaded(ifc);
   }


   disconnect_sr();
}

void test_interface_unix_params_load(void **state)
{
   system("../../tests/helpers/import_conf.sh -s nemea-test-1-startup-2.xml");
   connect_to_sr();

   int rc;
   char *xpath = NULL;

   interface_t *ifc = interface_alloc();
   IF_NO_MEM_FAIL(ifc)
   ifc->type = NS_IF_TYPE_UNIX;

   { // tests direction NS_IF_DIR_OUT
      ifc->direction = NS_IF_DIR_OUT;
      assert_int_equal(interface_specific_params_alloc(ifc), 0);
      xpath = NS_ROOT_XPATH"/module[name='intable_module']/interface[name='unix-out']";

      rc = interface_unix_params_load(sr_conn_link.sess, xpath, ifc);
      assert_int_equal(rc, 0);
      test_interface_specific_params_are_loaded(ifc);
   }


   disconnect_sr();
}

void test_interface_tcp_tls_params_load(void **state)
{
   system("../../tests/helpers/import_conf.sh -s nemea-test-1-startup-2.xml");
   connect_to_sr();

   int rc;
   char *xpath = NULL;

   interface_t *ifc = interface_alloc();
   IF_NO_MEM_FAIL(ifc)
   ifc->type = NS_IF_TYPE_TCP_TLS;

   { // tests direction NS_IF_DIR_OUT
      ifc->direction = NS_IF_DIR_OUT;
      assert_int_equal(interface_specific_params_alloc(ifc), 0);
      xpath = NS_ROOT_XPATH"/module[name='intable_module']/interface[name='tcp-tls-out']";
      rc = interface_tcp_tls_params_load(sr_conn_link.sess, xpath, ifc);
      assert_int_equal(rc, 0);
      test_interface_specific_params_are_loaded(ifc);
   }


   disconnect_sr();
}

void test_interface_tcp_params_load(void **state)
{
   system("../../tests/helpers/import_conf.sh -s nemea-test-1-startup-2.xml");
   connect_to_sr();

   int rc;
   char *xpath = NULL;

   interface_t *ifc = interface_alloc();
   IF_NO_MEM_FAIL(ifc)
   ifc->type = NS_IF_TYPE_TCP;

   { // tests direction NS_IF_DIR_OUT
      ifc->direction = NS_IF_DIR_OUT;
      assert_int_equal(interface_specific_params_alloc(ifc), 0);
      xpath = NS_ROOT_XPATH"/module[name='intable_module']/interface[name='tcp-out']";
      rc = interface_tcp_params_load(sr_conn_link.sess, xpath, ifc);
      assert_int_equal(rc, 0);
      test_interface_specific_params_are_loaded(ifc);
   }


   disconnect_sr();
}

void test_instance_pid_restore(void **state)
{
   system("../../tests/helpers/import_conf.sh -s nemea-test-1-startup-2.xml");
   connect_to_sr();

   int rc;
   pid_t fetched_pid;
   run_module_t *inst = NULL;
   av_module_t *mod = NULL;
   pid_t intable_pid;

   { // Fake loaded module
      mod = av_module_alloc();
      IF_NO_MEM_FAIL(mod)
      mod->name = "module A";
      mod->path = get_intable_run_path();
   }
   { // Fake loaded instance
      inst = run_module_alloc();
      IF_NO_MEM_FAIL(inst)
      inst->name = "intable_module";
      inst->mod_kind = mod;
   }

   { // Test that nothing changes when provided hopefully non existing PID
      instance_pid_restore(1999999, inst, sr_conn_link.sess);
      assert_int_equal(inst->pid, 0);
      assert_int_equal(inst->running, false);
   }

   { // Start intable_module, insert it to sysrepo and verify it's there
      intable_pid = start_intable_module("intable_module");
      insert_intable_pid_to_sr(intable_pid);
      // Verify PID was stored inside sysrepo
      rc = get_intable_pid_from_sr(&fetched_pid);
      assert_int_equal(intable_pid, fetched_pid);
      assert_int_equal(rc, SR_ERR_OK);
   }


   { // Restore PID and see that pid & running was set
      instance_pid_restore(intable_pid, inst, sr_conn_link.sess);
      assert_int_equal(inst->pid, intable_pid);
      assert_int_equal(inst->running, true);
      // Verify PID was removed from sysrepo
      rc = get_intable_pid_from_sr(&fetched_pid);
      assert_int_equal(0, fetched_pid);
      assert_int_equal(rc, SR_ERR_NOT_FOUND);
   }

   NULLP_TEST_AND_FREE(mod->path)
   NULLP_TEST_AND_FREE(inst)
   NULLP_TEST_AND_FREE(mod)
   kill(intable_pid, SIGINT);
   disconnect_sr();
}

void test_interface_load(void **state)
{
   system("../../tests/helpers/import_conf.sh -s nemea-test-1-startup-2.xml");
   connect_to_sr();

   char * xpath = NULL;

   run_module_t *inst = run_module_alloc();
   IF_NO_MEM_FAIL(inst)

   {
      xpath = NS_ROOT_XPATH"/module[name='intable_module']/interface[name='tcp-out']";

      assert_int_equal(inst->out_ifces.total, 0);
      assert_int_equal(interface_load(sr_conn_link.sess, xpath, inst), 0);
      assert_int_equal(inst->out_ifces.total, 1);
      test_interface_is_loaded(inst->out_ifces.items[0]);
   }

   disconnect_sr();
}

void test_av_module_load(void **state)
{
   system("../../tests/helpers/import_conf.sh -s nemea-test-1-startup-2.xml");
   connect_to_sr();

   int rc;
   sr_node_t *node = NULL;
   av_module_t *amod1 = NULL;


   sr_val_t *av_mods = NULL;
   size_t av_mods_cnt = 0;
   rc = sr_get_items(sr_conn_link.sess,
                NS_ROOT_XPATH"/available-module",
                &av_mods, &av_mods_cnt);
   assert_int_equal(rc, SR_ERR_OK);

   assert_int_equal(av_mods_cnt, 2);
   assert_int_equal(avmods_v.total, 0);
   for (int i = 0; i < av_mods_cnt; i++) {
      assert_int_equal(av_module_load(sr_conn_link.sess, av_mods[i].xpath), 0);
   }
   sr_free_values(av_mods, av_mods_cnt);

   assert_int_equal(avmods_v.total, 2);

   amod1 = avmods_v.items[0];
   assert_string_equal(amod1->name, "module A");
   assert_string_equal(amod1->path, "/a/a");
   assert_int_equal(amod1->is_nemea, true);
   assert_int_equal(amod1->is_sr_en, false);

   amod1 = avmods_v.items[1];
   assert_string_equal(amod1->name, "module B");
   assert_string_equal(amod1->path, "/a/b");
   assert_int_equal(amod1->is_nemea, false);
   assert_int_equal(amod1->is_sr_en, false);

   sr_free_tree(node);
   av_module_remove_at(0);
   av_module_remove_at(1);
   av_module_free(amod1);

   disconnect_sr();
}

void test_run_module_load(void **state)
{
   system("../../tests/helpers/import_conf.sh -s nemea-test-1-startup-2.xml");
   connect_to_sr();

   int rc;
   char * xpath = NS_ROOT_XPATH"/module[name='intable_module']";
   run_module_t *mod = NULL;

   av_module_t *avmod = av_module_alloc();
   IF_NO_MEM_FAIL(avmod)
   avmod->name = strdup("module A");
   IF_NO_MEM_FAIL(avmod->name)
   rc = vector_add(&avmods_v, avmod);
   assert_int_equal(rc, 0);

   assert_int_equal(rnmods_v.total, 0);
   rc = run_module_load(sr_conn_link.sess, xpath);
   assert_int_equal(rc, SR_ERR_OK);
   assert_int_equal(rnmods_v.total, 1);

   mod = rnmods_v.items[0];
   assert_string_equal(mod->name, "intable_module");
   // TODO more

   assert_int_equal(rnmods_v.total, 1);
   run_module_remove_at(0);
   assert_int_equal(rnmods_v.total, 0);
   run_module_free(mod);

   cleanup_structs_and_vectors();
   disconnect_sr();

}

void test_run_module_load_by_name(void **state)
{
   system("../../tests/helpers/import_conf.sh -s nemea-test-1-startup-2.xml");
   connect_to_sr();

   int rc;
   run_module_t *inst = NULL;
   av_module_t *avmod = av_module_alloc();
   IF_NO_MEM_FAIL(avmod)
   avmod->name = strdup("module A");
   IF_NO_MEM_FAIL(avmod->name)
   assert_int_equal(vector_add(&avmods_v, avmod), 0);

   {
      assert_int_equal(rnmods_v.total, 0);
      rc = run_module_load_by_name(sr_conn_link.sess, "intable_module");
      assert_int_equal(rc, 0);
      assert_int_equal(rnmods_v.total, 1);
      inst = rnmods_v.items[0];
      test_instance_is_loaded(inst);
      assert_int_equal(inst->out_ifces.total, 5);
      test_interface_is_loaded(inst->out_ifces.items[0]);
   }

   cleanup_structs_and_vectors();
   disconnect_sr();
}

void test_av_module_load_by_name(void **state)
{
   system("../../tests/helpers/import_conf.sh -s nemea-test-1-startup-3.xml");
   connect_to_sr();

   int rc;

   {
      output_fd = stdout;
      assert_int_equal(avmods_v.total, 0);
      assert_int_equal(rnmods_v.total, 0);
      rc = av_module_load_by_name(sr_conn_link.sess, "module A");
      assert_int_equal(rc, 0);
      assert_int_equal(avmods_v.total, 1);
      assert_int_equal(rnmods_v.total, 4);
/*      mod = mods_v.items[0];
      assert_string_equal(mod->name, "intable_module3");
      assert_string_equal(mod->path, "/ab");*/
   }

   {

   }

   cleanup_structs_and_vectors();
   disconnect_sr();
}

void test_ns_startup_config_load(void **state)
{

   system("../../tests/helpers/import_conf.sh -s nemea-test-1-startup-2.xml");
   connect_to_sr();

   int rc;

   rc = ns_startup_config_load(sr_conn_link.sess);
   IF_SR_ERR_FAIL(rc)
   assert_int_equal(avmods_v.total, 2);
   assert_int_equal(rnmods_v.total, 1);

   cleanup_structs_and_vectors();
}

void test_module_name_from_xpath(void **state)
{
   char *xpath;
   char *module_name;

   {
      xpath = strdup("/nemea:supervisor/module[name='yy'][module-kind='xx']/enabled");
      IF_NO_MEM_FAIL(xpath)
      module_name = module_name_from_xpath(xpath);
      assert_string_equal("yy", module_name);
      NULLP_TEST_AND_FREE(xpath)
   }

   {
      xpath = strdup("/nemea:supervisor/asdfasdf");
      IF_NO_MEM_FAIL(xpath)
      module_name = module_name_from_xpath(xpath);
      assert_null(module_name);
      NULLP_TEST_AND_FREE(xpath)
   }

   {
      xpath = strdup("/nemea:supervisor/module[name='yy']/enabled)");
      IF_NO_MEM_FAIL(xpath)
      module_name = module_name_from_xpath(xpath);
      assert_string_equal("yy", module_name);
      NULLP_TEST_AND_FREE(xpath)
   }
}

int main(void)
{
   const struct CMUnitTest tests[] = {
         cmocka_unit_test(test_interface_file_params_load),
         cmocka_unit_test(test_interface_unix_params_load),
         cmocka_unit_test(test_interface_tcp_tls_params_load),
         cmocka_unit_test(test_interface_tcp_params_load),
         cmocka_unit_test(test_interface_load),
         cmocka_unit_test(test_av_module_load),
         cmocka_unit_test(test_instance_pid_restore),
         cmocka_unit_test(test_run_module_load),
         cmocka_unit_test(test_ns_startup_config_load),
         cmocka_unit_test(test_run_module_load_by_name),
         cmocka_unit_test(test_module_name_from_xpath),
         cmocka_unit_test(test_av_module_load_by_name),

   };


   return cmocka_run_group_tests(tests, NULL, NULL);
}