#!/usr/bin/python
#Checks whether all modules in list below are running.

import os
import sys 
import json

#in this list specify all modules that you want to be running
check_modules = [ 
   "ipfixcol",
   "hoststatsnemea",
   "vportscan_detector",
   "bruteforce_detector",
   "sip_bf_detector",
   "link_traffic",
   "proto_traffic",
   "warden_hostats2idea",
   "warden_amplification2idea",
   "warden_ipblacklist2idea",
   "warden_vportscan2idea",
   "warden_bruteforce2idea",
   "reporter_leaktest",
   "warden_booterfilter2idea",
   "warden_sipbruteforce2idea",
   "warden_venom2idea"
]

with os.popen("supcli -i") as f:
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
