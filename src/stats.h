/**
 * @file stats.h
 * @brief Defines callbacks for requests to stats from sysrepo.
 */

#ifndef STATS_H
#define STATS_H

#include <sysrepo.h>

/*--BEGIN superglobal symbolic const--*/
/*--END superglobal symbolic const--*/

/*--BEGIN macros--*/
/*--END macros--*/

/*--BEGIN superglobal typedef--*/
/*--END superglobal typedef--*/

/*--BEGIN superglobal vars--*/
// denote with extern
//extern sr_conn_ctx_t *sr_con = NULL;
/*--END superglobal vars--*/

/*--BEGIN superglobal fn prototypes--*/

extern int interface_get_stats_cb(const char *xpath,
                                  sr_val_t **values,
                                  size_t *values_cnt,
                                  void *private_ctx);

extern int inst_get_stats_cb(const char *xpath,
                             sr_val_t **values,
                             size_t *values_cnt,
                             void *private_ctx);

/*--END superglobal fn prototypes--*/

#endif
