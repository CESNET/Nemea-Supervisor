/**
 * @file sr_get_changes_iter_example.c
 * @author Rastislav Szabo <raszabo@cisco.com>, Lukas Macko <lmacko@cisco.com>
 * @brief Example usage of sr_get_changes_iter function.
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

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include "sysrepo.h"
#include <string.h>
#include <sysrepo/values.h>
#include <sysrepo/xpath.h>

int count = 0;


#define NULLP_TEST_AND_FREE(pointer) do { \
   if (pointer != NULL) { \
      free(pointer); \
      pointer = NULL; \
   } \
} while (0);

typedef struct changes_s{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
} changes_t;

static void
print_change(sr_change_oper_t op, sr_val_t *old_val, sr_val_t *new_val) {
    switch(op) {
    case SR_OP_CREATED:
        if (NULL != new_val) {
           printf("CREATED: ");
           sr_print_val(new_val);
        }
        break;
    case SR_OP_DELETED:
        if (NULL != old_val) {
           printf("DELETED: ");
           sr_print_val(old_val);
        }
	break;
    case SR_OP_MODIFIED:
        if (NULL != old_val && NULL != new_val) {
           printf("old value: ");
           sr_print_val(old_val);
           printf("new value: ");
           sr_print_val(new_val);
        }
	break;
    case SR_OP_MOVED:
        if (NULL != new_val) {
            printf("MOVED: %s after %s", new_val->xpath, NULL != old_val ? old_val->xpath : NULL);
        }
	break;
    }
}

char *
event_to_str(sr_notif_event_t ev)
{
    switch (ev) {
    case SR_EV_ABORT:
        return "abort";
    case SR_EV_APPLY:
        return "apply";
    case SR_EV_VERIFY:
        return "verify";
    case SR_EV_ENABLED:
        return "enabled";
    }
    return "unknown";
}

#define testtt(key) "asd"(key)

// return pointer to key or NULL on err or not found
static char * fetch_xpath_val_fr_key(char * xpath, char * node_name, char * xpath_key)
{
   // TODO handle special chars [,],' in key and value. see how are those handled by sysrepo?
   char *tmp;
   char *value = NULL;

#define FST_CHAR_EQ_OR_ERR(chr) do { \
   if (xpath[0] != (chr)) { \
      NULLP_TEST_AND_FREE(value) \
      return NULL; \
   } \
   xpath++; \
} while (0);

   // Parsing example: /node_name[key_name='value']
   FST_CHAR_EQ_OR_ERR('/')

   // node_name[key_name='value']
   tmp = strstr(xpath, node_name);
   if (tmp != xpath) {
      return NULL;
   }
   xpath += strlen(node_name);

   // [key_name='value']
   FST_CHAR_EQ_OR_ERR('[')

   // key_name='value']
   tmp = strstr(xpath, xpath_key);
   if (tmp != xpath) {
      return NULL;
   }
   xpath += strlen(xpath_key);

   // ='value']
   FST_CHAR_EQ_OR_ERR('=')
   // 'value']
   FST_CHAR_EQ_OR_ERR('\'')

   // value']
   tmp = strchr(xpath, '\'');
   if (tmp == NULL) {
      return NULL;
   }
   value = strndup(xpath, tmp - xpath);
   if (value == NULL) {
      return NULL;
   }
   xpath = tmp + 1;
{
   sr_xpath_ctx_t state = {0};
   char *res = NULL;

   res = sr_xpath_next_node(xpath, &state);
   printf("%s\n", res);
   // strcmp might be unecessary
   if (res == NULL || strcmp(res, "nemea-supervisor") != 0) {
      printf("err1\n");
      // TODO err
      sr_xpath_recover(&state);
      return;
   }

   res = sr_xpath_next_node(NULL, &state);
   printf("%s\n", res);
   if (res == NULL || strcmp(res, "module-group") != 0) {
      // TODO err
      printf("err2\n");
      sr_xpath_recover(&state);
      return;
   }

   res = sr_xpath_node_key_value(NULL, "name", &state);
   if (res == NULL) {
      // TODO err
      printf("err3\n");
      sr_xpath_recover(&state);
      return;
   }
   printf("MODULE GROUP=%s\n", res);

   res = sr_xpath_next_node(NULL, &state);
   if (res == NULL || strcmp(res, "module") != 0) {
      sr_xpath_recover(&state);
      printf("group change only1\n");
      // TODO logic for group change
      return;
   }

   res = sr_xpath_node_key_value(NULL, "name", &state);
   if (res == NULL) {
      // TODO err must be module node now
      sr_xpath_recover(&state);
      return;
   }
   printf("MODULE_NAME=%s\n", res);

   sr_xpath_recover(&state);
   return;
   /*
    * FROM OLD VALUE:
    * 1) find concerned module or null
    * 2) find concerned group if module = null
    *
    * FROM NEW VALUE:
    * 1) find whether change is inside module
    * 1.1) if its
    * */



   char *tmp = strstr(xpath, "/nemea:nemea-supervisor");
   if (tmp != xpath) {
      // TODO err
      return;
   }
   xpath += 23;
   printf("xpath=%s\n", xpath);

   char * val = NULL;
   val = fetch_xpath_val_fr_key2(xpath, "module-group", "name");
   printf("val=%s\n", val);
   xpath += strlen(val) + 22;
   printf("xpath=%s\n", xpath);
   NULLP_TEST_AND_FREE(val)
   val = fetch_xpath_val_fr_key2(xpath, "module", "name");
   printf("val=%s\n", val);
   NULLP_TEST_AND_FREE(val)
}

