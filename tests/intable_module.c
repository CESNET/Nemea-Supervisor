/**
 * @file intable_module.c
 * @brief Fake Nemea module that is interuptable by SIGINT. Used for testing.
 */

#include <unistd.h>

int main()
{
  while(1) {
    usleep(5000);
  }
}
