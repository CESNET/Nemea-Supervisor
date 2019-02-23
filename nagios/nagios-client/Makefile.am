nconfdir=$(pkgdatadir)/nagios-client
dist_nconf_DATA=conf/nrpe-part.conf

nagiosnrpedir=$(libdir)/nagios/plugins/nemea
dist_nagiosnrpe_SCRIPTS=scripts/check_ipfixcol_trapoutifc.sh \
	scripts/check_link_traffic.sh \
	scripts/check_free_memory.sh \
	scripts/check_nemea_sent.sh \
	scripts/check_network_interface.sh \
	scripts/check_nemea_dropped.sh \
	scripts/check_swap_memory.sh \
	scripts/check_supervisor.sh \
	scripts/nemea_common.sh \
	scripts/check_nemea_modules.py \
	scripts/utils.sh

EXTRA_DIST=README.md
