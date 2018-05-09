/**
 * @file conf.h
 * @brief Defines functions for loading configuration from sysrepo.
 */

#ifndef CONF_H
#define CONF_H

#include <sysrepo.h>

extern bool daemon_flag; ///< CLI startup option to tell whether to start as daemon (whether to fork)
extern char *logs_path; ///< Path to where logs directory should reside

/**
 * @brief Loads whole nemea supervisor config tree.
 * @details Load whole nemea supervisor config tree into supervisor structures
 * @param sess Sysrepo session to use
 * */
extern int ns_startup_config_load(sr_session_ctx_t *sess);


/**
 * @brief Loads module structure of given name from sysrepo.
 * @param sess Sysrepo session to use
 * @param module_name Name of module to load
 * @return sysrepo error code
 * */
extern int
av_module_load_by_name(sr_session_ctx_t *sess, const char *module_name);

/**
 * @brief Loads instance structure of given name from sysrepo.
 * @param sess Sysrepo session to use
 * @param inst_name Name of module to load
 * @return sysrepo error code
 * */
extern int
inst_load_by_name(sr_session_ctx_t *sess, const char *inst_name);

#endif
