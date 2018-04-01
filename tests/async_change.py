#!/usr/bin/env python3

import libsysrepoPython3 as sr
import sys
from time import sleep

sleep(0.1) # wait for test to get into loop
conn = sr.Connection("asynch_change.py")
sess = sr.Session(conn, sr.SR_DS_RUNNING)

action = sys.argv[1]

if action == "create_group_with_modules_and_instances":
    sess.set_item("/nemea-test-1:supervisor/instance[name='m1']/path", sr.Val("/m1path"))
    sess.set_item("/nemea-test-1:supervisor/instance[name='m2']/path", sr.Val("/m2path"))
    sess.set_item("/nemea-test-1:supervisor/instance[name='m2']/instance[name='i1']/enabled", sr.Val(True))
    sess.commit()
##################
elif action == "create_available_module":
    sess.set_item("/nemea-test-1:supervisor/available-module[name='m1']/path", sr.Val("/m1path"))
    sess.set_item("/nemea-test-1:supervisor/available-module[name='m1']/description", sr.Val("lorem ipsum"))
    sess.set_item("/nemea-test-1:supervisor/available-module[name='m1']/is-nemea-module", sr.Val(True))
    sess.set_item("/nemea-test-1:supervisor/available-module[name='m1']/is-sysrepo-ready", sr.Val(False))
    sess.set_item("/nemea-test-1:supervisor/available-module[name='m1']/sysrepo-callbacks-ready", sr.Val(True))
    sess.commit()
##################
elif action == "delete_group_with_module_and_instance":
    sess.delete_item("/nemea-test-1:supervisor/[name='To remove']")
    sess.commit()
##################
elif action == "delete_available_module":
    sess.delete_item("/nemea-test-1:supervisor/instance[name='m3']")
    sess.delete_item("/nemea-test-1:supervisor/available-module[name='module B']")
    sess.commit()
##################
elif action == "group_modified_1":
    sess.set_item("/nemea-test-1:supervisor/[name='To remove']/enabled", sr.Val(False))
    sess.commit()
##################
elif action == "available_module_modified_1":
    sess.set_item("/nemea-test-1:supervisor/available-module[name='module B']/path", sr.Val("/a/b/cc"))
    sess.commit()
##################
elif action == "instance_modified_1":
    sess.set_item("/nemea-test-1:supervisor/instance[name='m4']/enabled", sr.Val(False))
    sess.set_item_str("/nemea-test-1:supervisor/instance[name='m4']/interface[name='tcp-in-4-1']/direction", "IN")
    sess.set_item_str("/nemea-test-1:supervisor/instance[name='m4']/interface[name='tcp-in-4-1']/type", "UNIXSOCKET")
    sess.set_item("/nemea-test-1:supervisor/instance[name='m4']/interface[name='tcp-in-4-1']/unix-params/socket-name", sr.Val("sock1"))
    sess.commit()
##################


exit(0)

def module_change_cb(sess, module_name, event, private_ctx):
    with open('python-cb-file', 'a') as the_file:
        the_file.write('changed\n')
    return sr.SR_ERR_OK

conn = sr.Connection("example_application")
sess = sr.Session(conn, sr.SR_DS_STARTUP)
subscribe = sr.Subscribe(sess)
subscribe.module_change_subscribe("nemea-test-1", module_change_cb, None, 0, sr.SR_SUBSCR_DEFAULT | sr.SR_SUBSCR_APPLY_ONLY)
sr.global_loop()
