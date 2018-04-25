#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

#include "../src/utils.h"
#include "../src/module.c"
#include "testing_utils.h"


///////////////////////////HELPERS
///////////////////////////HELPERS
///////////////////////////HELPERS
///////////////////////////HELPERS


static void alloc_module_and_ifc(inst_t **module, interface_t **ifc)
{

   inst_t *test_module = inst_alloc();
   if (test_module == NULL) { fail_msg("Failed to allocate tests module."); }
   test_module->name = strdup("tests-module");
   if (test_module->name == NULL) { fail_msg("Failed to allocate tests module name."); }
   interface_t *test_ifc = interface_alloc();
   if (test_ifc == NULL) { fail_msg("Failed to allocate tests interface."); }

   *module = test_module;
   *ifc = test_ifc;
}

static interface_t * get_test_ifc(interface_dir_t dir,
                                  interface_type_t type,
                                  uint16_t uniq_num)
{
   interface_t *ifc = interface_alloc();
   if (ifc == NULL) { fail_msg("Failed to allocate tests interface."); }

   switch (type) {
      case NS_IF_TYPE_TCP:
      {
         ifc->type = NS_IF_TYPE_TCP;
         ifc->direction = dir;
         assert_int_equal(interface_specific_params_alloc(ifc), 0);
         ifc->specific_params.tcp->port = uniq_num;
         if (dir == NS_IF_DIR_OUT) {
            ifc->specific_params.tcp->host = strdup("192.168.0.1");
            IF_NO_MEM_FAIL_MSG(ifc->specific_params.tcp->host,
                               "ifc->specific_params.tcp->host")
         }
         // t:192.168.0.1:uniq_num
      }
         break;

      case NS_IF_TYPE_UNIX:
      {
         ifc->type = NS_IF_TYPE_UNIX;
         ifc->direction = dir;
         assert_int_equal(interface_specific_params_alloc(ifc), 0);
         char s[15];
         sprintf(s, "sock_%d", uniq_num);
         ifc->specific_params.nix->socket_name = strdup(s);
         IF_NO_MEM_FAIL_MSG(ifc->specific_params.nix->socket_name,
                            "ifc->specific_params.nix->socket_name")

         if (dir == NS_IF_DIR_OUT) {
            ifc->specific_params.nix->max_clients = 123;
         }
         // u:socket_name:uniq_num
      }
         break;

      default:
         // Compiler libation
         break;
   }

   return ifc;
}




