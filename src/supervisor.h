#ifndef SUP_H
#define SUP_H


#include <inttypes.h>
#include <sys/stat.h>

#include "config.h"
#include "module.h"
#include "utils.h"

 /*--BEGIN superglobal symbolic const--*/
 /*--END superglobal symbolic const--*/

 /*--BEGIN macros--*/
 /*--END macros--*/

 /*--BEGIN superglobal typedef--*/

 /*--END superglobal typedef--*/

 /*--BEGIN superglobal vars--*/
extern const char *ns_root_sr_path; ///< TODO
 /*--END superglobal vars--*/

 /*--BEGIN superglobal fn prototypes--*/
extern int init_paths();
extern int daemon_init_process();
extern int supervisor_initialization();
extern void supervisor_routine();
extern void terminate_supervisor(bool);
 /*--END superglobal fn prototypes--*/

#endif
