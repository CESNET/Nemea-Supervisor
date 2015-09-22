## README outline

- [Project status](#project-status)
- [Breaf description](#supervisor)
- [Program arguments](#program-arguments)
- [Program modes](#program-modes)
  - Interactive
  - Daemon
  - System service
  - Netopeer server plugin
- [Configuration file](#configuration-file)
- [Configuring modules](#configuring-modules)
- [Monitoring modules](#monitoring-modules)
  - Module status
  - Modules CPU and memory usage
  - Communication between modules
- [Log files](#log-files)
- [Program termination](#program-termination)
  - [Backup file](#backup-file)
  - [Signals](#signals)
- [Supervisor client](#supervisor-client)
  - Usage of the client
  - [Collecting statistics about Nemea modules](#statistics-about-nemea-modules)
- [Output format](#output-format)



## Project status

Travis CI build: [![Build Status](https://travis-ci.org/CESNET/Nemea-Supervisor.svg?branch=master)](https://travis-ci.org/CESNET/Nemea-Supervisor)

Coverity Scan: [![Coverity Scan Build Status](https://scan.coverity.com/projects/6190/badge.svg)](https://scan.coverity.com/projects/6190)



# Supervisor

This module allows user to configure and monitor Nemea modules (see [basic modules](https://github.com/CESNET/Nemea-Modules) and [detectors](https://github.com/CESNET/Nemea-Detectors)).
User specifies modules in xml configuration file, which is input for the Supervisor and it is shown and described in the section "Configuration file".



## Program arguments

Here is the list of arguments the program accepts:
- `-f FILE` or `--config-file=FILE` - xml file with initial configuration (the only mandatory parameter)
- `-L PATH` or `--logs-path=PATH` - optional path to Supervisor logs directory (see more information about logs in the section "Log files")
- `-s SOCKET` or `--daemon-socket=SOCKET` - optional path to unix socket used for communication with supervisor client (default /tmp/daemon_supervisor.sock)
- `-v` or `--verbose` - optional verbose flag used for enabling printing stats about messages and CPU usage to "supervisor_log_statistics" file in logs directory (more about these stats down below) // TODO context
- `-d` or `--daemon` - flag used for running Supervisor as a process in background
- `-h` or `--help` - program prints help and terminates



## Program modes

Supervisor can run in one of the following modes:

1) Interactive Mode:

```
./supervisor -f config_file.xml
```

2) Daemon Mode:
  Program is executed with `-d (or --daemon)` argument and runs as a process in backround
  of the operating system.

```
./supervisor -f config_file.xml --daemon
```

  The activity of the running daemon can be controlled by a special thin client:

```
./supervisor_cli
```

  Both daemon and client supports `-s` to specify path to the UNIX socket
  that is used for communication between daemon and client (more information about the client can be found in the section "Supervisor client").

  Note: paths must match to communicate with right Supervisor daemon

3) System service // TODO

4) Netopeer server plugin (using NETCONF protocol) // TODO


## Configuration file

Example configuration of one nemea module called flowcounter:

```xml
<nemea-supervisor>
  <supervisor>
    <verbose>true</verbose>
    <module-restarts>4</module-restarts>
    <logs-directory>/data/supervisor_logs</logs-directory>
  </supervisor>
  <modules>
    <name>test_profile</name>
    <enabled>true</enabled>
    <module>
      <name>flowcounter</name>
      <enabled>false</enabled>
      <params></params>
      <path>/usr/bin/nemea/flowcounter</path>
      <module-restarts>0</module-restarts>
      <trapinterfaces>
        <interface>
          <note></note>
          <type>TCP</type>
          <direction>IN</direction>
          <params>localhost:8004</params>
        </interface>
      </trapinterfaces>
    </module>
  </modules>
</nemea-supervisor>
```

Every module contains unique name (future name of the running process), path to binary file,
program parameters, enabled flag, module-restarts and trap interfaces. Enabled flag tells Supervisor to run
or not to run module after startup and module-restarts is maximum module restarts per minute. The trap interface
is specified by type (tcp, unixsocket or service), direction (in or out), parameters and optional note.

Trap interface parameters depends on the interface direction. Output interface has optional address (default is localhost) and port. Input interface has port and optional number of clients (default number of clients is 10). There are also few optional setters that can affect behaviour of the interface (check // TODO ref for more information about libtrap interfaces and their params).

Modules can be divided into profiles (groups of modules - element "modules"). Every profile has unique name and enabled flag.
This allows user to create several groups of modules (e.g. "detectors", "filters" or "loggers") and turn
all of them on/off by one simple change in the configuration file.

Optional element "supervisor" in the root element "nemea-supervisor" containts several optional items:

- verbose - sets verbose flag to TRUE or FALSE
- module-restarts - sets maximum number of restarts per minute for all modules (default number is 3). If this element is set also for concrete module, it has bigger priority.
- logs-directory - sets supervisor logs directory path (bigger priority than the path from -L program parameter). This allows user to change logs directory during runtime (see reload configuration in the section "Options").

// TODO ref to data model



## Configuring modules

User can do various operations with modules via Supervisor. After launch appears
menu with available operations:

1. START ALL MODULES - starts all modules from loaded configuration
2. STOP ALL MODULES - stops all modules from loaded configuration
3. START MODULE - starts one specific module from loaded configuration
4. STOP MODULE - stops one specific module from loaded configuration
5. STARTED MODULES STATUS - displays status of loaded modules (stopped or running)
                            and PIDs of modules processes
6. AVAILABLE MODULES - prints out actual loaded configuration
7. RELOAD CONFIGURATION - operation allows user to reload actual configuration
                          from initial xml file or from another xml file
8. STOP SUPERVISOR - this operation terminates whole Supervisor


Note: for reload operation is important unique module name. There are three cases,
      that can occur during reloading:

a) Supervisor didnt find a module with same name in loaded configuration -> inserting a new
   module.
b) Supervisor found a module with same name in loaded configuration -> compares every value
   and if there is a difference, the module will be reloaded.
c) Module in loaded configuration was not found in reloaded configuration -> it is
   removed.



## Monitoring modules

Supervisor monitors the status of all modules which depends on "enabled flag" of the module. If the status is stopped and the flag is set to true, modules is restared. There is limit of restarts per minute which can be modified in configuration file (optional element module-restarts). On the other hand if the module is running and flag is set to false, SIGINT is used to stop the module. If it keeps running, SIGKILL is used to stop it.

Every Nemea module can be run with special "service" interface (see [configuration file](supervisor_config.xml)), which allows Supervisor to get statistics about module interfaces. These stats include following counters:

- input interface: received messages, recevied buffers
- output interface: sent messages, sent buffers, dropped messages, autoflushes

Data sent via service interface are encoded in JSON format. More information about stats utilization [here](#statistics-about-nemea-modules)

Last monitored event is CPU usage (kernel and user modes) and system memory usage of every module.



## Log files

Supervisor uses log files for its own output and also for an output of the started modules.

Path of the logs can be specified in two ways:
- in configuration file (optional element logs-directory in example configuration file)
- program parameter (-L path, --logs-path=path) can be used only in the begining

If the path is specified with the parameter and also in the configuration file, bigger priority has the config file.
The path can be changed during runtime by adding or changing the element in the config file and performing reload configuration.

In case the user-defined path is not correct (process has no permissions to create the directories etc.), default path is used.
The path depends on the current supervisors mode:
- interactive mode: `/tmp/interactive_supervisor_logs/`
- daemon mode: `/tmp/daemon_supervisor_logs/`
- netconf mode: `/tmp/netconf_supervisor_logs/`

These paths are used because /tmp directory is accesable by every process so the output won´t be lost.

Content of the log directory is as follows:
- logs-path/supervisor_log
- logs-path/supervisor_debug_log
- logs-path/supervisor_log_module_events - log with modules events (starting module [module_name], connecting to module [module_name], error while receiving from module [module_name] etc.)
- logs-path/supervisor_log_statistics
- logs-path/modules_logs/ - contains files with started modules stdout and stderr in form of [module_name]_stdout and [module_name]_stderr



## Program termination

Supervisor can be terminated via one of the menu options (interactive mode "STOP SUPERVISOR" or daemon mode "Dstop") or via sending a signal.



### Backup file

Supervisor is able to terminate without stopping running modules and "find" them again after restart. This is achived via backup file which is generated during termination if needed.
Every backup file is bound to unique configuration file so the path is chosen according to it. The path is `/tmp/PREFIX_sup_backup_file.xml` where "PREFIX" is a number computed from absolute path of the configuration file.
The backup file contains current configuration with PIDs of running modules.
Together with backup file is also generated info file with basic information about current configuration.

Example:

```
./supervisor -f /data/configs/supervisor_config.xml
```

Start some modules and terminate supervisor. Because of running modules it generates following files:

- `/tmp/65018_sup_backup_file.xml` - backup file
- `/tmp/65018_sup_backup_file.xml_info` - file with basic information about current configuration

If the user executes supervisor with the same configuration file (which is also saved in the .xml_info file), supervisor will automatically find the backup file, loads the configuration and connects to running modules.



### Signals

Signal handler catches following signals:

- SIGTERM - stops all running modules and does not generate backup file (the only case modules are stopped)
- SIGINT - it let the modules continue running and generates backup file
- SIGQUIT - same as SIGINT
- SIGSEGV - this is for case something goes wrong during runtime. SIGSEGV is catched, modules continue running and backup file is saved.



## Supervisor client

The client communicates with supervisor daemon (process in the background - supervisor started with `-d` argument) via UNIX socket which can be specified with `-s SOCKET` just like supervisor. According to the optional given argument it performs one of the following commands:

- reload supervisor - `./supervisor_cli -r`
- get modules statistics - `./supervisor_cli -x`
- enter configuration mode - `./supervisor_cli`

Note: All these parameters are optional so if the client is started without them (`./supervisor_cli`), it uses default socket path (/tmp/daemon_supervisor.sock) and enters configuration mode.



### Statistics about Nemea modules

  Supervisor client has a special mode that is enabled by `-x`. It allows user
  to get statistics about message numbers and cpu usage of modules.
  In `-x` mode, client connects to the supervisor, receives and prints statistics,
  disconnects and terminates.
```
./supervisor_cli -x
```

  Note: Supervisor daemon must be running.
  
  // TODO munin

### Output format

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


### Overall Example of the output with statistics:

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

