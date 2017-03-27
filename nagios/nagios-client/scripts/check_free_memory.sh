#!/bin/bash
#Checks free memory.
#By default if there remains less than 10% of free memory it returns 2 as critical.
#To change percentage there is an optional argument.

if [ $# -gt 1 ]; then
   echo "Too many arguments."
   echo "Usage: `basename $0` [remaining]"
   exit 3
fi

TOTAL=$(cat /proc/meminfo | grep MemTotal | tr -s ' ' | cut -d' ' -f2)
FREE=$(cat /proc/meminfo | grep MemAvailable | tr -s ' ' | cut -d' ' -f2)

#checking for optional argument
if [ $# -eq 0 ]; then
   LIMIT=90
else
   if [ $1 -lt 1 -o $1 -gt 100 ]; then
      echo "Argument must be a number between 1-100."
      exit 3
   fi  
   LIMIT="$1"
fi

#PERCENT of total memory used
PERCENT=$( echo "scale=2; (($TOTAL-$FREE)/$TOTAL)*100" | bc | cut -d'.' -f1)

if [ "$PERCENT" -lt "$LIMIT" ]; then
   echo "OK. Consuming $PERCENT% of memory."
   exit 0
fi
echo "Running out. Consuming $PERCENT% of memory, limit is $1%."
exit 2
