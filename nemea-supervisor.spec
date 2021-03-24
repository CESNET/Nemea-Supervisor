Summary: Controller and monitoring module for Nemea
Name: nemea-supervisor
Version: 1.8.1
Release: 1
URL: http://www.liberouter.org/nemea
Source: https://www.liberouter.org/repo/SOURCES/%{name}-%{version}-%{release}.tar.gz
Group: Liberouter
License: BSD
Vendor: CESNET, z.s.p.o.
Packager: Travis CI User <travis@example.org>
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}
Requires: libtrap libxml2
Requires(pre): /usr/sbin/useradd, /usr/bin/getent
Requires(postun): /usr/sbin/userdel
BuildRequires: gcc make doxygen pkgconfig libtrap-devel libxml2-devel
Provides: nemea-supervisor

%description

%package  nagios-server
Summary: Configuration files dor nagios server.
Group: Liberouter
Provides: nemea-supervisor-nagios-server

%description nagios-server
The package contains configuration files for nagios server.
These files contain definitions of checkable services on NEMEA servers.

%package  nagios-nrpe
Summary: Nagios scripts and config files for NEMEA collector.
Group: Liberouter
Provides: nemea-supervisor-nagios-nrpe

%description nagios-nrpe
This package should be installed on NEMEA collectors to allow their
monitoring using a nagios server.

%prep
%setup

%build
./configure --prefix=%{_prefix} --libdir=%{_libdir} --sysconfdir=%{_sysconfdir}/nemea --bindir=%{_bindir}/nemea --datarootdir=%{_datarootdir} --docdir=%{_docdir}/nemea-supervisor --localstatedir=%{_localstatedir} -q --enable-silent-rules --disable-repobuild;
make -j5

%install
make -j5 DESTDIR=$RPM_BUILD_ROOT install

%post
%{_bindir}/nemea/prepare_default_config.sh install

%files
%{_bindir}/nemea/*
%{_docdir}/nemea-supervisor/README.md
%{_docdir}/nemea-supervisor/README.munin
%{_sysconfdir}/init.d/nemea-supervisor
%{_datarootdir}/munin/plugins/nemea_supervisor
%{_datarootdir}/munin/plugins/nemea_ipfixcol2
%{_datarootdir}/nemea-supervisor/warning.sup
%{_datarootdir}/nemea-supervisor/template.sup
%{_datarootdir}/nemea-supervisor/*.mkdir
%{_datarootdir}/nemea-supervisor/detectors/*
%{_datarootdir}/nemea-supervisor/data-sources/*
%{_datarootdir}/nemea-supervisor/loggers/*
%{_datarootdir}/nemea-supervisor/others/*
%{_datarootdir}/nemea-supervisor/README.md
%{_datarootdir}/nemea-supervisor/config_example.xml
%{_datarootdir}/nemea-supervisor/set_default_nic.sh
%{_datarootdir}/nemea-supervisor/prepare_default_config.sh
/lib/systemd/system
%defattr(755, nemead, nemead)
%config(noreplace) %{_sysconfdir}/nemea/ipfixcol-startup.xml
%defattr(664, nemead, nemead)
%config(noreplace) %{_sysconfdir}/nemea/supervisor_config_template.xml


%files nagios-server
%config(noreplace) %{_sysconfdir}/nagios/conf.d/nemea.cfg
%config(noreplace) %{_sysconfdir}/nagios/conf.d/nemea-hosts.cfg
%config(noreplace) %{_sysconfdir}/nagios/conf.d/nrpe.cfg

%files nagios-nrpe
%config(noreplace) %{_libdir}/nagios/plugins/nemea/check_ipfixcol_trapoutifc.sh
%config(noreplace) %{_libdir}/nagios/plugins/nemea/check_link_traffic.sh
%config(noreplace) %{_libdir}/nagios/plugins/nemea/check_free_memory.sh
%config(noreplace) %{_libdir}/nagios/plugins/nemea/check_nemea_sent.sh
%config(noreplace) %{_libdir}/nagios/plugins/nemea/check_network_interface.sh
%config(noreplace) %{_libdir}/nagios/plugins/nemea/check_nemea_dropped.sh
%config(noreplace) %{_libdir}/nagios/plugins/nemea/check_swap_memory.sh
%config(noreplace) %{_libdir}/nagios/plugins/nemea/check_supervisor.sh
%config(noreplace) %{_libdir}/nagios/plugins/nemea/nemea_common.sh
%config(noreplace) %{_libdir}/nagios/plugins/nemea/check_nemea_modules.py*
%config(noreplace) %{_libdir}/nagios/plugins/nemea/check_nemea_loaded_module_config
%config(noreplace) %{_libdir}/nagios/plugins/nemea/check_nemea_modules_connected
%config(noreplace) %{_libdir}/nagios/plugins/nemea/utils.sh
%config(noreplace) %{_datarootdir}/nemea-supervisor/nagios-client/nrpe-part.conf
