bin_PROGRAMS=supervisor supervisor_cli supervisor_remote
supervisor_SOURCES= graph.c graph.h supervisor.c supervisor.h supervisor_api.h supervisor_main.c internal.c internal.h
supervisor_LDADD = -lpthread
supervisor_cli_SOURCES= supervisor_cli.c
supervisor_remote_SOURCES= supervisor_remote.c
supervisor_remote_LDADD = -lpthread
pkgdocdir=${docdir}/supervisor
pkgdoc_DATA=README