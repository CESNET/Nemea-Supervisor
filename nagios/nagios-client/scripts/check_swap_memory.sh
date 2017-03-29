#!/bin/bash
#Checks swap memory.
#By default if there is more than 20% used it returns 2 as critical.
#To change the percantage there is an optional argument.

if [ $# -gt 1 ]; then
   echo "Too many arguments."
   echo "Usage: `basename $0` [maxlimit]"
   exit 2
fi

TOTAL=$(cat /proc/meminfo | grep SwapTotal | tr -s ' ' | cut -d' ' -f2)
FREE=$(cat /proc/meminfo | grep SwapFree | tr -s ' ' | cut -d' ' -f2)

#checking for optional argument
if [ $# -eq 0 ]; then
   LIMIT=20
else
   if [ $1 -lt 1 -o $1 -gt 100 ]; then
      echo "Argument must be a number between 1-100."
      exit 2
   fi
   LIMIT="$1"
fi

#checking if there is swap memory enabled on system
if [ "$TOTAL" -eq 0 ]; then
  echo "Swap memory disabled."
  exit 1
fi

#counting percentage of swap used
PERCENT=$( echo "scale=2; 100-(($FREE/$TOTAL)*100)" | bc | cut -d'.' -f1)

if [ "$PERCENT" -lt "$LIMIT" ]; then
   echo "OK. Consuming $PERCENT% of swap."
   exit 0
else
   echo "Running out. Consuming $PERCENT% of swap, limit is $LIMIT%."
   exit 2
fi
