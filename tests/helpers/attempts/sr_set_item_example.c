/**
 * @file sr_set_item_example.c
 * @author Rastislav Szabo <raszabo@cisco.com>, Lukas Macko <lmacko@cisco.com>
 * @brief Example usage of sr_set_item_example function.
 *
 * @copyright
 * Copyright 2016 Cisco Systems, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "sysrepo.h"

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


int
main(int argc, char **argv)
{
    sr_conn_ctx_t *conn = NULL;
    sr_session_ctx_t *sess = NULL;
    sr_val_t value = { 0 };
    int rc = SR_ERR_OK;
    //sr_log_stderr(SR_LL_DBG);

    /* connect to sysrepo */
    rc = sr_connect("app3", SR_CONN_DEFAULT, &conn);
    if (SR_ERR_OK != rc) {
        goto cleanup;
    }

    /* start session */
    rc = sr_session_start(conn, SR_DS_STARTUP, SR_SESS_DEFAULT, &sess);
    if (SR_ERR_OK != rc) {
        goto cleanup;
    }

    /* create new interface named 'gigaeth0' of type 'ethernetCsmacd' */
    value.type = SR_STRING_T;

    value.data.string_val = "iana-if-type:ethernetCsmacd";
    rc = sr_set_item(sess, "/nemea-test-1:supervisor/available-module[name='m1']/path", &value, SR_EDIT_DEFAULT);
    if (SR_ERR_OK != rc) {
        printf("Error by sr_set_item: %s\n", sr_strerror(rc));
   PRINT_LASTEST_SR_ERRS(sess); 
        goto cleanup;
    }

    ///* set 'prefix-length' leaf inside of the 'address' list entry with key 'fe80::ab8'
    //   (list entry will be automatically created if it does not exist) */
    //value.type = SR_UINT8_T;
    //value.data.uint8_val = 64;
    //rc = sr_set_item(sess, "/ietf-interfaces:interfaces/interface[name='gigaeth0']/ietf-ip:ipv6/address[ip='fe80::ab8']/prefix-length",
    //        &value, SR_EDIT_DEFAULT);
    //if (SR_ERR_OK != rc) {
    //    printf("Error by sr_set_item: %s\n", sr_strerror(rc));
    //    goto cleanup;
    //}

    /* commit the changes */
    rc = sr_commit(sess);
    if (SR_ERR_OK != rc) {
        printf("Error by sr_commit: %s\n", sr_strerror(rc));
        goto cleanup;
    }
   PRINT_LASTEST_SR_ERRS(sess); 

cleanup:
    if (NULL != sess) {
        sr_session_stop(sess);
    }
    if (NULL != conn) {
        sr_disconnect(conn);
    }
    return rc;
}
