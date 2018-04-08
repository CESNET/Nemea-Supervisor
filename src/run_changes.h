/**
 * @file run_changes.h
 * @brief Defines callback function for changes in running sysrepo datastore.
 */

#ifndef RUN_CHANGES_H
#define RUN_CHANGES_H


#include <sysrepo.h>

/**
 * @brief Callback function subscribed to changes of subtree beginning
 *  at module level.
 * @details Though it acts simiralily as module_group_config_change_cb(), I
 *  recommend you not to merge those two. Separation is done for sake of readibility.
 * @param sess Used sysrepo sessing
 * @param smn Name of sysrepo module that was changed
 * @param evnt Type of sysrepo event for this change. This function is cabable of
 *  parsing only SR_EV_APPLY.
 * @param priv_ctx Context pointer passed when the subscription was made.
 * @see module_group_config_change_cb()
 * @return Sysrepo error code of sr_error_t enum.
 * */
extern int run_config_change_cb(sr_session_ctx_t *sess, const char *smn,
                                sr_notif_event_t evnt, void *priv_ctx);
#endif
