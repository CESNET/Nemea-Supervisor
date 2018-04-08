/**
 * @file stats.h
 * @brief Defines callbacks for requests to stats from sysrepo.
 */

#ifndef STATS_H
#define STATS_H

#include <sysrepo.h>

extern int interface_get_stats_cb(const char *xpath,
                                  sr_val_t **values,
                                  size_t *values_cnt,
                                  void *private_ctx);

extern int inst_get_stats_cb(const char *xpath,
                             sr_val_t **values,
                             size_t *values_cnt,
                             void *private_ctx);
#endif
