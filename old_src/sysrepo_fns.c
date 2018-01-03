
#include <config.h>

#include "sysrepo_fns.h"
#include "internal.h"
//include "supervisor.h"

/* #include <systemlib.h> */

/*--BEGIN superglobal vars--*/
//without extern
/*--END superglobal vars--*/

/*--BEGIN local #define--*/
#define SR_PROG_ID "NEMEA_SUPERVISOR"
/*--END local #define--*/

/*--BEGIN local typedef--*/
/*--END local typedef--*/

/* --BEGIN local vars-- */
static sr_conn_ctx_t *conn = NULL;
static sr_session_ctx_t *sess = NULL;
static sr_subscription_ctx_t *sbscr = NULL;
/* --END local vars-- */

/* --BEGIN full fns prototypes-- */
//TDD asi netreba, kdyz tu neni main
//int sr_reload_conf();
//int sr_subscr_states();
//int reconnect_sysrepo();
//void disconnect_sysrepo();
//static sr_dp_get_items_cb get_state();
static int get_state_cb(const char *xpath, sr_val_t **vals, size_t *vals_cnt, void *private_ctx);
/* --END full fns prototypes-- */

/* --BEGIN superglobal fns-- */
//TDD
int sr_reload_conf()
{
   return 0;
}
//TDD
int sr_subscr_states()
{
   const char *sub_path = "/nemea:mod_info_full";
   int rc;

   rc = sr_dp_get_items_subscribe(sess, sub_path, get_state_cb, NULL, SR_SUBSCR_DEFAULT, &sbscr);
   if (rc != SR_ERR_OK) {
      VERBOSE(N_STDOUT, "%s [ERROR] Failed to connect to Sysrepo: %s\n", 
              get_formatted_time(), sr_strerror(rc));
      goto error_label;
   }

   return 0;

error_label:
   if (sbscr != NULL) {
      sr_unsubscribe(sess, sbscr);
   }
   return rc;
}
//TDD
int reconnect_sysrepo() 
{
   int rc;

   if (conn == NULL) {
      if ((rc = sr_connect(SR_PROG_ID, SR_CONN_DEFAULT, &conn)) != SR_ERR_OK) {
         VERBOSE(N_STDOUT, "%s [ERROR] Failed to connect to Sysrepo: %s\n", 
                 get_formatted_time(), sr_strerror(rc));
         return rc;
      }
   }

   if (sess == NULL) {
      if ((rc = sr_session_start(conn, SR_DS_STARTUP, SR_SESS_DEFAULT, &sess)) != SR_ERR_OK) {
         VERBOSE(N_STDOUT, "%s [ERROR] Failed to set Sysrepo session: %s\n", 
                get_formatted_time(), sr_strerror(rc));
         return rc;
      }
   }
   return 0;
}
//TDD
void disconnect_sysrepo() 
{
   if (sbscr != NULL) {
      sr_unsubscribe(sess, sbscr);
   }
   if (sess != NULL) {
      sr_session_stop(sess);
   }
   if (conn != NULL) {
      sr_disconnect(conn);
   }
}
/* --END superglobal fns-- */

/* --BEGIN local fns-- */
static int get_state_cb(const char *xpath, sr_val_t **vals, size_t *vals_cnt, void *private_ctx)
{
   int rc;

   rc = sr_new_values(1, vals);
   if (rc != SR_ERR_OK) {
       return rc;
   }
   
   rc = sr_val_set_str_data(&(*vals)[0], SR_STRING_T, "aksdjhfkashdfkjahds");
   if (rc != SR_ERR_OK) {
       return rc;
   }
   
   *vals_cnt = 1;
   
   return 0;
}
/* --END local fns-- */
