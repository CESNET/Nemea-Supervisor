#include <time.h>

#include "utils.h"

char verbose_msg[4096];
FILE *output_fd = NULL;
FILE *supervisor_log_fd = NULL;
uint8_t verbosity_level = 0;

static int vector_resize(vector_t *v, uint32_t capacity);

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
      case N_ERR:
         if (output_fd != NULL) {
            fprintf(output_fd, "[ERR]%s", string);
            fflush(output_fd);
         } else if (stderr != NULL) {
            fprintf(stderr, "[ERR]%s", string);
            fflush(stderr);
         }
         break;
      case V1:
         if (verbosity_level >= V1) {
            if (output_fd != NULL) {
               fprintf(output_fd, "[INFO]%s", string);
               fflush(output_fd);
            } else if (stdout != NULL) {
               fprintf(stdout, "[INFO]%s", string);
               fflush(stdout);
            }
         }
         break;
      case V2:
         if (verbosity_level >= V2) {
            if (output_fd != NULL) {
               fprintf(output_fd, "[ERR]%s", string);
               fflush(output_fd);
            } else if (stderr != NULL) {
               fprintf(stderr, "[ERR]%s", string);
               fflush(stderr);
            }
         }
         break;
      case V3:
         if (verbosity_level >= V3) {
            if (output_fd != NULL) {
               fprintf(output_fd, "[DBG]%s", string);
               fflush(output_fd);
            } else if (stdout != NULL) {
               fprintf(stdout, "[DBG]%s", string);
               fflush(stdout);
            }
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

void vector_delete(vector_t *v, uint32_t index)
{
   memmove(v->items + index,
           v->items + (index + 1),
           (v->total - index - 1) * sizeof(void *));
   v->total--;
}

void vector_free(vector_t *v)
{
   v->total = 0;
   v->capacity = 0;
   NULLP_TEST_AND_FREE(v->items)
}
static int vector_resize(vector_t *v, uint32_t capacity)
{
   void **items = realloc(v->items, sizeof(void *) * capacity);
   IF_NO_MEM_INT_ERR(items)

   v->items = items;
   v->capacity = capacity;

   return 0;
}