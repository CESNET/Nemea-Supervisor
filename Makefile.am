ACLOCAL_AMFLAGS = -I m4

SUBDIRS=munin configs

bin_PROGRAMS=supervisor supervisor_cli
supervisor_SOURCES= supervisor.c supervisor.h supervisor_api.h supervisor_main.c internal.c internal.h
supervisor_LDADD = -lpthread -ltrap
supervisor_cli_SOURCES= supervisor_cli.c internal.h internal.c
doc_DATA=README.md
EXTRA_DIST = nemea-supervisor README.md
bin_SCRIPTS=nemea-supervisor


DISTCHECK_CONFIGURE_FLAGS="--with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)"
systemdsystemunit_DATA=nemea-supervisor.service

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

EXTRA_DIST += ncnemea/nemea-data.rng ncnemea/libtool ncnemea/nemea.c ncnemea/config.guess ncnemea/nemea.yang ncnemea/Makefile.in ncnemea/nemea-gdefs.rng ncnemea/config.sub ncnemea/paths_file ncnemea/aclocal.m4 ncnemea/ncnemea.h ncnemea/ltmain.sh ncnemea/nemea-schematron.xsl ncnemea/configure ncnemea/install-sh ncnemea/configure.in


install-exec-hook:
	mkdir -p $(DESTDIR)/$(initdir)
	$(LN_S) -f $(bindir)/nemea-supervisor $(DESTDIR)/$(initdir)/nemea-supervisor

