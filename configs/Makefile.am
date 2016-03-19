config_files=amplification_detection.mkdir \
	   amplification_detection.sup \
	   dnstunnel_detection.mkdir \
	   dnstunnel_detection.sup \
	   flow_meter.mkdir \
	   flow_meter.sup \
	   hoststats.mkdir \
	   hoststats.sup \
	   ipblacklistfilter.mkdir \
	   ipblacklistfilter.sup \
	   ipfixcol.sup \
	   multopt.mkdir \
	   multopt.sup \
	   prepare_default_config.sh \
	   set_default_nic.sh \
	   supervisor.mkdir \
	   traffic_repeater.sup \
	   voipfrauddetection.mkdir \
	   voipfrauddetection.sup \
	   vportscandetector.mkdir \
	   vportscandetector.sup \
	   supervisor_config_template.xml \
	   warning.sup \
	   README.md

EXTRA_DIST=ipfixcol-startup.xml ${config_files}

BUILT_SOURCES=supervisor_config.xml
sysconf_DATA = supervisor_config.xml ipfixcol-startup.xml
bin_SCRIPTS=prepare_default_config.sh set_default_nic.sh

pkgdata_DATA=${config_files}

supervisor_config.xml: ${config_files}
	bash ./prepare_default_config.sh configonly

clean-local:
	@rm -f ${BUILT_SOURCES}

install-exec-hook:
	bash ./prepare_default_config.sh install 2> /dev/null || echo "Creation of /data failed. Probably a permission issue."

