#!/usr/bin/env python
# Needs Python 3.x or Python 2.6+
import random
import time
import subprocess
import json

from flask import Flask, request, render_template, g, jsonify

app = Flask(__name__)

app.jinja_env.trim_blocks = True
app.jinja_env.lstrip_blocks = True


SUP_PATH = '/usr/bin/nemea/supervisor_cli'
SUP_SOCK_PATH = '/var/run/nemea-supervisor/nemea-supervisor.sock'


# TODO: detection of topology change (in Javascript, ifcs returned from _stats don't match the ones loaded -> tell user to reload page)
# TODO: links to other machines in page header according to configuration (i.e. add config file) 

#####

# Backport of subprocess.check_output to Python 2.6
# Taken from http://stackoverflow.com/questions/28904750/python-check-output-workaround-in-2-6
if "check_output" not in dir( subprocess ): # duck punch it in!
    def check_output(*popenargs, **kwargs):
        r"""Run command with arguments and return its output as a byte string.

        Backported from Python 2.7 as it's implemented as pure python on stdlib.

        >>> check_output(['/usr/bin/python', '--version'])
        Python 2.6.2
        """
        process = subprocess.Popen(stdout=subprocess.PIPE, *popenargs, **kwargs)
        output, unused_err = process.communicate()
        retcode = process.poll()
        if retcode:
            cmd = kwargs.get("args")
            if cmd is None:
                cmd = popenargs[0]
            error = subprocess.CalledProcessError(retcode, cmd)
            error.output = output
            raise error
        return output
    subprocess.check_output = check_output


###############
# Topology

def get_topology():
    cmd_and_args = [SUP_PATH, "-s", SUP_SOCK_PATH, "-i"]
    #print(' '.join(cmd_and_args))
    try:
        out = subprocess.check_output(cmd_and_args, stderr=subprocess.STDOUT)
        if isinstance(out, bytes):
            out = out.decode() # in Py3, output of check_output is of type bytes
        #print(out)
    except subprocess.CalledProcessError as e:
        return {
            'error': 'callerror',
            'cmd': ' '.join(cmd_and_args),
            'code': e.returncode,
            'output': e.output,
        }
    try:
        return sorted(json.loads(out)['modules'], key=lambda mod: mod.get('module-idx',0))
    except ValueError:
        return {
            'error': 'invalidjson',
            'cmd': ' '.join(cmd_and_args),
            'output': out,
        }


###############
# Counter stats

def get_indxed_key(d, prefix):
    i = 0
    while prefix + str(i) in d:
        i += 1
    return prefix + str(i)

def get_stats():
    cmd_and_args = [SUP_PATH, "-s", SUP_SOCK_PATH, "-x"]
    #print(' '.join(cmd_and_args))
    try:
        out = subprocess.check_output(cmd_and_args, stderr=subprocess.STDOUT)
        if isinstance(out, bytes):
            out = out.decode() # in Py3, output of check_output is of type bytes
        #print(out)
    except subprocess.CalledProcessError as e:
        return {
            'error': 'callerror',
            'cmd': ' '.join(cmd_and_args),
            'code': e.returncode,
            'output': e.output,
        }
    # Parse output
    try:
        res = {}
        for l in out.splitlines():
            module, type, rest = l.split(',', 2)
            if type == 'in':
                iftype, ifid, cnt_msg, cnt_buf = rest.split(',')
                cnt_msg = int(cnt_msg)
                cnt_buf = int(cnt_buf)
                res[get_indxed_key(res, module+'_'+type)] = cnt_msg
            elif type == 'out':
                iftype, ifid, cnt_msg, cnt_buf, cnt_drop, cnt_autoflush = rest.split(',')
                cnt_msg = int(cnt_msg)
                cnt_buf = int(cnt_buf)
                cnt_drop = int(cnt_drop)
                cnt_autoflush = int(cnt_autoflush)
                res[get_indxed_key(res, module+'_'+type)] = cnt_msg
                res[get_indxed_key(res, module+'_'+type) + '_drop'] = cnt_drop
            elif type == 'cpu':
                cpu_kernel, cpu_user = rest.split(',')
                cpu_kernel = int(cpu_kernel)
                cpu_user = int(cpu_user)
                res[module + '_cpu'] = cpu_kernel + cpu_user
            elif type == 'mem':
                mem = int(rest)
                res[module + '_mem'] = mem
        return res
    except Exception:
        raise
        return {
            'error': 'format',
            'cmd': ' '.join(cmd_and_args),
            'output': out,
        }

        

# ***** Main page *****
@app.route('/')
def main():
    topology = get_topology()
    return render_template('nemea_status.html', topology=topology)


# ***** Get statistics via AJAX request *****
@app.route('/_stats')
def events():
    stats = get_stats()
    #time.sleep(1)
    return jsonify(stats=stats)


if __name__ == "__main__":
    app.run(host='0.0.0.0',debug=True)

