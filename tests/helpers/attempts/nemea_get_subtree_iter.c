/**
 * @file sr_get_subtrees_iter_example.c
 * @author Rastislav Szabo <raszabo@cisco.com>, Lukas Macko <lmacko@cisco.com>,
 *         Milan Lenco <milan.lenco@pantheon.tech>
 * @brief Example usage of sr_get_subtree function in the SR_GET_SUBTREE_ITERATIVE mode.
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
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sysrepo.h>
#include <sysrepo/trees.h>


typedef enum {
   T_SUP_R, // nemea-supervisor root node
   T_MODULE_GROUP_R, // module-group root nodes
   T_MODULE_GROUP_N, // module-group nodes: name, enabled, modules_ll
   T_MODULE_R, // module root node
   T_MODULE_N, // module nodes: name, path, enabled, params, module-restarts
} sr_tree_lvl;


void ttt(sr_session_ctx_t *sess, sr_node_t *subtree)
{
}

void
tree_traversal_recursive(sr_session_ctx_t *sess, sr_node_t *subtree, size_t *node_cnt, sr_tree_lvl level)
{
    sr_node_t *child = NULL;
    int x;

    *node_cnt += 1;

    if (NULL == subtree) {
       printf("found end\n");
       return;
    }

    printf("Visited node=%s,node_cnt=%d,level=%d.\n", subtree->name,*node_cnt,level);
    child = sr_node_get_child(sess, subtree);
    
    while (NULL != child) {
       switch(level) {
        case T_SUP_R:
           printf("ROOT LEVEL\n");
           tree_traversal_recursive(sess, child, node_cnt, T_MODULE_GROUP_R);
           child = sr_node_get_next_sibling(sess, child);
           break;

        case T_MODULE_GROUP_R:
           printf("  MGROOT LEVEL\n");
           tree_traversal_recursive(sess, child, node_cnt, T_MODULE_GROUP_N);
           child = sr_node_get_next_sibling(sess, child);
           printf("  child=%s\n", child == NULL ? "null" : "not null");
           break;

        case T_MODULE_GROUP_N:
           printf("    MGNODE LEVEL\n");
           if (0 == strcmp(child->name, "name")) {
              printf(" -module-group=%s\n", child->data);
           } else if (0 == strcmp(child->name, "enabled")) {

           } else if (0 == strcmp(child->name, "module")) {
              tree_traversal_recursive(sess, child, node_cnt, T_MODULE_R);
           }
           child = sr_node_get_next_sibling(sess, child);
           break;

        case T_MODULE_R:
           printf("      MOD ROOT LEVEL\n");
           tree_traversal_recursive(sess, child, node_cnt, T_MODULE_N);
           child = sr_node_get_next_sibling(sess, child);
           break;

        case T_MODULE_N:
           printf("        MOD NODE LEVEL\n");
           if (0 == strcmp(child->name, "name")) {
           } else if (0 == strcmp(child->name, "path")) {
           } else if (0 == strcmp(child->name, "enabled")) {
           } else if (0 == strcmp(child->name, "params")) {
           } else if (0 == strcmp(child->name, "module-restarts")) {
           } else if (0 == strcmp(child->name, "iface")) {
              // TODO dig down
           }
           child = sr_node_get_next_sibling(sess, child);
           break;
       }
    }
}

void
tree_traversal(sr_session_ctx_t *sess, sr_node_t *tree)
{
    size_t node_cnt = 0;
    printf("\nStarting tree traversal...\n");
    tree_traversal_recursive(sess, tree, &node_cnt, T_SUP_R);
    printf("Tree traversal finished, visited %lu nodes.\n", node_cnt);
}

int
main(int argc, char **argv)
{
    sr_conn_ctx_t *conn = NULL;
    sr_session_ctx_t *sess = NULL;
    const char *xpath = NULL;
    sr_node_t *tree = NULL;
    int rc = SR_ERR_OK;

    /* turn on debug logging to stderr - to see what's happening behind the scenes */
    sr_log_stderr(SR_LL_DBG);

    /* connect to sysrepo */
    rc = sr_connect("app1", SR_CONN_DEFAULT, &conn);
    if (SR_ERR_OK != rc) {
        goto cleanup;
    }

    /* start session */
    rc = sr_session_start(conn, SR_DS_STARTUP, SR_SESS_DEFAULT, &sess);
    if (SR_ERR_OK != rc) {
        goto cleanup;
    }

    /* get complete configuration of all interfaces iterativelly */
    xpath = "/nemea:nemea-supervisor";
    rc = sr_get_subtree(sess, xpath, SR_GET_SUBTREE_ITERATIVE, &tree);
    if (SR_ERR_OK != rc) {
        goto cleanup;
    }

    /* print the partially loaded tree content */
    //printf("\n\nPartially loaded tree on xpath: %s =\n", xpath);
    //sr_print_tree(tree, INT_MAX);

    /**
     * Traverse the entire tree.
     * Missing nodes will get loaded (in bulks) behind the scenes.
     */
    tree_traversal(sess, tree);

    /* print the fully loaded tree content */
    printf("\nFully loaded tree on xpath: %s =\n", xpath);
    sr_print_tree(tree, INT_MAX);
    printf("\n\n");


cleanup:
    if (NULL != tree) {
        sr_free_tree(tree);
    }
    if (NULL != sess) {
        sr_session_stop(sess);
    }
    if (NULL != conn) {
        sr_disconnect(conn);
       printf("disconnecting!!!\n");
    }
    return rc;
}
