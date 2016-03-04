EXTRA_DIST=amplification_detection.mkdir \
	   amplification_detection.sup \
	   flow_meter.mkdir \
	   flow_meter.sup \
	   hoststats.mkdir \
	   hoststats.sup \
	   ipblacklistfilter.mkdir \
	   ipblacklistfilter.sup \
      ipfixcol.sup \
	   prepare_default_config.sh \
	   set_default_nic.sh \
	   supervisor.mkdir \
	   traffic_repeater.sup \
	   voipfrauddetection.mkdir \
	   voipfrauddetection.sup \
	   vportscandetector.mkdir \
	   vportscandetector.sup \
	   README.md

BUILT_SOURCES=supervisor_config.xml
sysconf_DATA = supervisor_config.xml
bin_SCRIPTS=prepare_default_config.sh set_default_nic.sh

pkgdata_DATA=${EXTRA_DIST}

supervisor_config.xml: ${EXTRA_DIST}
	bash ./prepare_default_config.sh configonly

clean-local:
	@rm -f ${BUILT_SOURCES}

install-exec-hook:
	bash ./prepare_default_config.sh install 2> /dev/null || echo "Creation of /data failed. Probably a permission issue."

