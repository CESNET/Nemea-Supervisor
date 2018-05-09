/**
 * @file stats.h
 * @brief Defines callbacks for requests to stats from sysrepo.
 */

#ifndef STATS_H
#define STATS_H

#include <sysrepo.h>

/**
 * @brief Callback that should be subscribed at /nemea:supervisor/instance/interface/stats to provide stats about interfaces.
 * @param xpath Received XPATH of interface
 * @param[out] values Array of returned values
 * @param[out] values_cnt Size of returned array
 * @param private_ctx unused
 * @return sysrepo error code
 * */
extern int interface_get_stats_cb(const char *xpath,
                                  sr_val_t **values,
                                  size_t *values_cnt,
                                  void *private_ctx);

/**
 * @brief Callback that should be subscribed at /nemea:supervisor/instance/stats to provide stats about instances.
 * @param xpath Received XPATH of instance
 * @param[out] values Array of returned values
 * @param[out] values_cnt Size of returned array
 * @param private_ctx unused
 * @return sysrepo error code
 * */
extern int inst_get_stats_cb(const char *xpath,
                             sr_val_t **values,
                             size_t *values_cnt,
                             void *private_ctx);
#endif
