#include <time.h>

#include "utils.h"

 /*--BEGIN superglobal vars--*/
char verbose_msg[4096];
FILE *output_fd = NULL;
FILE *supervisor_debug_log_fd = NULL;
FILE *supervisor_log_fd = NULL;
FILE *statistics_fd = NULL;
FILE *inst_event_fd = NULL;
 /*--END superglobal vars--*/
 
 /*--BEGIN local #define--*/
 /*--END local #define--*/
 
 /*--BEGIN local typedef--*/
 /*--END local typedef--*/
 
 /* --BEGIN local vars-- */
 /* --END local vars-- */
 
 /* --BEGIN full fns prototypes-- */
static int vector_resize(vector_t *v, uint32_t size);
 /* --END full fns prototypes-- */
 
 /* --BEGIN superglobal fns-- */
char *get_formatted_time()
{
   time_t raw_time;
   static char buffer[28];
   struct tm* tm_info;

   time(&raw_time);
   tm_info = localtime(&raw_time);
   strftime(buffer, 28, "[%Y-%m-%d %H:%M:%S]", tm_info);

   return buffer;
}

void print_msg(int level, char *string)
{
   switch (level) {
      case STATISTICS:
         if (statistics_fd != NULL) {
            fprintf(statistics_fd, "%s", string);
            fflush(statistics_fd);
         }
         break;

      case MOD_EVNT:
         if (output_fd != NULL) {
            fprintf(output_fd, "[MOD_EV]%s", string);
            fflush(inst_event_fd);
         }
         break;

      case N_STDOUT:
         if (output_fd != NULL) {
            fprintf(output_fd, "%s", string);
            fflush(output_fd);
         }
         break;

      case DEBUG:
         if (output_fd != NULL) {
            fprintf(output_fd, "[DBG]%s", string);
            fflush(output_fd);
         } else if (stdout != NULL) {
            fprintf(stdout, "[DBG]%s", string);
            fflush(stdout);
         }
         break;

      case SUP_LOG:
         if (supervisor_log_fd != NULL) {
            fprintf(supervisor_log_fd, "%s", string);
            fflush(supervisor_log_fd);
         }
         break;

      case N_ERR:
         if (output_fd != NULL) {
            fprintf(output_fd, "[ERR]%s", string);
            fflush(output_fd);
         }
         break;

      case N_WARN:
         if (output_fd != NULL) {
            fprintf(output_fd, "[WARN]%s", string);
            fflush(output_fd);
         }
         break;

      case N_INFO:
      case V1:
         if (output_fd != NULL) {
            fprintf(output_fd, "[INFO]%s", string);
            fflush(output_fd);
         } else if (stdout != NULL) {
            fprintf(stdout, "[DBG]%s", string);
            fflush(stdout);
         }
         break;
      case V2:
      case V3:
         if (output_fd != NULL) {
            fprintf(output_fd, "[DBG]%s", string);
            fflush(output_fd);
         } else if (stdout != NULL) {
            fprintf(stdout, "[DBG]%s", string);
            fflush(stdout);
         }
         break;
   }
}

int vector_init(vector_t *v, uint32_t size)
{
   (v)->capacity = size;
   (v)->total = 0;
   (v)->items = calloc((v)->capacity, sizeof(void *));
   IF_NO_MEM_INT_ERR((v)->items)

   return 0;
}

int vector_add(vector_t *v, void *item)
{
   if (v->capacity == v->total) {
      int rc = vector_resize(v, (v->capacity == 0 ? 1 : v->capacity) * 2);
      if (rc != 0) {
         return rc;
      }
   }
   v->items[v->total++] = item;

   return 0;
}

void vector_set(vector_t *v, uint32_t index, void *item)
{
   v->items[index] = item;
}

void * vector_get(vector_t *v, uint32_t index)
{
   return v->items[index];
}

void vector_delete(vector_t *v, uint32_t index)
{
   memmove(v->items + index,
           v->items + (index + 1),
           (v->total - index - 1) * sizeof(void *));
   v->total--;
   /* // Save some memory?
    * if (v->total > 0 && v->total == v->capacity / 4) {
    *    vector_resize(v, v->capacity / 2);
    * }
    * */
}

void vector_free(vector_t *v)
{
   v->total = 0;
   v->capacity = 0;
   NULLP_TEST_AND_FREE(v->items)
}
/* --END superglobal fns-- */

/* --BEGIN local fns-- */
static int vector_resize(vector_t *v, uint32_t capacity)
{
   void **items = realloc(v->items, sizeof(void *) * capacity);
   IF_NO_MEM_INT_ERR(items)

   v->items = items;
   v->capacity = capacity;

   return 0;
}
/* --END local fns-- */
