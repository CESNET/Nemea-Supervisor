#!/usr/bin/env python3

__author__ = "Mislav Novakovic <mislav.novakovic@sartura.hr>"
__copyright__ = "Copyright 2016, Deutsche Telekom AG"
__license__ = "Apache 2.0"

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import libsysrepoPython3 as sr
import sys

# Function to print current configuration state.
# It does so by loading all the items of a session and printing them out.
def print_current_config(session, module_name):
    select_xpath = "/" + module_name + ":*//*"

    values = session.get_items(select_xpath)

    for i in range(values.val_cnt()):
        print(values.val(i).to_string(), end=" ")

# Function to be called for subscribed client of given session whenever configuration changes.
def module_change_cb(sess, module_name, event, private_ctx):
    print("\n\n ========== CONFIG HAS CHANGED, CURRENT RUNNING CONFIG: ==========\n")

    print_current_config(sess, module_name)

    return sr.SR_ERR_OK

# Notable difference between c implementation is using exception mechanism for open handling unexpected events.
# Here it is useful because `Conenction`, `Session` and `Subscribe` could throw an exception.
try:
    module_name = "nemea"
    # connect to sysrepo
    conn = sr.Connection("nemea_supervisor_python3_test")
    print('connected')

    # start session
    sess = sr.Session(conn)
    print('session created')

    # subscribe for changes in running config */
    subscribe = sr.Subscribe(sess)
    print('subscribed')

    subscribe.module_change_subscribe(module_name, module_change_cb, None, 0, sr.SR_SUBSCR_DEFAULT)
    #subscribe.module_change_subscribe(mod_name, module_change_cb, None, 0, sr.SR_SUBSCR_DEFAULT | sr.SR_SUBSCR_APPLY_ONLY)

    print("\n\n ========== READING STARTUP CONFIG: ==========\n")
    try:
        print_current_config(sess, module_name)
    except Exception as e:
        print (e)

    print("\n\n ========== STARTUP CONFIG APPLIED AS RUNNING ==========\n")

    sr.global_loop()

    print("Application exit requested, exiting.\n")

except Exception as e:
    print ("EXCEPTION: %s" % e)
