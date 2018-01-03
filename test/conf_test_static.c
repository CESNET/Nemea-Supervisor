#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <sysrepo.h>
#include <sysrepo/xpath.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define VERBOSE(level, fmt, ...) do { \
   printf(fmt, ##__VA_ARGS__); \
} while (0);

#define NO_MEM_ERR do { \
   VERBOSE(N_ERR, "Failed to allocate memory in %s %s:%d", \
         __FILE__, __func__,  __LINE__) \
} while (0);

#define NULLP_TEST_AND_FREE(pointer) do { \
   if (pointer != NULL) { \
      free(pointer); \
      pointer = NULL; \
   } \
} while (0);

const char *ns_root_sr_path = "/nemea-test-1:nemea-supervisor";
typedef enum ns_change_type_e {
   NS_CHE_T_INVAL = -1,
   NS_CHE_T_GRP_NOD,
   NS_CHE_T_GRP_RT,
   NS_CHE_T_MOD_NOD,
   NS_CHE_T_MOD_RT,
} ns_change_type_t;

typedef struct ns_change_s {
   char *grp_name;
   char *mod_name;
   ns_change_type_t type;
} ns_change_t;

static int ns_change_elem_load(ns_change_t *elem, char *xpath)
{
   sr_xpath_ctx_t state = {0};
   char *res = NULL;

   res = sr_xpath_next_node(xpath, &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to parse ns_change_elem from xpath: '%s' on line %d",
              xpath, __LINE__)
      sr_xpath_recover(&state);
      return -1;
   }

   res = sr_xpath_next_node(NULL, &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to parse ns_change_elem from xpath: '%s' on line %d",
              xpath, __LINE__)
      sr_xpath_recover(&state);
      return -1;
   }

   res = sr_xpath_node_key_value(NULL, "name", &state);
   if (res == NULL) {
      VERBOSE(N_ERR, "Failed to parse ns_change_elem name from xpath: '%s'",
              xpath)
      sr_xpath_recover(&state);
      return -1;
   }
   elem->grp_name = strdup(res);
   if (elem->grp_name == NULL) {
      NO_MEM_ERR
      sr_xpath_recover(&state);
      return -1;
   }

   res = sr_xpath_next_node(NULL, &state);
   if (res == NULL) {
      elem->type = NS_CHE_T_GRP_RT;
      sr_xpath_recover(&state);
      return 0;
   }

   res = sr_xpath_node_key_value(NULL, "name", &state);
   if (res == NULL) {
      elem->type = NS_CHE_T_GRP_NOD;
      sr_xpath_recover(&state);
      return 0;
   }
   elem->mod_name = strdup(res);
   if (elem->mod_name == NULL) {
      NO_MEM_ERR
      NULLP_TEST_AND_FREE(elem->grp_name)
      sr_xpath_recover(&state);
      return -1;
   }

   res = sr_xpath_next_node(NULL, &state);
   if (res == NULL) {
      elem->type = NS_CHE_T_MOD_RT;
      sr_xpath_recover(&state);
      return 0;
   }

   elem->type = NS_CHE_T_MOD_NOD;
   sr_xpath_recover(&state);
   return 0;
}
static void ns_change_elem_load_test_group_node(void **state)
{

   int rc;
   ns_change_t elem = {
         .grp_name = NULL,
         .mod_name = NULL,
         .type = NS_CHE_T_INVAL
   };
   char xpath[] = "/nemea-test-1:nemea-supervisor/module-group[name='g1']";

   rc = ns_change_elem_load(&elem, xpath);
   assert_int_equal(0, rc);
   assert_string_equal(elem.grp_name, "g1");
   assert_null(elem.mod_name);
   assert_int_equal(elem.type, NS_CHE_T_GRP_RT);
}
static void ns_change_elem_load_test_group_leaf(void **state)
{
   int rc;
   ns_change_t elem = {
         .grp_name = NULL,
         .mod_name = NULL,
         .type = NS_CHE_T_INVAL
   };
   char xpath[] = "/nemea-test-1:nemea-supervisor/module-group[name='g1']/enabled";

   rc = ns_change_elem_load(&elem, xpath);
   assert_int_equal(0, rc);
   assert_string_equal(elem.grp_name, "g1");
   assert_null(elem.mod_name);
   assert_int_equal(elem.type, NS_CHE_T_GRP_NOD);
}
static void ns_change_elem_load_test_module_node(void **state)
{
   int rc;
   ns_change_t elem = {
         .grp_name = NULL,
         .mod_name = NULL,
         .type = NS_CHE_T_INVAL
   };
   char xpath[] = "/nemea-test-1:nemea-supervisor/module-group[name='g1']/module[name='m1']";

   rc = ns_change_elem_load(&elem, xpath);
   assert_int_equal(0, rc);
   assert_string_equal(elem.grp_name, "g1");
   assert_string_equal(elem.mod_name, "m1");
   assert_int_equal(elem.type, NS_CHE_T_MOD_RT);
}
static void ns_change_elem_load_test_module_leaf(void **state)
{
   int rc;
   ns_change_t elem = {
         .grp_name = NULL,
         .mod_name = NULL,
         .type = NS_CHE_T_INVAL
   };
   char xpath[] = "/nemea-test-1:nemea-supervisor/module-group[name='g1']/module[name='m1']/path";

   rc = ns_change_elem_load(&elem, xpath);
   assert_int_equal(0, rc);
   assert_string_equal(elem.grp_name, "g1");
   assert_string_equal(elem.mod_name, "m1");
   assert_int_equal(elem.type, NS_CHE_T_MOD_NOD);
}

int main(void)
{
   const struct CMUnitTest tests[] = {
         cmocka_unit_test(ns_change_elem_load_test_group_node),
         cmocka_unit_test(ns_change_elem_load_test_group_leaf),
         cmocka_unit_test(ns_change_elem_load_test_module_node),
         cmocka_unit_test(ns_change_elem_load_test_module_leaf),
   };

   return cmocka_run_group_tests(tests, NULL, NULL);
}