#!/usr/bin/python3
#Checks whether all modules in list below are running.

import os
import sys
import json

#importing modules configured in /etc/nemea/check_modules_list
check_modules = []
with open("/etc/nemea/check_modules_list", "r") as conf:
   for line in conf:
      line = line.rstrip()
      if not line:
         continue
      elif line[0] != '#':
         check_modules.append(line)
   conf.close()

with os.popen("supcli -i 2>/dev/null") as f:
   data = f.read()
   if not data:
      print("Could not read from supervisor.")
      sys.exit(2)
   f.close()

errors = []
jd = json.loads(data)

#checking for occurence of given lists in output of "supcli -i"
for module in check_modules:
   if module not in jd or (module in jd and jd[module]["status"] != "running"):
      errors.append(module)

if not errors:
   print("All monitored modules are running.")
   sys.exit(0)
else:
   print("Stopped modules:"+",".join(errors))
   sys.exit(2)
