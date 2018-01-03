#ifndef SYSREPO_FNS_H
#define SYSREPO_FNS_H

#include <stdio.h>
#include <sysrepo.h>
#include <sysrepo/values.h>

/*--BEGIN superglobal symbolic const--*/
/*--END superglobal symbolic const--*/

/*--BEGIN macros--*/
/*--END macros--*/

/*--BEGIN superglobal typedef--*/
/*--END superglobal typedef--*/

/*--BEGIN superglobal vars--*/
// denote with extern
//extern sr_conn_ctx_t *sr_con = NULL;
//extern sr_session_ctx_t *sr_sess = NULL;
/*--END superglobal vars--*/

/*--BEGIN superglobal fn prototypes--*/
int sr_reload_conf();
int sr_subscr_states();
void disconnect_sysrepo();
/*--END superglobal fn prototypes--*/

#endif
