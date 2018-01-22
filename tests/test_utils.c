#include <sysrepo.h>
#include <pthread.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

#include "testing_utils.h"
#include "../src/utils.c"


///////////////////////////HELPERS
///////////////////////////HELPERS
///////////////////////////HELPERS
///////////////////////////HELPERS


///////////////////////////TESTS
///////////////////////////TESTS
///////////////////////////TESTS
///////////////////////////TESTS


void test_vector_delete(void **state)
{
   vector_t v;
   int ints[] = {0, 1, 2, 3, 4};
   vector_init(&v, 5);
   vector_add(&v, &ints[0]);
   vector_add(&v, &ints[1]);
   vector_add(&v, &ints[2]);
   vector_add(&v, &ints[3]);
   vector_add(&v, &ints[4]);

   {
      vector_delete(&v, 2);
      assert_int_equal(v.total, 4);
      assert_int_equal(v.items[0], &ints[0]);
      assert_int_equal(v.items[1], &ints[1]);
      assert_int_equal(v.items[2], &ints[3]);
      assert_int_equal(v.items[3], &ints[4]);

      // reset
      vector_free(&v);
      vector_init(&v, 5);
      vector_add(&v, &ints[0]);
      vector_add(&v, &ints[1]);
      vector_add(&v, &ints[2]);
      vector_add(&v, &ints[3]);
      vector_add(&v, &ints[4]);
   }

   {
      vector_delete(&v, 0);
      assert_int_equal(v.total, 4);
      assert_int_equal(v.items[0], &ints[1]);
      assert_int_equal(v.items[1], &ints[2]);
      assert_int_equal(v.items[2], &ints[3]);
      assert_int_equal(v.items[3], &ints[4]);

      // reset
      vector_free(&v);
      vector_init(&v, 5);
      vector_add(&v, &ints[0]);
      vector_add(&v, &ints[1]);
      vector_add(&v, &ints[2]);
      vector_add(&v, &ints[3]);
      vector_add(&v, &ints[4]);
   }

   {
      vector_delete(&v, 4);
      assert_int_equal(v.total, 4);
      assert_int_equal(v.items[0], &ints[0]);
      assert_int_equal(v.items[1], &ints[1]);
      assert_int_equal(v.items[2], &ints[2]);
      assert_int_equal(v.items[3], &ints[3]);

      // reset
      vector_free(&v);
      vector_init(&v, 5);
      vector_add(&v, &ints[0]);
      vector_add(&v, &ints[1]);
      vector_add(&v, &ints[2]);
      vector_add(&v, &ints[3]);
      vector_add(&v, &ints[4]);
   }
}

int main(void)
{

   const struct CMUnitTest tests[] = {
         cmocka_unit_test(test_vector_delete),
   };

   return cmocka_run_group_tests(tests, NULL, NULL);
}