static int
list_changes_cb(sr_session_ctx_t *session, const char *module_name, sr_notif_event_t ev,
                void *private_ctx)
{
    //changes_t *ch = (changes_t *) private_ctx;
   pthread_mutex_t *mutex = (pthread_mutex_t *) private_ctx;
    sr_change_iter_t *it = NULL;
    int rc = SR_ERR_OK;
    sr_change_oper_t op;
    sr_val_t *new_val = NULL;
    sr_val_t *old_val = NULL;

    if (SR_EV_VERIFY != ev) {
        pthread_mutex_lock(&mutex);
    }
    printf ("================ %s event =============\n", event_to_str(ev));

    rc = sr_get_changes_iter(session, "/nemea:nemea-supervisor", &it);
    if (SR_ERR_OK != rc) {
        puts("sr get changes iter failed");
        goto cleanup;
    }
    count = 0;
    while (SR_ERR_OK == (rc = sr_get_change_next(session, it, &op, &old_val, &new_val))) {
        print_change(op, old_val, new_val);
       if (op == SR_OP_MODIFIED && old_val != NULL && old_val->xpath != NULL) {
          parse_xpath(old_val->xpath);
       }
        sr_free_val(old_val);
        sr_free_val(new_val);
        count++;
    }
    puts("======================================================");
cleanup:
    sr_free_change_iter(it);
    if (SR_EV_VERIFY != ev) {
        //pthread_cond_signal(&ch->cond);
        pthread_mutex_unlock(&mutex);
    }
    return SR_ERR_OK;
}

int
main(int argc, char **argv)
{
    sr_conn_ctx_t *conn = NULL;
    sr_session_ctx_t *session = NULL;
    sr_subscription_ctx_t *sub = NULL;
    //changes_t sync = {.mutex = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER, .count = 0};
    sr_val_t value = {0};
    int rc = SR_ERR_OK;
   pthread_mutex_t glob_mutex;
   pthread_mutex_init(&glob_mutex, NULL);

    sr_free_change_iter(it);
    if (SR_EV_VERIFY != ev) {
        //pthread_cond_signal(&ch->cond);
        pthread_mutex_unlock(&mutex);
    }
    return SR_ERR_OK;
}

int
main(int argc, char **argv)
{
    sr_conn_ctx_t *conn = NULL;
    sr_session_ctx_t *session = NULL;
    sr_subscription_ctx_t *sub = NULL;
    changes_t sync = {.mutex = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER, .count = 0};
    sr_val_t value = {0};
    int rc = SR_ERR_OK;
   pthread_mutex_t glob_mutex;
   pthread_mutex_init(&glob_mutex, NULL);


    rc = sr_connect("sr_get_changes_iter", SR_CONN_DEFAULT, &conn);
    if (SR_ERR_OK != rc) {
        goto cleanup;
    }

    rc = sr_session_start(conn, SR_DS_RUNNING, SR_SESS_CONFIG_ONLY, &session);
    if (SR_ERR_OK != rc) {
        goto cleanup;
    }

    rc = sr_subtree_change_subscribe(session, "/nemea:nemea-supervisor", list_changes_cb, &glob_mutex, 0, SR_SUBSCR_DEFAULT, &sub);
    if (SR_ERR_OK != rc) {
        goto cleanup;
    }

    puts("subscription for changes in nemea successful");
    puts("======================================================");

   /*
    value.type = SR_STRING_T;
    value.data.string_val = "/usr/local/bin/vportscan_detectoxxx";

    rc = sr_set_item(session, "/nemea:nemea-supervisor/module-group[name='Detectors']/module[name='vportscan_detector']/path",
            &value, SR_EDIT_DEFAULT);
    if (SR_ERR_OK != rc) {
       puts("here");
       printf("err: %s\n", sr_strerror(rc));
        goto cleanup;
    }
    */

   /*
    value.type = SR_IDENTITYREF_T;
    value.data.identityref_val = "ethernetCsmacd";
    rc = sr_set_item(session, "/ietf-interfaces:interfaces/interface[name='gigaeth1']/type",
            &value, SR_EDIT_DEFAULT);
    if (SR_ERR_OK != rc) {
        goto cleanup;
    }
    */

   rc = sr_delete_item(session, "/nemea:nemea-supervisor/module-group/*", SR_EDIT_DEFAULT);
   if (SR_ERR_OK != rc) {
      printf("err: %s\n", sr_strerror(rc));
      goto cleanup;
   }

    pthread_mutex_lock(&sync.mutex);
    rc = sr_commit(session);
    if (SR_ERR_OK != rc) {
       printf("err: %s\n", sr_strerror(rc));
       puts("failed  to commit");
        goto cleanup;
    }
    //pthread_cond_wait(&sync.cond, &sync.mutex);
    pthread_mutex_unlock(&sync.mutex);
   sleep

cleanup:
    printf("some fail??\n");
    if (NULL != sub) {
        sr_unsubscribe(session, sub);
    }
    if (NULL != session) {
        sr_session_stop(session);
    }
    if (NULL != conn) {
        sr_disconnect(conn);
    }
return rc;

}
