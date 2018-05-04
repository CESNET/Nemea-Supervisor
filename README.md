## README outline

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

# Supervisor

This module allows user to configure and monitor Nemea modules (see [basic modules](https://github.com/CESNET/Nemea-Modules) and [detectors](https://github.com/CESNET/Nemea-Detectors)).
User specifies modules in xml configuration file, which is input for the Supervisor and it is shown and described in the section "Configuration file".


## How to build

Simply with:

```sh
./bootstrap.sh
cmake .
make
```
or use get docker image from zmat/nemea-supervisor-sysrepo-edition (built from `deploy/docker/staas-demo`).

To rebuild documentation in `doc/` folder, use
```sh
doxygen doc/Doxyfile
```


## Dependencies

Supervisor needs the following to be installed:

- [sysrepo](https://github.com/sysrepo/sysrepo)
- [Nemea-framework](https://github.com/CESNET/Nemea-Framework)


## Program parameters

List of **mandatory** parameters the program accepts:
- `-L PATH` or `--logs-path=path`   Path of the directory where the logs (both supervisor's and modules') will be saved.

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



##Monitoring NEMEA modules

####Modules status

Supervisor monitors the status of every module. The status can be **running** or **stopped** and it depends on the **enabled flag** of the module. Once the module is set to enabled, supervisor will automatically start it. If the module stops but is still enabled (user did not disable it), supervisor will restart it. Maximum number of restarts per minute can be specified with **module-restarts** in the configuration file. When the limit is reached, module is automatically set to disabled.
If the module is running and it is disabled by user, SIGINT is used to stop the module. If it keeps running, SIGKILL must be used.

####Statistics about modules´ interfaces

Every Nemea module has an implicit **service interface**, which allows Supervisor to get statistics about modules interfaces. These statistics include the following counters:

- for every input interface: received messages, recevied buffers
- for every output interface: sent messages, sent buffers, dropped messages, autoflushes

Data sent via service interface are encoded in JSON format. More information about service interface [here](https://github.com/CESNET/Nemea-Framework/blob/master/libtrap/service-ifc.md)

####CPU and memory usage

The last monitored statistic is CPU usage (kernel and user mode) and system memory usage of every module.



## Log files

Supervisor uses log files for its own output and also for an output of the started modules.
Path of the logs can be specified with **mandatory** program parameter `-L PATH` or `--logs-path=path`.

Logs directory has the following content:
- supervisor_log - contains warning or error messages of the supervisor
- modules_events - contains messages about modules´ status changes
- modules_statistics - contains statistics about modules´ interfaces (they are printed periodically every minute)
- directory modules_logs - contains files with modules´ stdout and stderr in form of [mod_name]_stdout and [mod_name]_stderr



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
