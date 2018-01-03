#include <pthread.h>
#include <stdio.h>
#include <sysrepo.h>
#include <sysrepo/values.h>

#include "testing_utils.h"
#include "../src/module.h"
#include "../src/conf.c"

const char *ns_root_sr_path = "/nemea-tests-1:nemea-supervisor";

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
   char xpath[4096];

   memset(xpath, 0, 4096);
   sprintf(xpath,
           "%s/module-group[name='Test1']"
                 "/module[name='intable_module']"
                 "/instance[name='inst 1']/last-pid",
           ns_root_sr_path);

   rc = sr_new_val(NULL, &val);
   if (rc != SR_ERR_OK) { fail_msg("SR error:%s", sr_strerror(rc)); }

   val->type = SR_UINT32_T;
   val->data.uint32_val = (uint32_t) pid;
   val->xpath = xpath;
   IF_NO_MEM_FAIL(val->xpath)

   // Node must not exist
   rc = sr_set_item(sr_conn_link.sess, xpath, val, SR_EDIT_DEFAULT);
   if (rc != SR_ERR_OK) {
      fail_msg("Failed to set intable_module pid inside sysrepo: %s",
                sr_strerror(rc));
   }

   rc = sr_commit(sr_conn_link.sess);
   if (rc != SR_ERR_OK) {
      fail_msg("Failed to commit intable_module pid inside sysrepo: %s",
               sr_strerror(rc));
   }
}

