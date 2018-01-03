#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

int main() {
/*   const char *path = "/proc/1/stat";
   const char delimiters[] = " ";
   FILE *proc_stat_fd = NULL;
   char *line = NULL,
         *token = NULL,
         *cp = NULL;
   int line_len = 0;

   proc_stat_fd = fopen(path, "r");
   printf("asdasd\n");

   if (NULL == proc_stat_fd) {
      printf("Failed to read stats at %s\n", path);
      return 1;
   }

   if (getline(&line, &line_len, proc_stat_fd) == -1) {
      printf("failed to get line\n");
      return 1;
   }



   cp = line;
   */
   //cp = strdup (line);                // Make writable copy.  */
   //free(line);
   /*
   token = strsep (&line, delimiters);
   for (int i = 0; token != NULL && i < 13; i++) {
      printf("parsed %s\n", token);
      token = strsep (&line, delimiters);
   }


   printf("line==NULL ? %d\n", line == NULL);
   free(cp);
   printf("proc_stat_fd==NULL ? %d\n", proc_stat_fd == NULL);
   fclose(proc_stat_fd);
   */

   const char delim[2] = " ";
   char *line = NULL,
         *token = NULL,
         *endptr = NULL;
   size_t line_len = 0;
   FILE *proc_stat_fd = NULL;
   const char *path = "/proc/1/stat";
   unsigned long utime = 0,
         stime = 0,
         diff_total_cpu = 0,
         new_total_cpu = 0,
         num = 0;

   proc_stat_fd = fopen(path, "r");

   if (NULL == proc_stat_fd) {
      printf("Failed to read stats of module with PID=%s\n", path);
      goto cleanup;
   }

   if (getline(&line, &line_len, proc_stat_fd) == -1) {
      goto cleanup;
   }

   token = strtok(line, delim);
   if (NULL == token) {
      goto cleanup;
   }

   for (int position = 1; NULL != token && position <= 23; position++) {
      switch (position) {
         case 14:
            // position of "user mode time" field in /proc/pid/stat file is 14
            utime = strtoul(token, &endptr, 10);
            if (endptr == token) {
               printf("Unable to get user mode time for module PID=%s\n", path);
               continue;
            }
            //module->last_cpu_umode = 100 *
            //                         ((double)(utime - module->last_cpu_umode) / (double)diff_total_cpu);
            //module->last_cpu_umode = utime;
            break;

         case 15:
            // position of "kernel mode time" field in /proc/pid/stat file is 15
            stime = strtoul(token, &endptr, 10);
            if (endptr == token) {
               printf("Unable to get kernel mode time for module PID=%s\n", path);
               continue;
            }
            //module->last_cpu_kmode = 100 *
            //                         ((double)(stime - module->last_cpu_kmode) / (double)diff_total_cpu);
            //module->last_cpu_kmode = stime;
            break;

         case 23:
            // position of "virtual memory size" field in /proc/pid/stat file is 23
            num = strtoul(token, &endptr, 10);
            if (endptr == token) {
               continue;
            }
            //module->mem_vms = num;
            break;
      }

      token = strtok(NULL, delim);
   }
   printf("%lu\n",num);
   printf("%lu\n",new_total_cpu);
   printf("%lu\n",diff_total_cpu);
   printf("%lu\n",stime);
   printf("%lu\n",utime);

cleanup:
   if (NULL != proc_stat_fd) {
      fclose(proc_stat_fd);
   }
   free(line);

   return 0;
}