///////////////////////////TESTS
///////////////////////////TESTS
///////////////////////////TESTS
///////////////////////////TESTS
static void test_module_load_exec_args(void **state)
{
   interface_t *ifc;
   av_module_t *mod = av_module_alloc();
   IF_NO_MEM_FAIL_MSG(mod, "fialed to allocate module struct")
   mod->use_trap_ifces = true;

   inst_t *inst = inst_alloc();
   IF_NO_MEM_FAIL_MSG(inst, "tests module")
   inst->mod_ref = mod;

   inst->name = strdup("tests-module");
   IF_NO_MEM_FAIL_MSG(inst->name, "inst->name")
   inst->params = strdup("-p1 -p2 --p3 p4");
   IF_NO_MEM_FAIL_MSG(inst->params, "inst->params")
   inst->mod_ref->path = strdup("./intable_module");
   IF_NO_MEM_FAIL_MSG(inst->mod_ref->path, "inst->module->path")
   ifc = get_test_ifc(NS_IF_DIR_OUT, NS_IF_TYPE_TCP, 1);
   inst_interface_add(inst, ifc);

   {
      assert_int_equal(inst_gen_exec_args(inst), 0);
      assert_non_null(inst->exec_args);
      assert_string_equal(inst->exec_args[0], inst->name);
      assert_string_equal(inst->exec_args[1], "-p1");
      assert_string_equal(inst->exec_args[2], "-p2");
      assert_string_equal(inst->exec_args[3], "--p3");
      assert_string_equal(inst->exec_args[4], "p4");
      assert_string_equal(inst->exec_args[5], "-i");
      assert_string_equal(inst->exec_args[6], "t:192.168.0.1:1");
      assert_null(inst->exec_args[7]);

      for (int i = 0; inst->exec_args[i] != NULL; i++) {
         free(inst->exec_args[i]);
      }
      free(inst->exec_args);
   }

   ifc = get_test_ifc(NS_IF_DIR_IN, NS_IF_TYPE_UNIX, 2);
   inst_interface_add(inst, ifc);
   {
      assert_int_equal(inst_gen_exec_args(inst), 0);
      assert_non_null(inst->exec_args);
      assert_string_equal(inst->exec_args[0], inst->name);
      assert_string_equal(inst->exec_args[1], "-p1");
      assert_string_equal(inst->exec_args[2], "-p2");
      assert_string_equal(inst->exec_args[3], "--p3");
      assert_string_equal(inst->exec_args[4], "p4");
      assert_string_equal(inst->exec_args[5], "-i");
      assert_string_equal(inst->exec_args[6], "u:sock_2,t:192.168.0.1:1");
      assert_null(inst->exec_args[7]);

      for (int i = 0; inst->exec_args[i] != NULL; i++) {
         free(inst->exec_args[i]);
      }
      free(inst->exec_args);
   }

   { // test with use_trap_ifces == false (eg. IPFIXCOL)
      mod->use_trap_ifces = false;

      assert_int_equal(inst_gen_exec_args(inst), 0);
      assert_non_null(inst->exec_args);
      assert_string_equal(inst->exec_args[0], inst->name);
      assert_string_equal(inst->exec_args[1], "-p1");
      assert_string_equal(inst->exec_args[2], "-p2");
      assert_string_equal(inst->exec_args[3], "--p3");
      assert_string_equal(inst->exec_args[4], "p4");

      for (int i = 0; inst->exec_args[i] != NULL; i++) {
         free(inst->exec_args[i]);
      }
      free(inst->exec_args);
   }

   { // test only interfaces param
      mod->use_trap_ifces = true;

      NULLP_TEST_AND_FREE(inst->params)
      assert_int_equal(inst_gen_exec_args(inst), 0);
      assert_non_null(inst->exec_args);
      assert_string_equal(inst->exec_args[0], inst->name);
      assert_string_equal(inst->exec_args[1], "-i");
      assert_string_equal(inst->exec_args[2], "u:sock_2,t:192.168.0.1:1");
      assert_null(inst->exec_args[3]);

      for (int i = 0; inst->exec_args[i] != NULL; i++) {
         free(inst->exec_args[i]);
      }
      free(inst->exec_args);
   }

   { // test sysrepo params
      mod->sr_rdy = true;
      mod->use_trap_ifces = true;
      inst->use_sysrepo = true;

      assert_int_equal(inst_gen_exec_args(inst), 0);
      assert_non_null(inst->exec_args);
      assert_string_equal(inst->exec_args[0], inst->name);
      assert_string_equal(inst->exec_args[1], "-x");
      assert_string_equal(inst->exec_args[2], "tests-module");
      assert_string_equal(inst->exec_args[3], "-i");
      assert_string_equal(inst->exec_args[4], "u:sock_2,t:192.168.0.1:1");
   }



   av_module_free(mod);
   inst_free(inst);
}

static void test_module_params_no_ifc_arr(void **state)
{
   int rc;
   uint32_t params_num;
   char **parsed_params;
   inst_t *i1 = inst_alloc();
   if (i1 == NULL) { fail_msg("Failed to allocate new module."); }
   i1->name = strdup("tests-module");
   IF_NO_MEM_FAIL(i1->name)
   i1->params = strdup("-vvv");
   IF_NO_MEM_FAIL(i1->params)

   {
      parsed_params = module_params_to_ifc_arr(i1, &params_num, &rc);
      assert_int_equal(rc, 0);
      assert_int_equal(params_num, 1);
      assert_string_equal(parsed_params[0], i1->params);
      assert_null(parsed_params[params_num]);

      // cleanup
      NULLP_TEST_AND_FREE(parsed_params[0])
      NULLP_TEST_AND_FREE(parsed_params)
      NULLP_TEST_AND_FREE(i1->params)
   }

   {
      i1->params = strdup("-v v xxv tt rr -vv --yyy");
      IF_NO_MEM_FAIL(i1->params)
      parsed_params = module_params_to_ifc_arr(i1, &params_num, &rc);
      assert_int_equal(rc, 0);
      assert_int_equal(params_num, 7);
      assert_string_equal(parsed_params[0], "-v");
      assert_string_equal(parsed_params[1], "v");
      assert_string_equal(parsed_params[2], "xxv");
      assert_string_equal(parsed_params[3], "tt");
      assert_string_equal(parsed_params[4], "rr");
      assert_string_equal(parsed_params[5], "-vv");
      assert_string_equal(parsed_params[6], "--yyy");
      assert_null(parsed_params[params_num]);

      // cleanup
      for (int i = 0; i < 7; i++) {
         NULLP_TEST_AND_FREE(parsed_params[i])
      }
      NULLP_TEST_AND_FREE(parsed_params)
   }

   inst_free(i1);
}

