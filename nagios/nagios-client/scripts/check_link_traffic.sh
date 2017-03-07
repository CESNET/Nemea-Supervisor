#!/bin/bash
#script checks whether all links are transferring data


pf="/tmp/`basename $0`-prevval"

#getting current output of munin_link_traffic script
curval="`nc -U /var/run/libtrap/munin_link_traffic | awk 'NR>=2' | tr ',' ' '`"
curtime="`date +%s`"

#checking if there is any previous value and creating one if there was not
if [ -e "$pf" ]; then
  prevval=`cat "$pf" | head -2`
else
  prevval=`echo -e "$curval\n$curtime"`
fi

prevtime="`echo "$prevval" | awk 'FNR==2'`"
prevval="`echo "$prevval" | awk 'FNR==1'`"
echo -e "$curval\n$curtime" > "$pf"

if [ "$curtime" -ne "$prevtime" ]; then
#comparing values in curval and prev val
#if they are the same shell exits with critical 2 (one link is down)
   counter=0
   for i in `echo "$curval"`; do
      counter=$(($counter+1))
      #hotfix for skipping telia links
      if [ "$counter" -eq 13 ]; then
         counter=$(($counter+6))
      fi
      previous=`echo "$prevval" | cut -d' ' -f"$counter"`
      if [ "$i" -eq $previous ]; then
         echo "Link $(($counter-1)) is DOWN."
         exit 2
      fi
   done
else
   echo "All links are UP."
   exit 0
fi
