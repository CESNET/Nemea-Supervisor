/**
 * @file conf.h
 * @brief Defines functions for loading configuration from sysrepo.
 */

#ifndef CONF_H
#define CONF_H

#include <sysrepo.h>

extern bool daemon_flag; ///< CLI startup option to tell whether to start as daemon
extern char *logs_path; ///< Path to where logs directory should reside

/**
 * @brief Loads whole nemea supervisor config tree.
 * @details Load whole nemea supervisor config tree into supervisor structures
 *  (module_group_t and module_t) and pushes them into appropriate linked lists.
 *  Parses nemea-supervisor container in YANG schema.
 * @param sess Sysrepo session to use
 *  configugration from sr_get_subtree function.
 * */
extern int ns_startup_config_load(sr_session_ctx_t *sess);


/**
 * @brief Loads module structure of given name from sysrepo.
 * @details It's a wrapper around
 *  load_module(sr_node_t *module_node, module_group_t *group) function.
 * @see load_module()
 * @param sess Sysrepo session to use
 * @param group_name Name of group to which module belongs to
 * @param module_name Name of module to load
 * */
extern int
av_module_load_by_name(sr_session_ctx_t *sess, const char *module_name);

/**
 * TODO
 * */
extern int
inst_load_by_name(sr_session_ctx_t *sess, const char *inst_name);

#endif
