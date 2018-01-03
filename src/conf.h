/**
 * @file conf.h
 * @brief Defines functions for loading configuration from sysrepo.
 */

#ifndef CONF_H
#define CONF_H

#include <sysrepo.h>

 /*--BEGIN superglobal symbolic const--*/
 /*--END superglobal symbolic const--*/

 /*--BEGIN macros--*/
 /*--END macros--*/

 /*--BEGIN superglobal typedef--*/
 /*--END superglobal typedef--*/

 /*--BEGIN superglobal vars--*/
 /*--END superglobal vars--*/

 /*--BEGIN superglobal fn prototypes--*/

/**
 * @brief Loads whole nemea supervisor config tree.
 * @details Load whole nemea supervisor config tree into supervisor structures
 *  (module_group_t and module_t) and pushes them into appropriate linked lists.
 *  Parses nemea-supervisor container in YANG schema.
 * @param sess Sysrepo session to use
 *  configugration from sr_get_subtree function.
 * */
extern int ns_config_load(sr_session_ctx_t *sess);

/**
 * @brief Loads module group structure of given name from sysrepo.
 * @details It's a wrapper around load_module_group(sr_node_t *group_node) function.
 * @see load_module_group()
 * @param sess Sysrepo session to use
 * @param name Name of module group to load
 * */
extern int module_group_load_by_name(sr_session_ctx_t *sess, const char *name);

/**
 * @brief Loads module structure of given name from sysrepo.
 * @details It's a wrapper around
 *  load_module(sr_node_t *module_node, module_group_t *group) function.
 * @see load_module()
 * @param sess Sysrepo session to use
 * @param group_name Name of group to which module belongs to
 * @param module_name Name of module to load
 * */
extern int module_load_by_name(sr_session_ctx_t *sess, const char *group_name,
                               const char *module_name);

/**
 * TODO
 * */
int instance_load_by_name(sr_session_ctx_t *sess,
                          const char *group_name,
                          const char *module_name,
                          const char *inst_name);
 /*--END superglobal fn prototypes--*/

#endif
