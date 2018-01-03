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
#include "sysrepo/values.h"


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
           printf("MODIFIED: ");
           printf("old value");
           sr_print_val(old_val);
           printf("new value");
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

static int
list_changes_cb(sr_session_ctx_t *session, const char *module_name, sr_notif_event_t ev, void *private_ctx)
{
    changes_t *ch = (changes_t *) private_ctx;
    sr_change_iter_t *it = NULL;
    int rc = SR_ERR_OK;
    sr_change_oper_t op;
    sr_val_t *new_val = NULL;
    sr_val_t *old_val = NULL;

    if (SR_EV_VERIFY != ev) {
        pthread_mutex_lock(&ch->mutex);
    }
    printf ("================ %s event =============\n", event_to_str(ev));

    rc = sr_get_changes_iter(session, "/ietf-interfaces:interfaces", &it);
    if (SR_ERR_OK != rc) {
        puts("sr get changes iter failed");
        goto cleanup;
    }
    ch->count = 0;
    while (SR_ERR_OK == (rc = sr_get_change_next(session, it, &op, &old_val, &new_val))) {
        print_change(op, old_val, new_val);
        sr_free_val(old_val);
        sr_free_val(new_val);
        ch->count++;
    }
    puts("======================================================");
cleanup:
    sr_free_change_iter(it);
    if (SR_EV_VERIFY != ev) {
        pthread_cond_signal(&ch->cond);
        pthread_mutex_unlock(&ch->mutex);
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


    rc = sr_connect("sr_get_changes_iter", SR_CONN_DEFAULT, &conn);
    if (SR_ERR_OK != rc) {
        goto cleanup;
    }

    rc = sr_session_start(conn, SR_DS_RUNNING, SR_SESS_CONFIG_ONLY, &session);
    if (SR_ERR_OK != rc) {
        goto cleanup;
    }

		//rc = sr_module_change_subscribe(session, "ietf-interfaces", list_changes_cb, &sync, 0, SR_SUBSCR_DEFAULT, &sub);
		rc = sr_subtree_change_subscribe(session, "/nemea:nemea-supervisor/module-group", list_changes_cb, &sync, 0, SR_SUBSCR_DEFAULT, &sub);
    if (SR_ERR_OK != rc) {
			printf("e=%s\n", sr_strerror(rc));
        goto cleanup;
    }
		rc = sr_subtree_change_subscribe(session, "/nemea:nemea-supervisor/module-group", list_changes_cb, &sync, 1, SR_SUBSCR_DEFAULT, &sub);
    if (SR_ERR_OK != rc) {
			printf("e=%s\n", sr_strerror(rc));
        goto cleanup;
    }

    puts("subscription for changes in ietf-interfaces successful");
    puts("======================================================");

    value.type = SR_UINT8_T;
    value.data.uint8_val = 64;
    rc = sr_set_item(session, "/ietf-interfaces:interfaces/interface[name='gigaeth1']/ietf-ip:ipv6/address[ip='fe80::ab8']/prefix-length",
            &value, SR_EDIT_DEFAULT);
    if (SR_ERR_OK != rc) {
        goto cleanup;
    }

    value.type = SR_IDENTITYREF_T;
    value.data.identityref_val = "ethernetCsmacd";
    rc = sr_set_item(session, "/ietf-interfaces:interfaces/interface[name='gigaeth1']/type",
            &value, SR_EDIT_DEFAULT);
    if (SR_ERR_OK != rc) {
        goto cleanup;
    }

    pthread_mutex_lock(&sync.mutex);
    rc = sr_commit(session);
    if (SR_ERR_OK != rc) {
        goto cleanup;
    }
    pthread_cond_wait(&sync.cond, &sync.mutex);
    pthread_mutex_unlock(&sync.mutex);

cleanup:
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
