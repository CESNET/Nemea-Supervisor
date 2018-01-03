#!/usr/bin/env python3

import libsysrepoPython3 as sr
import sys
from time import sleep

sleep(0.1) # wait for test to get into loop
conn = sr.Connection("asynch_stats.py")
sess = sr.Session(conn, sr.SR_DS_RUNNING)

action = sys.argv[1]

if action == "intable_module_stats":
    xpath_base = "/nemea-test-1:nemea-supervisor/module-group[name='Grp1']/module[name='Intable']/instance[name='Intable1']/stats/"
    try:
        s = sess.get_items(xpath_base + "*")
        if s.val_cnt() != 6:
            exit(1)
        if s.val(0).xpath() != xpath_base + "running":
            exit(22)
        if s.val(0).data().get_bool() != False:
            exit(2)

        if s.val(1).xpath() != xpath_base + "restart-counter":
            exit(33)
        if s.val(1).data().get_uint8() != 2:
            exit(3)

        if s.val(2).xpath() != xpath_base + "cpu-user":
            exit(44)
        if s.val(2).data().get_uint64() != 9999:
            exit(4)

        if s.val(3).xpath() != xpath_base + "cpu-kern":
            exit(55)
        if s.val(3).data().get_uint64() != 8888:
            exit(5)

        if s.val(4).xpath() != xpath_base + "mem-vms":
            exit(66)
        if s.val(4).data().get_uint64() != 7777:
            exit(6)

        if s.val(5).xpath() != xpath_base + "mem-rss":
            exit(77)
        if s.val(5).data().get_uint64() != 6666:
            exit(7)

    except Exception as e:
        print('asynch_stats.py error: ' +str(e))
        exit(250)
elif action == "intable_interfaces_stats":
    xpath_base = "/nemea-test-1:nemea-supervisor/module-group[name='Grp1']/module[name='Intable']/instance[name='Intable1']/interface/stats/"
    xpath_if1 = "/nemea-test-1:nemea-supervisor/module-group[name='Grp1']/module[name='Intable']/instance[name='Intable1']/interface[name='if1']/stats/"
    xpath_if2 = "/nemea-test-1:nemea-supervisor/module-group[name='Grp1']/module[name='Intable']/instance[name='Intable1']/interface[name='if2']/stats/"
    try:
    # tohle musi bejt!
        s = sess.get_items(xpath_base + '*')
        if s.val_cnt() != 6:
            exit(111)
        if s.val(0).xpath() != xpath_if1 + "sent-msg-cnt":
            print(s.val(0).xpath() + ' != ' + xpath_if1 + "sent-msg-cnt")
            exit(112)
        if s.val(0).data().get_uint64() != 11111:
            exit(113)

        if s.val(1).xpath() != xpath_if1 + "sent-buff-cnt":
            print(s.val(1).xpath() + ' != ' + xpath_if1 + "sent-buff-cnt")
            exit(114)
        if s.val(1).data().get_uint64() != 11112:
            exit(115)

        if s.val(2).xpath() != xpath_if1 + "dropped-msg-cnt":
            print(s.val(2).xpath() + ' != ' + xpath_if1 + "dropped-msg-cnt")
            exit(116)
        if s.val(2).data().get_uint64() != 11113:
            exit(117)

        if s.val(3).xpath() != xpath_if1 + "autoflush-cnt":
            print(s.val(3).xpath() + ' != ' + xpath_if1 + "autoflush-cnt")
            exit(118)
        if s.val(3).data().get_uint64() != 11114:
            exit(119)

        if s.val(4).xpath() != xpath_if2 + "recv-msg-cnt":
            print(s.val(4).xpath() + ' != ' + xpath_if1 + "recv-msg-cnt")
            exit(120)
        if s.val(4).data().get_uint64() != 12333:
            exit(121)

        if s.val(5).xpath() != xpath_if2 + "recv-buff-cnt":
            print(s.val(5).xpath() + ' != ' + xpath_if1 + "recv-buff-cnt")
            exit(122)
        if s.val(5).data().get_uint64() != 12334:
            exit(123)

    except Exception as e:
        print('asynch_stats.py error: ' +str(e))
        exit(251)
else:
    exit(252)

exit(0)