static int get_intable_pid_from_sr(pid_t *pid)
{
   int rc;
   char xpath[4096];
   sr_val_t *val = NULL;

/*
   rc = sr_session_start(sr_conn_link.conn, SR_DS_STARTUP, SR_SESS_CONFIG_ONLY,
                         &sr_conn_link.sess);
   if (SR_ERR_OK != rc) {
      fail_msg("Failed to create sysrepo session", sr_strerror(rc));
   }
*/

   memset(xpath, 0, 4096);
   sprintf(xpath,
           "%s/module-group[name='Test1']"
                 "/module[name='intable_module']"
                 "/instance[name='inst 1']/last-pid",
           ns_root_sr_path);

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

void test_instance_is_loaded(const instance_t *inst)
{
   assert_string_equal(inst->name, "inst 1");
   assert_int_equal(inst->enabled, true);
   assert_int_equal(inst->max_restarts_minute, 4);
}

void cleanup_structs_and_vectors()
{
   instance_t *inst = NULL;
   for (uint32_t i = 0; i < insts_v.total; i++) {
      inst = insts_v.items[i];
      instance_free(inst);
   }
   vector_free(&insts_v);

   module_t *mod = NULL;
   for (uint32_t i = 0; i < mods_v.total; i++) {
      mod = mods_v.items[i];
      module_free(mod);
   }
   vector_free(&mods_v);

   module_group_t *grp = NULL;
   for (uint32_t i = 0; i < mgrps_v.total; i++) {
      grp = mgrps_v.items[i];
      module_group_free(grp);
   }
   vector_free(&mgrps_v);

}

///////////////////////////TESTS
///////////////////////////TESTS
///////////////////////////TESTS
///////////////////////////TESTS


void test_interface_file_params_load(void **state)
{
   system("helpers/import_conf.sh -s nemea-tests-1-startup-config-2.xml");
   connect_to_sr();

   int rc;
   char *xpath = NULL;
   sr_node_t *node = NULL;

   interface_t *ifc = interface_alloc();
   IF_NO_MEM_FAIL(ifc)
   ifc->type = NS_IF_TYPE_FILE;

   { // tests direction NS_IF_DIR_OUT
      ifc->direction = NS_IF_DIR_OUT;
      assert_int_equal(interface_specific_params_alloc(ifc), 0);
      xpath = calloc(1, sizeof(char) * (strlen(ns_root_sr_path) + 132));
      IF_NO_MEM_FAIL(xpath)
      sprintf(xpath, "%s/module-group[name='Test1']/module[name='intable_module']"
                    "/instance[name='inst 1']/interface[name='file-out']"
                    "/file-params",
              ns_root_sr_path);
      rc = sr_get_subtree(sr_conn_link.sess, xpath, SR_GET_SUBTREE_DEFAULT, &node);
      IF_SR_ERR_FAIL(rc)

      assert_non_null(node->first_child);
      rc = interface_file_params_load(node->first_child, ifc);
      assert_int_equal(rc, 0);
      test_interface_specific_params_are_loaded(ifc);
   }


   disconnect_sr();
}

void test_interface_unix_params_load(void **state)
{
   system("helpers/import_conf.sh -s nemea-tests-1-startup-config-2.xml");
   connect_to_sr();

   int rc;
   char *xpath = NULL;
   sr_node_t *node = NULL;

   interface_t *ifc = interface_alloc();
   IF_NO_MEM_FAIL(ifc)
   ifc->type = NS_IF_TYPE_UNIX;

   { // tests direction NS_IF_DIR_OUT
      ifc->direction = NS_IF_DIR_OUT;
      assert_int_equal(interface_specific_params_alloc(ifc), 0);
      xpath = calloc(1, sizeof(char) * (strlen(ns_root_sr_path) + 132));
      IF_NO_MEM_FAIL(xpath)
      sprintf(xpath, "%s/module-group[name='Test1']/module[name='intable_module']"
                    "/instance[name='inst 1']/interface[name='unix-out']"
                    "/unix-params",
              ns_root_sr_path);
      rc = sr_get_subtree(sr_conn_link.sess, xpath, SR_GET_SUBTREE_DEFAULT, &node);
      IF_SR_ERR_FAIL(rc)

      assert_non_null(node->first_child);
      rc = interface_unix_params_load(node->first_child, ifc);
      assert_int_equal(rc, 0);
      test_interface_specific_params_are_loaded(ifc);
   }


   disconnect_sr();
}

void test_interface_tcp_tls_params_load(void **state)
{
   system("helpers/import_conf.sh -s nemea-tests-1-startup-config-2.xml");
   connect_to_sr();

   int rc;
   char *xpath = NULL;
   sr_node_t *node = NULL;

   interface_t *ifc = interface_alloc();
   IF_NO_MEM_FAIL(ifc)
   ifc->type = NS_IF_TYPE_TCP_TLS;

   { // tests direction NS_IF_DIR_OUT
      ifc->direction = NS_IF_DIR_OUT;
      assert_int_equal(interface_specific_params_alloc(ifc), 0);
      xpath = calloc(1, sizeof(char) * (strlen(ns_root_sr_path) + 138));
      IF_NO_MEM_FAIL(xpath)
      sprintf(xpath, "%s/module-group[name='Test1']/module[name='intable_module']"
                    "/instance[name='inst 1']/interface[name='tcp-tls-out']"
                    "/tcp-tls-params",
              ns_root_sr_path);
      rc = sr_get_subtree(sr_conn_link.sess, xpath, SR_GET_SUBTREE_DEFAULT, &node);
      IF_SR_ERR_FAIL(rc)

      assert_non_null(node->first_child);
      rc = interface_tcp_tls_params_load(node->first_child, ifc);
      assert_int_equal(rc, 0);
      test_interface_specific_params_are_loaded(ifc);
   }


   disconnect_sr();
}

void test_interface_tcp_params_load(void **state)
{
   system("helpers/import_conf.sh -s nemea-tests-1-startup-config-2.xml");
   connect_to_sr();

   int rc;
   char *xpath = NULL;
   sr_node_t *node = NULL;

   interface_t *ifc = interface_alloc();
   IF_NO_MEM_FAIL(ifc)
   ifc->type = NS_IF_TYPE_TCP;

   { // tests direction NS_IF_DIR_OUT
      ifc->direction = NS_IF_DIR_OUT;
      assert_int_equal(interface_specific_params_alloc(ifc), 0);
      xpath = calloc(1, sizeof(char) * (strlen(ns_root_sr_path) + 130));
      IF_NO_MEM_FAIL(xpath)
      sprintf(xpath, "%s/module-group[name='Test1']/module[name='intable_module']"
                    "/instance[name='inst 1']/interface[name='tcp-out']"
                    "/tcp-params",
              ns_root_sr_path);
      rc = sr_get_subtree(sr_conn_link.sess, xpath, SR_GET_SUBTREE_DEFAULT, &node);
      IF_SR_ERR_FAIL(rc)

      assert_non_null(node->first_child);
      rc = interface_tcp_params_load(node->first_child, ifc);
      assert_int_equal(rc, 0);
      test_interface_specific_params_are_loaded(ifc);
   }


   disconnect_sr();
}

void test_instance_pid_restore(void **state)
{
   system("helpers/import_conf.sh -s nemea-tests-1-startup-config-2.xml");
   connect_to_sr();

   int rc;
   pid_t fetched_pid;
   instance_t *inst = NULL;
   module_group_t *grp = NULL;
   module_t *mod = NULL;
   pid_t intable_pid;

   { // Fake loaded group
      grp = module_group_alloc();
      IF_NO_MEM_FAIL(grp)
      grp->name = "Test1";
   }
   { // Fake loaded module
      mod = module_alloc();
      IF_NO_MEM_FAIL(mod)
      mod->name = "intable_module";
      mod->group = grp;
      mod->path = get_intable_run_path();
   }
   { // Fake loaded instance
      inst = instance_alloc();
      IF_NO_MEM_FAIL(inst)
      inst->name = "inst 1";
      inst->module = mod;
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
   NULLP_TEST_AND_FREE(grp)
   kill(intable_pid, SIGINT);
   disconnect_sr();
}

void test_interface_load(void **state)
{
   system("helpers/import_conf.sh -s nemea-tests-1-startup-config-2.xml");
   connect_to_sr();

   int rc;
   char * xpath = NULL;
   sr_node_t *node = NULL;

   instance_t *inst = instance_alloc();
   IF_NO_MEM_FAIL(inst)

   {
      xpath = calloc(1, sizeof(char) * (strlen(ns_root_sr_path) + 108));
      IF_NO_MEM_FAIL(xpath)
      sprintf(xpath, "%s/module-group[name='Test1']/module[name='intable_module']"
                    "/instance[name='inst 1']/interface[name='tcp-out']",
              ns_root_sr_path);
      rc = sr_get_subtree(sr_conn_link.sess, xpath, SR_GET_SUBTREE_DEFAULT, &node);
      IF_SR_ERR_FAIL(rc)
      assert_non_null(node->first_child);

      assert_int_equal(inst->out_ifces.total, 0);
      assert_int_equal(interface_load(node->first_child, inst), 0);
      assert_int_equal(inst->out_ifces.total, 1);
      test_interface_is_loaded(inst->out_ifces.items[0]);
   }

   disconnect_sr();
}

void test_instance_load(void **state)
{
   system("helpers/import_conf.sh -s nemea-tests-1-startup-config-2.xml");
   connect_to_sr();

   int rc;
   char * xpath = NULL;
   sr_node_t *node = NULL;
   instance_t *inst = NULL;
   module_group_t *grp = module_group_alloc();
   IF_NO_MEM_FAIL(grp)
   module_t *mod = module_alloc();
   IF_NO_MEM_FAIL(mod)
   mod->group = grp;
   mod->path = strdup("/tests-path");
   IF_NO_MEM_FAIL(mod->path)

   {
      xpath = calloc(1, sizeof(char) * (strlen(ns_root_sr_path) + 82));
      IF_NO_MEM_FAIL(xpath)
      sprintf(xpath, "%s/module-group[name='Test1']/module[name='intable_module2']"
                    "/instance[name='inst 1']",
              ns_root_sr_path);
      rc = sr_get_subtree(sr_conn_link.sess, xpath, SR_GET_SUBTREE_DEFAULT, &node);
      IF_SR_ERR_FAIL(rc)
      assert_non_null(node->first_child);

      assert_int_equal(insts_v.total, 0);
      rc = instance_load(sr_conn_link.sess, NULL, mod, node->first_child);
      assert_int_equal(rc, 0);
      assert_int_equal(insts_v.total, 1);
      inst = insts_v.items[0];
      test_instance_is_loaded(inst);
      assert_int_equal(inst->out_ifces.total, 5);
      test_interface_is_loaded(inst->out_ifces.items[0]);
   }

   module_free(mod);
   module_group_free(grp);
   NULLP_TEST_AND_FREE(xpath)
   cleanup_structs_and_vectors();
   disconnect_sr();
}

void test_module_load(void **state)
{
   system("helpers/import_conf.sh -s nemea-tests-1-startup-config-2.xml");
   connect_to_sr();

   int rc;
   char * xpath = NULL;
   sr_node_t *node = NULL;
   module_t *mod = NULL;
   module_group_t *grp = module_group_alloc();
   IF_NO_MEM_FAIL(grp)

   { // generate tree XPATH
      xpath = calloc(1, sizeof(char) * (strlen(ns_root_sr_path) + 59));
      IF_NO_MEM_FAIL(xpath)
      sprintf(xpath, "%s/module-group[name='Test1']/module[name='intable_module3']",
              ns_root_sr_path);
   }

   { // tests with passed module = NULL
      rc = sr_get_subtree(sr_conn_link.sess, xpath, SR_GET_SUBTREE_DEFAULT, &node);
      IF_SR_ERR_FAIL(rc)
      assert_non_null(node->first_child);

      assert_int_equal(mods_v.total, 0);
      rc = module_load(sr_conn_link.sess, NULL, grp, node->first_child);
      assert_int_equal(rc, 0);
      assert_int_equal(mods_v.total, 1);

      mod = mods_v.items[0];
      assert_ptr_equal(mod->group, grp);
      assert_string_equal(mod->name, "intable_module3");
      assert_string_equal(mod->path, "/ab");


      sr_free_tree(node);
      module_remove_at(0);
      module_free(mod);
   }

   { // tests with allocated module struct passed
      mod = (module_t *) calloc(1, sizeof(module_t));
      IF_NO_MEM_FAIL(mod)

      rc = sr_get_subtree(sr_conn_link.sess, xpath, SR_GET_SUBTREE_DEFAULT, &node);
      IF_SR_ERR_FAIL(rc)
      assert_non_null(node->first_child);

      assert_int_equal(module_add(mod), 0);
      assert_int_equal(mods_v.total, 1);
      rc = module_load(sr_conn_link.sess, mod, grp, node->first_child);
      assert_int_equal(rc, 0);
      assert_int_equal(mods_v.total, 1);

      assert_ptr_equal(mod, mods_v.items[0]);
      assert_string_equal(mod->name, "intable_module3");
      assert_string_equal(mod->path, "/ab");

      sr_free_tree(node);
      module_remove_at(0);
      module_free(mod);
   }

   module_group_free(grp);
   NULLP_TEST_AND_FREE(xpath)
   cleanup_structs_and_vectors();
   disconnect_sr();

}

void test_module_group_load(void **state)
{
   system("helpers/import_conf.sh -s nemea-tests-1-startup-config-2.xml");
   connect_to_sr();

   int rc;
   char * xpath = NULL;
   sr_node_t *node = NULL;
   module_group_t *grp = NULL;

   { // generate tree XPATH
      xpath = calloc(1, sizeof(char) * (strlen(ns_root_sr_path) + 28));
      IF_NO_MEM_FAIL(xpath)
      sprintf(xpath, "%s/module-group[name='Test 2']", ns_root_sr_path);
   }

   { // tests with passed group = NULL
      rc = sr_get_subtree(sr_conn_link.sess, xpath, SR_GET_SUBTREE_DEFAULT, &node);
      IF_SR_ERR_FAIL(rc)
      assert_non_null(node->first_child);

      assert_int_equal(mgrps_v.total, 0);
      rc = module_group_load(sr_conn_link.sess, NULL, node->first_child);
      assert_int_equal(rc, 0);
      assert_int_equal(mgrps_v.total, 1);

      grp = mgrps_v.items[0];
      assert_string_equal(grp->name, "Test 2");
      assert_int_equal(grp->enabled, 1);


      sr_free_tree(node);
      module_group_remove_at(0);
      module_group_free(grp);
   }

   { // tests with allocated module struct passed
      grp = (module_group_t *) calloc(1, sizeof(module_group_t));
      IF_NO_MEM_FAIL(grp)

      rc = sr_get_subtree(sr_conn_link.sess, xpath, SR_GET_SUBTREE_DEFAULT, &node);
      IF_SR_ERR_FAIL(rc)
      assert_non_null(node->first_child);

      assert_int_equal(module_group_add(grp), 0);
      assert_int_equal(mgrps_v.total, 1);
      rc = module_group_load(sr_conn_link.sess, grp, node->first_child);
      assert_int_equal(rc, 0);
      assert_int_equal(mgrps_v.total, 1);

      assert_ptr_equal(grp, mgrps_v.items[0]);
      assert_string_equal(grp->name, "Test 2");
      assert_int_equal(grp->enabled, 1);

      sr_free_tree(node);
      module_group_remove_at(0);
      module_group_free(grp);
   }

   NULLP_TEST_AND_FREE(xpath)
   disconnect_sr();

}

void test_instance_load_by_name(void **state)
{
   system("helpers/import_conf.sh -s nemea-tests-1-startup-config-2.xml");
   connect_to_sr();

   VERBOSE(DEBUG, "-----------------------")
   VERBOSE(DEBUG, "insts_v %d %d", insts_v.total, insts_v.capacity)
   VERBOSE(DEBUG, "mods_v %d %d", mods_v.total, mods_v.capacity)
   VERBOSE(DEBUG, "grps_v %d %d", mgrps_v.total, mgrps_v.capacity)
   int rc;
   instance_t *inst = NULL;
   module_t *module = module_alloc();
   IF_NO_MEM_FAIL(module)
   module->name = strdup("intable_module2");
   IF_NO_MEM_FAIL(module->name)
   assert_int_equal(module_add(module), 0);

   {
      assert_int_equal(insts_v.total, 0);
      rc = instance_load_by_name(sr_conn_link.sess, "Test1", "intable_module2",
                                 "inst 1");
      assert_int_equal(rc, 0);
      assert_int_equal(insts_v.total, 1);
      inst = insts_v.items[0];
      test_instance_is_loaded(inst);
      assert_int_equal(inst->out_ifces.total, 5);
      test_interface_is_loaded(inst->out_ifces.items[0]);
   }

   cleanup_structs_and_vectors();
   disconnect_sr();
}

void test_module_load_by_name(void **state)
{
   system("helpers/import_conf.sh -s nemea-tests-1-startup-config-2.xml");
   connect_to_sr();

   int rc;
   module_t *mod = NULL;
   module_group_t *group = module_group_alloc();
   IF_NO_MEM_FAIL(group)
   group->name = strdup("Test1");
   IF_NO_MEM_FAIL(group->name)
   assert_int_equal(module_group_add(group), 0);

   {
      assert_int_equal(mods_v.total, 0);
      rc = module_load_by_name(sr_conn_link.sess, "Test1", "intable_module3");
      assert_int_equal(rc, 0);
      assert_int_equal(mods_v.total, 1);
      mod = mods_v.items[0];
      assert_ptr_equal(mod->group, group);
      assert_string_equal(mod->name, "intable_module3");
      assert_string_equal(mod->path, "/ab");
   }

   cleanup_structs_and_vectors();
   disconnect_sr();
}

void test_module_group_load_by_name(void **state)
{

   system("helpers/import_conf.sh -s nemea-tests-1-startup-config-2.xml");
   connect_to_sr();

   int rc;
   module_group_t *group = NULL;

   {
      assert_int_equal(mgrps_v.total, 0);
      rc = module_group_load_by_name(sr_conn_link.sess, "Test1");
      assert_int_equal(rc, 0);
      assert_int_equal(mgrps_v.total, 1);
      group = mgrps_v.items[0];
      assert_string_equal(group->name, "Test1");
      assert_int_equal(group->enabled, true);
   }

   cleanup_structs_and_vectors();
   disconnect_sr();
}

int main(void)
{
   const struct CMUnitTest tests[] = {
         cmocka_unit_test(test_instance_load_by_name),
         cmocka_unit_test(test_module_load_by_name),
         cmocka_unit_test(test_module_group_load_by_name),
         cmocka_unit_test(test_module_group_load),
         cmocka_unit_test(test_module_load),
         cmocka_unit_test(test_interface_load),
         cmocka_unit_test(test_instance_load),
         cmocka_unit_test(test_instance_pid_restore),
         cmocka_unit_test(test_interface_file_params_load),
         cmocka_unit_test(test_interface_unix_params_load),
         cmocka_unit_test(test_interface_tcp_tls_params_load),
         cmocka_unit_test(test_interface_tcp_params_load),
   };


   return cmocka_run_group_tests(tests, NULL, NULL);
}