static void test_module_get_ifcs_as_arg(void **state)
{
   inst_t *mod;
   interface_t *ifc;
   char *params;

   mod = inst_alloc();
   IF_NO_MEM_FAIL_MSG(mod, "mod");

   ifc = get_test_ifc(NS_IF_DIR_IN, NS_IF_TYPE_TCP, 1);
   inst_interface_add(mod, ifc);
   params = module_get_ifcs_as_arg(mod);
   assert_non_null(params);
   assert_string_equal(params, "t:1");
   free(params);

   ifc = get_test_ifc(NS_IF_DIR_OUT, NS_IF_TYPE_UNIX, 2);
   inst_interface_add(mod, ifc);
   params = module_get_ifcs_as_arg(mod);
   assert_non_null(params);
   assert_string_equal(params, "t:1,u:sock_2:123");
   free(params);

   ifc = get_test_ifc(NS_IF_DIR_IN, NS_IF_TYPE_UNIX, 3);
   inst_interface_add(mod, ifc);
   params = module_get_ifcs_as_arg(mod);
   assert_non_null(params);
   assert_string_equal(params, "t:1,u:sock_3,u:sock_2:123");
   free(params);

   ifc = get_test_ifc(NS_IF_DIR_OUT, NS_IF_TYPE_TCP, 4);
   inst_interface_add(mod, ifc);
   params = module_get_ifcs_as_arg(mod);
   assert_non_null(params);
   assert_string_equal(params, "t:1,u:sock_3,u:sock_2:123,t:192.168.0.1:4");
   free(params);

   ifc->autoflush = strdup("off");
   IF_NO_MEM_FAIL_MSG(ifc->autoflush, "ifc->autoflush")
   ifc->buffer = strdup("on");
   IF_NO_MEM_FAIL_MSG(ifc->buffer, "ifc->buffer")
   ifc->timeout = strdup("HALF_WAIT");
   IF_NO_MEM_FAIL_MSG(ifc->timeout, "ifc->timeout")
   params = module_get_ifcs_as_arg(mod);
   assert_non_null(params);
   assert_string_equal(params,
                       "t:1,u:sock_3,u:sock_2:123,""t:192.168.0.1:4:"
                             "buffer=on:autoflush=off:timeout=HALF_WAIT");
   free(params);

   inst_free(mod);
}

static void test_tcp_ifc_to_cli_arg(void **state)
{
   inst_t *mod;
   interface_t *ifc;
   char * params;

   alloc_module_and_ifc(&mod, &ifc);
   ifc->type = NS_IF_TYPE_TCP;
   assert_int_equal(interface_specific_params_alloc(ifc), 0);

   // Required params
   ifc->specific_params.tcp->port = 1111;
   params = ifc->ifc_to_cli_arg_fn(ifc);
   assert_non_null(params);
   assert_string_equal(params, "t:1111");
   NULLP_TEST_AND_FREE(params)

   ifc->specific_params.tcp->host = strdup("192.168.0.1");
   IF_NO_MEM_FAIL_MSG(ifc->specific_params.tcp->host, "ifc->specific_params.tcp->host")
   params = ifc->ifc_to_cli_arg_fn(ifc);
   assert_non_null(params);
   assert_string_equal(params, "t:192.168.0.1:1111");
   NULLP_TEST_AND_FREE(params)

   ifc->specific_params.tcp->max_clients = 123;
   params = ifc->ifc_to_cli_arg_fn(ifc);
   assert_non_null(params);
   assert_string_equal(params, "t:192.168.0.1:1111:123");
   NULLP_TEST_AND_FREE(params)

   inst_free(mod);
   interface_free(ifc);
}

static void test_tcp_tls_ifc_to_cli_arg(void **state)
{
   inst_t *mod;
   interface_t *ifc;
   char * params;

   alloc_module_and_ifc(&mod, &ifc);
   ifc->type = NS_IF_TYPE_TCP_TLS;
   assert_int_equal(interface_specific_params_alloc(ifc), 0);

   // Required params
   ifc->specific_params.tcp_tls->port = 1111;
   ifc->specific_params.tcp_tls->keyfile = strdup("/key/path");
   IF_NO_MEM_FAIL_MSG(ifc->specific_params.tcp_tls->keyfile, "ifc->specific_params.tcp_tls->keyfile")
   ifc->specific_params.tcp_tls->certfile = strdup("/cert/path");
   IF_NO_MEM_FAIL_MSG(ifc->specific_params.tcp_tls->certfile, "ifc->specific_params.tcp_tls->certfile")
   ifc->specific_params.tcp_tls->cafile = strdup("/ca/path");
   IF_NO_MEM_FAIL_MSG(ifc->specific_params.tcp_tls->cafile, "ifc->specific_params.tcp_tls->cafile")
   params = ifc->ifc_to_cli_arg_fn(ifc);
   assert_non_null(params);
   assert_string_equal(params, "T:1111:/key/path:/cert/path:/ca/path");
   NULLP_TEST_AND_FREE(params)

   ifc->specific_params.tcp_tls->host = strdup("host.com");
   IF_NO_MEM_FAIL_MSG(ifc->specific_params.tcp_tls->host, "ifc->specific_params.tcp_tls->host")
   params = ifc->ifc_to_cli_arg_fn(ifc);
   assert_non_null(params);
   assert_string_equal(params, "T:host.com:1111:/key/path:/cert/path:/ca/path");
   NULLP_TEST_AND_FREE(params)


   ifc->specific_params.tcp_tls->max_clients = 123;
   params = ifc->ifc_to_cli_arg_fn(ifc);
   assert_non_null(params);
   assert_string_equal(params, "T:host.com:1111:123:/key/path:/cert/path:/ca/path");
   NULLP_TEST_AND_FREE(params)

   inst_free(mod);
   interface_free(ifc);
}

