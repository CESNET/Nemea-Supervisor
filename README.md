## README outline

- [Project status](#project-status)
- [Brief description](#supervisor)
- [How to build](#how-to-build)
- [Dependencies](#dependencies)
- [Program parameters](#program-parameters)
- [Program modes](#program-modes)
- [Configuration](#configuration)
  - [Configuration file](#example)
  - [Reload configuration](#reload-configuration)
  - [Installed configuration](#installed-configuration)
- [Supervisor functions](#supervisor-functions)
- [Monitoring Nemea modules_ll](#monitoring-nemea-modules_ll)
- [Log files](#log-files)
- [Program termination](#program-termination)
  - [Backup file](#backup-file)
  - [Signals](#signals)
- [Supervisor client](#supervisor-client)
  - [Clients parameters](#clients-parameters)
  - [Collecting statistics about modules_ll](#collecting-statistics-about-modules_ll)
  - [Collecting information about modules_ll](#collecting-information-about-modules_ll)



## Project status

Travis CI build: [![Build Status](https://travis-ci.org/CESNET/Nemea-Supervisor.svg?branch=master)](https://travis-ci.org/CESNET/Nemea-Supervisor)

Coverity Scan: [![Coverity Scan Build Status](https://scan.coverity.com/projects/6190/badge.svg)](https://scan.coverity.com/projects/6190)



# Supervisor

This module allows user to configure and monitor Nemea modules_ll (see [basic modules_ll](https://github.com/CESNET/Nemea-Modules) and [detectors](https://github.com/CESNET/Nemea-Detectors)).
User specifies modules_ll in xml configuration file, which is input for the Supervisor and it is shown and described in the section "Configuration file".


## How to build

Simply with:

```sh
./bootstrap.sh
cmake .
make
```
or use get docker image from zmat/nemea-supervisor-sysrepo-edition (built from deploy/docker/staas-demo)


## Dependencies

Supervisor needs the following to be installed:

- [sysrepo](https://github.com/sysrepo/sysrepo)
- [Nemea-framework](https://github.com/CESNET/Nemea-Framework)


## Program parameters

List of **mandatory** parameters the program accepts:
- `-L PATH` or `--logs-path=path`   Path of the directory where the logs (both supervisor's and modules_ll') will be saved.

List of **optional** parameters the program accepts:
- `-d` or `--daemon`   Runs supervisor as a system daemon.
- `-h` or `--help`   Prints program help.


## Program modes

Supervisor can run in one of the following modes:

####1) Interactive Mode

```
supervisor -L logs_path
```


####2) System daemon

Program is executed with `-d` (or `--daemon`) argument and runs as a process in backround.

```
supervisor -L logs_path --daemon
```

## Configuration

This version of Supervisor is loading configuration from sysrepo module **nemea** which is defined by YANG model in yang/nemea.yang. In yang/data/ you can find few configuration examples.

Supervisor also listens for configuration changes in running datstore of **nemea** and applies new configuration. Changes in startup datastore are ignored by Supervisor at runtime.

It is also possible to control Supervisor using [NEMEA GUI](http://github.com/zidekmat/nemea-gui). 

##Supervisor functions
Communication with Supervisor is done through sysrepo.


User can do various operations with modules via Supervisor. After launch (either supervisor in non-daemon mode or supervisor_cli) a menu with the following operations appears:

- `1. ENABLE MODULE OR PROFILE` - prints a list of disabled modules_ll and profiles, the selected ones are enabled
- `2. DISABLE MODULE OR PROFILE` - prints a list of enabled modules_ll and profiles, the selected ones are disabled
- `3. RESTART RUNNING MODULE` - prints a list of running modules_ll, the selected ones are restarted
- `4. PRINT BRIEF STATUS` - prints a table of the loaded modules_ll divided into profiles and shows enabled, status and PID of all modules_ll
- `5. PRINT DETAILED STATUS` - prints the same information as the option 4 plus status of all modules_ll interfaces including [service interface](https://github.com/CESNET/Nemea-Framework/blob/master/libtrap/service-ifc.md) (whether it is connected or number of connected clients)
- `6. PRINT LOADED CONFIGURATION` - prints all information from the configuration file
- `7. RELOAD CONFIGURATION` - performs reload of the configuration (see [Reload configuration](#reload-configuration))
- `8. PRINT SUPERVISOR INFO` - basic information about running supervisor - package and git version, used paths etc.
- `9. BROWSE LOG FILES` - prints a list of all log files, selected log file is opened with pager (see [Log files](#log-files))
- `0. STOP SUPERVISOR` - stops the supervisor **but not the running modules_ll**

If the supervisor is running as a system daemon, last option "STOP SUPERVISOR" is replaced with

- `-- Type "Cquit" to exit client --` - stops and disconnects the client
- `-- Type "Dstop" to stop daemon --` - stops the supervisor **but not the running modules_ll**



##Monitoring NEMEA modules

####Modules status

Supervisor monitors the status of every module. The status can be **running** or **stopped** and it depends on the **enabled flag** of the module. Once the module is set to enabled, supervisor will automatically start it. If the module stops but is still enabled (user did not disable it), supervisor will restart it. Maximum number of restarts per minute can be specified with **module-restarts** in the configuration file. When the limit is reached, module is automatically set to disabled.
If the module is running and it is disabled by user, SIGINT is used to stop the module. If it keeps running, SIGKILL must be used.

####Statistics about modules_ll´ interfaces

Every Nemea module has an implicit **service interface**, which allows Supervisor to get statistics about modules_ll interfaces. These statistics include the following counters:

- for every input interface: received messages, recevied buffers
- for every output interface: sent messages, sent buffers, dropped messages, autoflushes

Data sent via service interface are encoded in JSON format. More information about service interface [here](https://github.com/CESNET/Nemea-Framework/blob/master/libtrap/service-ifc.md)

####CPU and memory usage

The last monitored statistic is CPU usage (kernel and user mode) and system memory usage of every module.



## Log files

Supervisor uses log files for its own output and also for an output of the started modules_ll.
Path of the logs can be specified with **mandatory** program parameter `-L PATH` or `--logs-path=path`.

Logs directory has the following content:
- supervisor_log - contains warning or error messages of the supervisor
- modules_events - contains messages about modules_ll´ status changes
- modules_statistics - contains statistics about modules_ll´ interfaces (they are printed periodically every minute)
- directory modules_logs - contains files with modules_ll´ stdout and stderr in form of [mod_name]_stdout and [mod_name]_stderr



## Program termination

Supervisor can be terminated via one of the menu options:

- interactive mode - option `0. STOP SUPERVISOR`
- daemon mode - option `Dstop`

These two options **don´t stop running modules_ll** and generate backup file (see next subsection).

To stop **both the supervisor and all running modules_ll**, use `service nemea-supervisor stop`.

It is also possible to terminate the supervisor via sending a signal. `service nemea-supervisor stop` sends a SIGTERM. See all signals and their effect [here](#signals)


### Backup file

Supervisor is able to terminate without stopping running modules_ll and it will "find" them again after restart. This is achived via a backup file which is generated during termination if needed.
Every backup file is bound to unique configuration template it was started with (`-T` parameter) and the path of the backup file is chosen according to it. The path is `/tmp/sup_tmp_dir/PREFIX_sup_backup_file.xml` where "PREFIX" is a number computed from absolute path of the configuration template.
The backup file contains current configuration with PIDs of running modules_ll.
Together with backup file is also generated info file with basic information about current configuration.

For example supervisor started with `-T /etc/nemea/supervisor_config_template.xml` generates the following files:

- `/tmp/sup_tmp_dir/89264_sup_backup_file.xml` - backup file
- `/tmp/sup_tmp_dir/89264_sup_backup_file.xml_info` - file with basic information about current configuration

If the user executes supervisor with the same configuration template, supervisor will automatically find the backup file, loads the configuration and connects to running modules_ll.



### Signals

Signal handler catches the following signals:

- SIGTERM - stops all running modules_ll, does not generate backup file (the only case modules_ll are stopped)
- SIGINT - it let the modules_ll continue running and generates backup file
- SIGQUIT - same as SIGINT
- SIGSEGV - this is for case something goes wrong during runtime. SIGSEGV is catched, modules_ll continue running and backup file is saved.



## Supervisor client

### Clients parameters

List of **optional** parameters the program accepts:
- `-h`   Prints program help.
- `-s SOCKET`   Path of the unix socket which is used for supervisor daemon and client communication.
- `-x`   Receives and prints statistics about modules_ll and terminates.
- `-r`   Sends a command to supervisor to reload the configuration.
- `-i`   Receives and prints information about modules_ll in JSON and terminates.

Note: All these parameters are optional so if the client is started without `-x`, `-r` or `-i` (`supervisor_cli` or `supcli` from RPM installation) it enters configuration mode with [these functions](#supervisor-functions).


### Collecting statistics about modules_ll

  Supervisor client has a special mode that is enabled by `-x`. It allows user
  to get statistics about modules_ll mentioned [here](#statistics-about-modules_ll-interfaces).
  In `-x` mode, client connects to the supervisor, receives and prints statistics,
  disconnects and terminates.

  Note: this mode is used by plugin [nemea-supervisor](https://github.com/CESNET/Nemea-Supervisor/tree/master/munin) for munin.

#### Output format

```
<module unique name>,<information type>,<statistics/identification>
```

For different "information type", the part "statistics/identification" differs. There are 3 basic types of information:

1. Module interfaces statistics: ```<module unique name>,<interface direction>,<interface type>,<interface ID>,<interface counters>```
  * interface direction is either *in* or *out*
  * interface type is one of *{t, u, f, g, b}* values corresponding to *{tcpip, unix-socket, file, generator, blackhole}*
  * interface ID is *port number* (tcpip), *socket name* (unix-socket), *file name* (file) or *"none"* (generator, blackhole)
  * interface counters are described [here](#statistics-about-modules_ll-interfaces)
2. Module CPU usage: ```<module unique name>,cpu,<kernel mode CPU usage>,<user mode CPU usage>```
3. Module MEM usage: ```<module unique name>,mem,<size of virtual memory in MB>```


#### Overall example of the output with statistics:

```
dns_amplification,in,u,flow_data_source,92326719485,72985549
dns_amplification,out,t,12001,789,0,540,7291604
dnstunnel_detection,in,u,flow_data_source,3099282393,4126406
dnstunnel_detection,out,t,12004,100591,0,8959,1128918
dnstunnel_detection,out,u,dnstunnel_sdmoutput,224,0,0,1137913

dns_amplification,cpu,0,4
dnstunnel_detection,cpu,1,3

dns_amplification,mem,193928
dnstunnel_detection,mem,208600
```


### Collecting information about modules_ll

Another special mode of the supervisor client is enabled by `-i`. It allows user to get basic information about modules_ll in JSON format:

- module name (name of the running process)
- module index (in supervisor´s configuration)
- module parameters
- binary path
- module status (running or stopped)
- information about input and output interfaces
  - input ifc info format: `type:id:is_conn` where *is connected* is one char *0* or *1*
  - output ifc info format: `type:id:num_clients` where *number of clients* is int32

In `-i` mode, client connects to the supervisor, receives and prints information, disconnects and terminates.


#### Example of the output with modules_ll information:

```json
{
   "modules_ll-number":2,
   "modules_ll":[
      {
         "module-idx":2,
         "module-name":"dns_amplification",
         "status":"running",
         "module-params":"-d /data/dns_amplification_detection/",
         "bin-path":"/usr/bin/nemea/amplification_detection",
         "inputs":[
            "u:flow_data_source:0"
         ],
         "outputs":[
            "t:12001:1"
         ]
      },
      {
         "module-idx":3,
         "module-name":"dnstunnel_detection",
         "status":"running",
         "module-params":"none",
         "bin-path":"/usr/bin/nemea/dnstunnel_detection",
         "inputs":[
            "u:flow_data_source:0"
         ],
         "outputs":[
            "t:12004:1",
            "u:dnstunnel_sdmoutput:0"
         ]
      }
   ]
}
```