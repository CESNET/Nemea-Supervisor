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
- [Monitoring Nemea modules](#monitoring-nemea-modules)
- [Log files](#log-files)
- [Program termination](#program-termination)
  - [Backup file](#backup-file)
  - [Signals](#signals)
- [Supervisor client](#supervisor-client)
  - [Clients parameters](#clients-parameters)
  - [Collecting statistics about modules](#collecting-statistics-about-modules)
  - [Collecting information about modules](#collecting-information-about-modules)



## Project status

Travis CI build: [![Build Status](https://travis-ci.org/CESNET/Nemea-Supervisor.svg?branch=master)](https://travis-ci.org/CESNET/Nemea-Supervisor)

Coverity Scan: [![Coverity Scan Build Status](https://scan.coverity.com/projects/6190/badge.svg)](https://scan.coverity.com/projects/6190)



# Supervisor

This module allows user to configure and monitor Nemea modules (see [basic modules](https://github.com/CESNET/Nemea-Modules) and [detectors](https://github.com/CESNET/Nemea-Detectors)).
User specifies modules in xml configuration file, which is input for the Supervisor and it is shown and described in the section "Configuration file".


## How to build

Simply with:

```sh
./bootstrap.sh
./configure
make
```
Useful parameters for configure are `--prefix`, `--libdir`, `--sysconfdir` and `--bindir`. For example:

```sh
./configure --sysconfdir=/etc/nemea/ --prefix=/usr/ --bindir=/usr/bin/nemea/ --libdir=/usr/lib64/
```

And possible installation:

```sh
sudo make install
```


## Dependencies

Supervisor needs the following to be installed:

- libxml2 devel package
- [Nemea-framework](https://github.com/CESNET/Nemea-Framework).


## Program parameters

List of **mandatory** parameters the program accepts:
- `-T FILE` or `--config-template=path`   Path of the XML file with the configuration template.
- `-L PATH` or `--logs-path=path`   Path of the directory where the logs (both supervisor's and modules') will be saved.

List of **optional** parameters the program accepts:
- `-d` or `--daemon`   Runs supervisor as a system daemon.
- `-h` or `--help`   Prints program help.
- `-s SOCKET` or `--daemon-socket=path`   Path of the unix socket which is used for supervisor daemon and client communication.
- `-C PATH` or `--configs-path=path`   Path of the directory where the generated configuration files will be saved.



## Program modes

Supervisor can run in one of the following modes:

#### 1) Interactive Mode

```
supervisor -T supervisor_config_template.xml -L logs_path
```


#### 2) System daemon

Program is executed with `-d` (or `--daemon`) argument and runs as a process in backround.

```
supervisor -T supervisor_config_template.xml -L logs_path --daemon
```

  The activity of the running daemon can be controlled by a special thin client:

```
supervisor_cli (after installation from RPM, there is symlink "supcli" for the client)
```

  Both daemon and client supports optional `-s` to specify path to the UNIX socket
  that is used for communication between daemon and client (more information about the client can be found in the section "Supervisor client").


#### 3 ) System service

Supervisor is installed as a systemd service with the following commands:

- `service nemea-supervisor start` - starts the supervisor as a system deamon
- `service nemea-supervisor stop` - stops the deamon and **all running Nemea modules** from its configuration
- `service nemea-supervisor restart` - performs service start and service stop
- `service nemea-supervisor status` - returns running or stopped
- `service nemea-supervisor reload` - updates the configuration according to the configuration file



## Configuration

### Example

Example configuration file with comments: [config_example.xml](https://github.com/CESNET/Nemea-Supervisor/blob/master/configs/config_example.xml)

The picture below should help to understand the configuration file. It consists of several module groups (profiles) which can be controlled separately.

![Configuration file principle](doc/config-file-expl.png)


### Real usage of the configuration file

It is split into smaller parts called **sup files** (file.sup) for easier maintaining by multiple users. Example of such a file is [this sup file](https://github.com/CESNET/Nemea-Supervisor/blob/master/configs/detectors/dnstunnel_detection.sup) containing a configuration of one Nemea detection module called dns tunnel detector.

Sup files can be placed into directories according to their category (e.g. detectors, data-sources etc.). It is shown [here](https://github.com/CESNET/Nemea-Supervisor/tree/master/configs).

[XML template](https://github.com/CESNET/Nemea-Supervisor/blob/master/configs/supervisor_config_template.xml.in) helps to gather all the sup files and to construct the final configuration file using following directives:

```xml
<!-- include path_to_sup_file -->
<!-- include path_to_sup_files_dir -->
```

Path to the XML template is one of mandatory parameters for the supervisor (`-T FILE` or `--config-template=path`).


### Reload configuration

The most important functionality of the supervisor. It updates the configuration according to the configuration file. It has the following phases:

- **Generate** config - generates the final configuration file by replacing include directives in XML template with the content of the .sup files
- **Validate** config - checks the generated config file according to defined syntax and semantic rules (structure and values)
- **Apply** config - if the validation successfully finishes, all changes are applied to the running configuration

![Reload configuration](doc/reload-config.png)

There are three basic cases that can occur during "Apply config" phase:

- A module with same name was not found in loaded configuration -> a new module is **inserted**.
- A module with same name was found in loaded configuration -> every value is compared and if there is a difference, the module is **reloaded**.
- A module in loaded configuration was not found in the new configuration -> it is **removed**.


### Installed configuration

After installation from RPM or by using

```sh
./configure --sysconfdir=/etc/nemea/ --prefix=/usr/ --bindir=/usr/bin/nemea/ --libdir=/usr/lib64/
```

during build (see [How to build](#how-to-build)), there are 2 important paths with installed configuration.

- `/usr/share/nemea-supervisor` - contains default installed configuration from [configs](https://github.com/CESNET/Nemea-Supervisor/tree/master/configs) (all .sup files divided into directories the same way as on the picture [here](#reload-configuration))
- `/etc/nemea` - contains [XML template](https://github.com/CESNET/Nemea-Supervisor/blob/master/configs/supervisor_config_template.xml.in) with includes of directories also from /etc/nemea.

It is possible to copy directories with .sup files from `/usr/share/nemea-supervisor` to `/etc/nemea` and use default configuration or it is possible to prepare own configuration and add the path of the directory or .sup file to the XML template.



## Supervisor functions

User can do various operations with modules via Supervisor. After launch (either supervisor in non-daemon mode or supervisor_cli) a menu with the following operations appears:

- `1. ENABLE MODULE OR PROFILE` - prints a list of disabled modules and profiles, the selected ones are enabled
- `2. DISABLE MODULE OR PROFILE` - prints a list of enabled modules and profiles, the selected ones are disabled
- `3. RESTART RUNNING MODULE` - prints a list of running modules, the selected ones are restarted
- `4. PRINT BRIEF STATUS` - prints a table of the loaded modules divided into profiles and shows enabled, status and PID of all modules
- `5. PRINT DETAILED STATUS` - prints the same information as the option 4 plus status of all modules interfaces including [service interface](https://github.com/CESNET/Nemea-Framework/blob/master/libtrap/service-ifc.md) (whether it is connected or number of connected clients)
- `6. PRINT LOADED CONFIGURATION` - prints all information from the configuration file
- `7. RELOAD CONFIGURATION` - performs reload of the configuration (see [Reload configuration](#reload-configuration))
- `8. PRINT SUPERVISOR INFO` - basic information about running supervisor - package and git version, used paths etc.
- `9. BROWSE LOG FILES` - prints a list of all log files, selected log file is opened with pager (see [Log files](#log-files))
- `0. STOP SUPERVISOR` - stops the supervisor **but not the running modules**

If the supervisor is running as a system daemon, last option "STOP SUPERVISOR" is replaced with

- `-- Type "Cquit" to exit client --` - stops and disconnects the client
- `-- Type "Dstop" to stop daemon --` - stops the supervisor **but not the running modules**



## Monitoring Nemea modules

#### Modules status

Supervisor monitors the status of every module. The status can be **running** or **stopped** and it depends on the **enabled flag** of the module. Once the module is set to enabled, supervisor will automatically start it. If the module stops but is still enabled (user did not disable it), supervisor will restart it. Maximum number of restarts per minute can be specified with **module-restarts** in the configuration file. When the limit is reached, module is automatically set to disabled.
If the module is running and it is disabled by user, SIGINT is used to stop the module. If it keeps running, SIGKILL must be used.

#### Statistics about modules´ interfaces

Every Nemea module has an implicit **service interface**, which allows Supervisor to get statistics about modules interfaces. These statistics include the following counters:

- for every input interface: received messages, recevied buffers
- for every output interface: sent messages, sent buffers, dropped messages, autoflushes

Data sent via service interface are encoded in JSON format. More information about service interface [here](https://github.com/CESNET/Nemea-Framework/blob/master/libtrap/service-ifc.md)

#### CPU and memory usage

The last monitored statistic is CPU usage (kernel and user mode) and system memory usage of every module.



## Log files

Supervisor uses log files for its own output and also for an output of the started modules.
Path of the logs can be specified with **mandatory** program parameter `-L PATH` or `--logs-path=path`.

Logs directory has the following content:
- supervisor_log - contains warning or error messages of the supervisor
- modules_events - contains messages about modules´ status changes
- modules_statistics - contains statistics about modules´ interfaces (they are printed periodically every minute)
- directory modules_logs - contains files with modules´ stdout and stderr in form of [module_name]_stdout and [module_name]_stderr



## Program termination

Supervisor can be terminated via one of the menu options:

- interactive mode - option `0. STOP SUPERVISOR`
- daemon mode - option `Dstop`

These two options **don´t stop running modules** and generate backup file (see next subsection).

To stop **both the supervisor and all running modules**, use `service nemea-supervisor stop`.

It is also possible to terminate the supervisor via sending a signal. `service nemea-supervisor stop` sends a SIGTERM. See all signals and their effect [here](#signals)


### Backup file

Supervisor is able to terminate without stopping running modules and it will "find" them again after restart. This is achived via a backup file which is generated during termination if needed.
Every backup file is bound to unique configuration template it was started with (`-T` parameter) and the path of the backup file is chosen according to it. The path is `/tmp/sup_tmp_dir/PREFIX_sup_backup_file.xml` where "PREFIX" is a number computed from absolute path of the configuration template.
The backup file contains current configuration with PIDs of running modules.
Together with backup file is also generated info file with basic information about current configuration.

For example supervisor started with `-T /etc/nemea/supervisor_config_template.xml` generates the following files:

- `/tmp/sup_tmp_dir/89264_sup_backup_file.xml` - backup file
- `/tmp/sup_tmp_dir/89264_sup_backup_file.xml_info` - file with basic information about current configuration

If the user executes supervisor with the same configuration template, supervisor will automatically find the backup file, loads the configuration and connects to running modules.



### Signals

Signal handler catches the following signals:

- SIGTERM - stops all running modules, does not generate backup file (the only case modules are stopped)
- SIGINT - it let the modules continue running and generates backup file
- SIGQUIT - same as SIGINT
- SIGSEGV - this is for case something goes wrong during runtime. SIGSEGV is catched, modules continue running and backup file is saved.



## Supervisor client

### Clients parameters

List of **optional** parameters the program accepts:
- `-h`   Prints program help.
- `-s SOCKET`   Path of the unix socket which is used for supervisor daemon and client communication.
- `-x`   Receives and prints statistics about modules and terminates.
- `-r`   Sends a command to supervisor to reload the configuration.
- `-i`   Receives and prints information about modules in JSON and terminates.

Note: All these parameters are optional so if the client is started without `-x`, `-r` or `-i` (`supervisor_cli` or `supcli` from RPM installation) it enters configuration mode with [these functions](#supervisor-functions).


### Collecting statistics about modules

  Supervisor client has a special mode that is enabled by `-x`. It allows user
  to get statistics about modules mentioned [here](#statistics-about-modules-interfaces).
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
  * interface counters are described [here](#statistics-about-modules-interfaces)
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


### Collecting information about modules

Another special mode of the supervisor client is enabled by `-i`. It allows user to get basic information about modules in JSON format:

- module name (name of the running process)
- module index (in supervisor´s configuration)
- module parameters
- binary path
- module status (running or stopped)
- information about input and output interfaces
  - input ifc info format: `ifc_type:ifc_id:is_conn` where *is connected* is one char *0* or *1*
  - output ifc info format: `ifc_type:ifc_id:num_clients` where *number of clients* is int32

In `-i` mode, client connects to the supervisor, receives and prints information, disconnects and terminates.


#### Example of the output with modules information:

```json
{
   "modules-number":2,
   "modules":[
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
