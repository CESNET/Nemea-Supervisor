import libsysrepoPython3 as sr
import sys

conn = sr.Connection("get_subtree.py")
sess = sr.Session(conn, sr.SR_DS_STARTUP)
invalid_xpath = "/nemea-test-1:nemea-supervisor/module-group[name='Detectors']/module[name['m1']"
values = sess.get_subtree(invalid_xpath)

