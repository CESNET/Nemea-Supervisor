#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

/*--BEGIN superglobal symbolic const--*/
/*--END superglobal symbolic const--*/

/*--BEGIN macros--*/
#define IF_NO_MEM_FAIL_MSG(ptr, msg) do { \
   if ((ptr) == NULL) { \
   fail_msg("no memory to allocate %s", (msg)); \
   } \
} while (0);

#define IF_NO_MEM_FAIL(ptr) do { \
   if ((ptr) == NULL) { \
      fail(); \
   } \
} while (0);

#define PRINT_LASTEST_SR_ERRS(sess) do { \
   { \
      const sr_error_info_t *errs_45454 = NULL; \
      size_t cnt_45454; \
      sr_get_last_errors((sess), &errs_45454, &cnt_45454); \
      printf("Following are last %d sysrepo errors:\n", (int) cnt_45454); \
      for (int i_45454 = 0; i_45454 < cnt_45454; i_45454++) { \
         printf(" %d) xpath=%s, err:%s\n", i_45454, \
         errs_45454[i_45454].xpath, \
         errs_45454[i_45454].message); \
      } \
   } \
} while(0);

#define IF_SR_ERR_FAIL(rc) do { \
   if ((rc) != SR_ERR_OK) { \
      fail_msg("Sysrepo error - %s\n", sr_strerror((rc))); \
   } \
} while(0);
/*--END macros--*/

/*--BEGIN superglobal typedef--*/
/*--END superglobal typedef--*/

/*--BEGIN superglobal vars--*/
// denote with extern
//extern sr_conn_ctx_t *sr_con = NULL;
/*--END superglobal vars--*/

/*--BEGIN superglobal fn prototypes--*/
// denote with extern
/*--END superglobal fn prototypes--*/

#endif
