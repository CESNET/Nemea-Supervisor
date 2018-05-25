## README outline

- [Brief description](#supervisor)
- [How to build](#how-to-build)
- [How to test](#how-to-test)
- [Dependencies](#dependencies)
- [Program parameters](#program-parameters)
- [Program modes](#program-modes)
- [Configuration](#configuration)
- [Supervisor functions](#supervisor-functions)
- [Monitoring Nemea modules](#monitoring-nemea-modules)
- [Log files](#log-files)
- [Program termination](#program-termination)
  - [Last PID backup](#last-pid-backup)
  - [Signals](#signals)
- [Developer documentation](#developer-documentation)

# Supervisor
This program allows user to configure and monitor Nemea modules (see [basic modules](https://github.com/CESNET/Nemea-Modules) and [detectors](https://github.com/CESNET/Nemea-Detectors)). It is based on [NEMEA Supervisor](https://github.com/CESNET/Nemea-Supervisor).


## How to build
First install [dependencies](#dependencies) and [build dependencies](#build-dependencies), then simply run:

```sh
./bootstrap.sh
cmake .
make
```
or use get docker image from zmat/nemea-supervisor-sysrepo-edition (built from `deploy/docker/staas-demo`).

### Build dependencies
 - cmake
 - runtime [dependencies](#dependencies)


## How to test
```sh
cd tests && ./run_tests.sh
```

## Dependencies

Supervisor needs the following to be installed:

- [sysrepo](https://github.com/sysrepo/sysrepo)
- [libtrap](https://github.com/CESNET/Nemea-Framework/tree/master/libtrap) of [NEMEA Framework](https://github.com/CESNET/Nemea-Framework/tree/master) project
- [cmocka](https://github.com/clibs/cmocka) for tests


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
This version of Supervisor is loading configuration from [sysrepo](https://github.com/sysrepo/sysrepo) module **nemea** which is defined by YANG model in `yang/nemea.yang`. In `yang/data/` you can find few configuration examples.

Supervisor also listens for configuration changes in sysrepo's running datastore of **nemea** and applies new configuration. Changes in startup datastore are ignored by Supervisor at runtime.

It is also possible to control Supervisor using [NEMEA GUI](https://github.com/zidekmat/nemea-gui). 

##Monitoring NEMEA modules' instances

####Modules status
Supervisor monitors the status of every module. The status can be **running** or **stopped** and it depends on the **enabled flag** of the instance. Once the module is set to enabled, supervisor will automatically start it. If the instance stops but is still enabled (user did not disable it), supervisor will restart it. Maximum number of restarts per minute can be specified with **max-restarts-per-min** in configuration. When the limit is reached, instance is automatically set to disabled.
If the instance is running and it is disabled by user, SIGINT is used to stop the module. If it keeps running, SIGKILL must be used.

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
- directory modules_logs - contains files with modules´ stdout and stderr in form of [mod_name]_stdout and [mod_name]_stderr


### Last PID backup
Supervisor is able to terminate without stopping running module instances and it will "find" them again after restart. This is achived by storing last known PID of processes into sysrepo.

### Signals
Signal handler catches the following signals:

- SIGTERM - stops all running modules, does not generate backup file (the only case modules are stopped)
- SIGINT - it let the modules continue running and generates backup file
- SIGQUIT - same as SIGINT
- SIGSEGV - this is for case something goes wrong during runtime. SIGSEGV is catched, modules continue running and backup file is saved.

## Developer documentation
You can find it online on GitHub pages [here](https://zidekmat.github.io/nemea-supervisor-sysrepo-edition/index.html).
To rebuild your up-to-date version in `docs/` folder, use
```sh
doxygen docs/Doxyfile
```