static void test_unix_ifc_to_cli_arg(void **state)
{
   inst_t *mod;
   interface_t *ifc;
   char * params;

   alloc_module_and_ifc(&mod, &ifc);
   ifc->type = NS_IF_TYPE_UNIX;
   assert_int_equal(interface_specific_params_alloc(ifc), 0);

   ifc->specific_params.nix->socket_name = strdup("socket_name");
   IF_NO_MEM_FAIL_MSG(ifc->specific_params.nix->socket_name, "ifc->specific_params.nix->socket_name")
   params = ifc->ifc_to_cli_arg_fn(ifc);
   assert_non_null(params);
   assert_string_equal(params, "u:socket_name");
   NULLP_TEST_AND_FREE(params)

   ifc->specific_params.nix->max_clients = 123;
   params = ifc->ifc_to_cli_arg_fn(ifc);
   assert_non_null(params);
   assert_string_equal(params, "u:socket_name:123");
   NULLP_TEST_AND_FREE(params)

   inst_free(mod);
   interface_free(ifc);
}

static void test_file_ifc_to_cli_arg(void **state)
{
   inst_t *mod;
   interface_t *ifc;
   char * params;

   alloc_module_and_ifc(&mod, &ifc);
   ifc->type = NS_IF_TYPE_FILE;
   assert_int_equal(interface_specific_params_alloc(ifc), 0);

   ifc->specific_params.file->name = strdup("/file/path");
   IF_NO_MEM_FAIL(ifc->specific_params.file->name)
   params = ifc->ifc_to_cli_arg_fn(ifc);
   assert_non_null(params);
   assert_string_equal(params, "f:/file/path");
   NULLP_TEST_AND_FREE(params)

   ifc->specific_params.file->time = 123;
   params = ifc->ifc_to_cli_arg_fn(ifc);
   assert_non_null(params);
   assert_string_equal(params, "f:/file/path:123");
   NULLP_TEST_AND_FREE(params)

   ifc->specific_params.file->size = 223;
   params = ifc->ifc_to_cli_arg_fn(ifc);
   assert_non_null(params);
   assert_string_equal(params, "f:/file/path:223:123");
   NULLP_TEST_AND_FREE(params)

   ifc->specific_params.file->mode = strdup("a");
   IF_NO_MEM_FAIL(ifc->specific_params.file->mode)
   params = ifc->ifc_to_cli_arg_fn(ifc);
   assert_non_null(params);
   assert_string_equal(params, "f:/file/path:a:223:123");
   NULLP_TEST_AND_FREE(params)

   inst_free(mod);
   interface_free(ifc);
}

static void test_bh_ifc_to_cli_arg(void **state)
{
   inst_t *mod;
   interface_t *ifc;
   alloc_module_and_ifc(&mod, &ifc);
   ifc->type = NS_IF_TYPE_BH;
   assert_int_equal(interface_specific_params_alloc(ifc), 0);

   char * params = ifc->ifc_to_cli_arg_fn(ifc);
   assert_non_null(params);
   assert_string_equal(params, "b");
   NULLP_TEST_AND_FREE(params)

   inst_free(mod);
   interface_free(ifc);
}

static void test_strcat_many(void **state)
{
   char *a = strcat_many(3, "a", "b", "c");
   assert_string_equal(a, "abc");
   NULLP_TEST_AND_FREE(a)


}

int main(void)
{
   //verbosity_level = V3;
   const struct CMUnitTest tests[] = {
         cmocka_unit_test(test_module_load_exec_args),
         cmocka_unit_test(test_unix_ifc_to_cli_arg),
         cmocka_unit_test(test_module_params_no_ifc_arr),
         cmocka_unit_test(test_module_get_ifcs_as_arg),
         cmocka_unit_test(test_tcp_ifc_to_cli_arg),
         cmocka_unit_test(test_tcp_tls_ifc_to_cli_arg),
         cmocka_unit_test(test_strcat_many),
         cmocka_unit_test(test_bh_ifc_to_cli_arg),
         cmocka_unit_test(test_file_ifc_to_cli_arg),
   };

   return cmocka_run_group_tests(tests, NULL, NULL);
}