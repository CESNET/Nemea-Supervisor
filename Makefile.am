bin_PROGRAMS=supervisor supervisor_cli
supervisor_SOURCES= graph.c graph.h supervisor.c supervisor.h supervisor_api.h supervisor_main.c internal.c internal.h
supervisor_LDADD = -lpthread
supervisor_cli_SOURCES= supervisor_cli.c
doc_DATA=README
EXTRA_DIST = supervisor_config.xml
sysconf_DATA = supervisor_config.xml

if MAKE_RPMS
RPMDIR = RPMBUILD
RPMFILENAME=$(PACKAGE_NAME)-$(VERSION)
.PHONY: rpm
rpm:
	rm -rf "$(RPMDIR)/SOURCES/$(RPMFILENAME)"
	mkdir -p $(RPMDIR)/BUILD/ $(RPMDIR)/SRPMS/ $(RPMDIR)/RPMS/ $(RPMDIR)/SOURCES
	make ${AM_MAKEFLAGS} distdir='$(RPMDIR)/SOURCES/$(RPMFILENAME)' distdir
	( cd "$(RPMDIR)/SOURCES/"; tar -z -c -f $(RPMFILENAME)-$(RELEASE).tar.gz $(RPMFILENAME); rm -rf $(RPMFILENAME); )
	$(RPMBUILD) -ba $(PACKAGE_NAME).spec --define "_topdir `pwd`/$(RPMDIR)";

rpm-clean:
	rm -rf $(RPMDIR)
else
endif
