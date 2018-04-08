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

sr_session_ctx_t *sess = NULL;

typedef enum {
   T_ROOT, // nemea-supervisor root node
   T_MODULE_GRP_R, // module-group root nodes
   T_MODULE_GRP_N, // module-group nodes: name, enabled, modules_ll
   T_MODULE_R, // module root node
   T_MODULE_N, // module nodes: name, path, enabled, params, module-restarts
} sr_tree_lvl;

int ns_startup_config_load(sr_session_ctx_t *sess)
{
   // Root level
   if (NULL == node->first_child) {
      return;
   }

   // Descend from root to module groups root level
   node = node->first_child;

   // Module group roots level
   do {
      if (NULL != node->first_child) {
         module_group_load(NULL, NULL, node->first_child);
      }
      node = node->next; // move to next sibling
   } while (NULL != node);
}

int module_group_load(sr_session_ctx_t *sess, struct module_group_s *group,
                      sr_node_t *group)
{
   // TODO allocate group
   do {
      if (0 == strcmp(group->name, "name")) {
         printf("Group=%s\n",group->data.string_val);
         //cur_group->name = strdup(child->data.string_val);
      } else if (0 == strcmp(group->name, "enabled")) {
         printf(" enabled=%d\n",group->data.bool_val);
         //cur_group->enabled = child->data.bool_val ? true : false;
      } else if (0 == strcmp(group->name, "module")) {
         // child = one of module roots in the group
         if (NULL != group->first_child) {
            load_module(group->first_child);
            printf("next child is %s\n", group->first_child->name);
         }
      }

      // move to next group element
      group = group->next;
   } while (NULL != group);
}
void load_module(sr_node_t *module)
{
   printf("at the module root");
   // allocate module
   printf(" module=...\n");

   do {
      if (0 == strcmp(module->name, "name")) {
         printf(" Module=%s\n", module->data.string_val);
      } else if (0 == strcmp(module->name, "path")) {
         printf("  path=%s\n", module->data.string_val);
      } else if (0 == strcmp(module->name, "enabled")) {
         printf("  enabled=%d\n", module->data.bool_val);
      } else if (0 == strcmp(module->name, "params")) {
         printf("  path=%s\n", module->data.string_val);
      } else if (0 == strcmp(module->name, "module-restarts")) {
         printf("  module-restarts=%d\n", module->data.uint32_val);
      } else if (0 == strcmp(module->name, "iface")) {
         //printf("  path=%s\n", node->data.string_val);
         // TODO dig down for interfaces
      }

      // move to next module element
      module = module->next;
   } while (NULL != module);
}
/*
void
tree_traversal_recursive(sr_node_t *node, size_t *node_cnt, sr_tree_lvl level)
{
    *node_cnt += 1;

    if (NULL == node) {
       printf("found end\n");
       return;
    }

   printf("Visited node=%s,node_cnt=%d,level=%d.\n", node->name, *node_cnt,level);

   switch (level) {
      case T_ROOT:
         tree_traversal_recursive(node->first_child, node_cnt, T_MODULE_GRP_R);
         break;
      case T_MODULE_GRP_R:
         do {
            tree_traversal_recursive(node->first_child, node_cnt, T_MODULE_GRP_N);
            node = node->next; // move to next sibling
         } while (NULL != node);
         break;

      case T_MODULE_GRP_N:
         // allocate group
         do {
            if (0 == strcmp(node->name, "name")) {
               printf("Group=%s\n",node->data.string_val);
               //cur_group->name = strdup(child->data.string_val);
            } else if (0 == strcmp(node->name, "enabled")) {
               printf(" enabled=%d\n",node->data.bool_val);
               //cur_group->enabled = child->data.bool_val ? true : false;
            } else if (0 == strcmp(node->name, "module")) {
               // child = one of module roots in the group
               tree_traversal_recursive(node->first_child, node_cnt, T_MODULE_N);
            }
            node = node->next; // move to next sibling
         } while (NULL != node);
         break;

      case T_MODULE_N:
         printf("at the module root");
         // allocate module
         do {
            if (0 == strcmp(node->name, "name")) {
               printf(" Module=%s\n", node->data.string_val);
            } else if (0 == strcmp(node->name, "path")) {
               printf("  path=%s\n", node->data.string_val);
            } else if (0 == strcmp(node->name, "enabled")) {
               printf("  enabled=%d\n", node->data.bool_val);
            } else if (0 == strcmp(node->name, "params")) {
               printf("  path=%s\n", node->data.string_val);
            } else if (0 == strcmp(node->name, "module-restarts")) {
               printf("  module-restarts=%d\n", node->data.uint32_val);
            } else if (0 == strcmp(node->name, "iface")) {
               //printf("  path=%s\n", node->data.string_val);
               // TODO dig down
            }

            node = node->next; // move to next sibling
         } while (NULL != node);
               printf(" module=...\n");

         break;
   }
}

void
tree_traversal(sr_node_t *tree)
{
    size_t node_cnt = 0;
    printf("\nStarting tree traversal...\n");
    tree_traversal_recursive(tree, &node_cnt, T_ROOT);
    printf("Tree traversal finished, visited %lu nodes.\n", node_cnt);
}*/

int
main(int argc, char **argv)
{
    sr_conn_ctx_t *conn = NULL;
    const char *xpath = NULL;
    sr_node_t *tree = NULL;
    int rc = SR_ERR_OK;

    /* turn on debug logging to stderr - to see what's happening behind the scenes */
    //sr_log_stderr(SR_LL_DBG);

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
    rc = sr_get_subtree(sess, xpath, SR_GET_SUBTREE_DEFAULT, &tree);
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
    //tree_traversal(tree);
   ns_startup_config_load(NULL);

    /* print the fully loaded tree content */
    printf("\nFully loaded tree on xpath: %s =\n", xpath);
    //sr_print_tree(tree, INT_MAX);
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
