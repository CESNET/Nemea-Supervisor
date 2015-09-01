Supervisor:
===========

This module allows user to configure and monitor Nemea modules. User specifies modules
in xml configuration file, which is input for the Supervisor.

Example configuration of one module called flowcounter:

```
<module>
  <enabled>false</enabled>
  <params></params>
  <name>flowcounter</name>
  <path>../modules/flowcounter/flowcounter</path>
  <trapinterfaces>
    <interface>
      <note></note>
      <type>TCP</type>
      <direction>IN</direction>
      <params>localhost,8004</params>
    </interface>
  </trapinterfaces>
</module>
```

Every module contains unique name, path to binary file, parameters, enabled flag
and trap interfaces. Enabled flag tells Supervisor to run or not to run module after
startup. Every trap interface is specified by type (tcp, unixsocket or service),
direction (in or out), parameters (output interface: address + port; input
interface: port + number of clients) and optional note.

The configuration file has been designed according to the data model of the Supervisor.
Data model tree:

```
module: nemea
   +--rw nemea-supervisor
      +--rw modules
      +--rw name? string
      +--rw module* [name]
         +--rw name string
         +--rw params? string
         +--rw enabled boolean
         +--ro running? boolean
         +--rw path string
         +--rw trapinterfaces
            +--rw interface* [type direction params]
               +--rw note? string
               +--rw type trapifc-type
               +--rw direction trapifc-direction
               +--rw params string
```


Supervisor can run in one of two basic modes:
1) Interactive Mode:

```
./supervisor -f config_file.xml
```

2) Daemon Mode:
  Supervisor is executed with `--daemon` and runs as a process in backround
  of the operating system.

```
./supervisor -f config_file.xml --daemon
```

  The activity of the running daemon can be controlled by a special thin client:

```
./supervisor_cli
```

  Both daemon and client supports `-s` to specify path to the UNIX socket
  that is used for communication between daemon and client.

  Note: paths must match to communicate with right Supervisor daemon


Supervisor arguments:
 -f config\_file.xml   xml file with initial configuration
 -L logs\_path         optional path to Supervisor logs directory
                      (default /home/$user/supervisor_logs/)
 -s socket\_path       optional path to unix socket used for communication
                      with supervisor client (default /tmp/supervisor_daemon.sock)
 -v                   optional verbose flag used for enabling printing stats
                      about messages and CPU usage to "supervisor_log_statistics"
                      file in logs directory (more about these stats down below)
 --daemon             flag used for running Supervisor as a process in background
 -h                   program prints help and terminates

User can do various operations with modules via Supervisor. After launch appears
menu with available operations:

Options
-------

1. START ALL MODULES - starts all modules from loaded configuration
2. STOP ALL MODULES - stops all modules from loaded configuration
3. START MODULE - starts one specific module from loaded configuration
4. STOP MODULE - stops one specific module from loaded configuration
5. STARTED MODULES STATUS - displays status of loaded modules (stopped or running)
                            and PIDs of modules processes
6. AVAILABLE MODULES - prints out actual loaded configuration
7. SHOW GRAPH - if dot program is installed, this operation displays picture
                of oriented graph of modules
8. RELOAD CONFIGURATION - operation allows user to reload actual configuration
                          from initial xml file or from another xml file
9. STOP SUPERVISOR - this operation terminates whole Supervisor


Note: for reload operation is important unique module name. There are three cases,
      that can occur during reloading:

a) Supervisor didnt find a module with same name in loaded configuration -> inserting a new
   module.
b) Supervisor found a module with same name in loaded configuration -> compares every value
   and if there is a difference, the module will be reloaded.
c) Module in loaded configuration was not found in reloaded configuration -> it is
   removed.

Supervisor monitors the status of all modules and if needed, modules are auto-restarted.
Every module can be run with special "service" interface, which allows Supervisor to get
statistics about sent and received messages from module.
Another monitored event is CPU usage of every module.
These events are periodically monitored.

Supervisor also provides logs with different types of output:
 logs#1 - stdout and stderr of every started module
 logs#2 - supervisor output - this includes modules events (start, stop, restart,
          connecting to module, errors etc.) and in different output file statistics
          about messages and CPU usage.

Statistics about Nemea modules:
===============================

  Supervisor client has a special mode that is enabled by `-x`. It allows user
  to get statistics about message numbers and cpu usage of modules.
  In `-x` mode, client connects to the supervisor, receives and prints statistics,
  disconnects and terminates.
```
./supervisor_cli -x
```

  Note: Supervisor daemon must be running.

Output format - description:
----------------------------

```
<module unique name>,<information type>,<statistics/identification>
```

For different "information type", the part "statistics/identification" differs.

Example#1:
```
flowcounter1,in,0,1020229
```
The example means: number of received messages (1020229) of input interface (in)
with interface number (0) of the module (flowcounter1).

Example#2:
```
traffic_repeater2,out,0,510114,392,2
```
The example means: number of sent messages (510114), number of sent buffers (392),
number of executed "buffer auto-flushes" (2) of output interface (out)
with interface number (0) of the module (traffic_repeater2).

Example#3:
  flowcounter1,cpu,2,1
The example means: usage of CPU (cpu) in percent in kernel mode (2) and user mode (1)
of the module (flowcounter1).


Overall Example of the output with statistics:
----------------------------------------------

```
flowcounter1,in,0,1020229
flowcounter2,in,0,507519
flowcounter3,in,0,508817
flowcounter4,in,0,508817
traffic_repeater1,in,0,510115
traffic_repeater1,out,0,510114,392,2
traffic_repeater2,in,0,510115
traffic_repeater2,out,0,510114,392,2
nfreader1,out,0,515307,396,0
nfreader2,out,0,533479,410,0
traffic_merger3,in,0,510764
traffic_merger3,in,1,510763
traffic_merger3,out,0,1021527,786,2
flowcounter1,cpu,2,1
flowcounter2,cpu,1,0
flowcounter3,cpu,1,0
flowcounter4,cpu,2,0
traffic_repeater1,cpu,2,1
traffic_repeater2,cpu,2,1
nfreader1,cpu,7,1
nfreader2,cpu,5,0
traffic_merger3,cpu,5,2
```

