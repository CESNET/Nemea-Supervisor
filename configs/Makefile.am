EXTRA_DIST=amplification_detection.mkdir \
	   amplification_detection.sup \
	   hoststats.mkdir \
	   hoststats.sup \
	   prepare_default_config.sh \
	   set_default_nic.sh \
	   supervisor.mkdir \
	   traffic_repeater.sup \
	   README.md

BUILT_SOURCES=supervisor_config.xml
sysconf_DATA = supervisor_config.xml
bin_SCRIPTS=prepare_default_config.sh set_default_nic.sh

pkgdata_DATA=${EXTRA_DIST}

supervisor_config.xml: ${EXTRA_DIST}
	bash ./prepare_default_config.sh configonly

clean-local:
	@rm -f ${BUILT_SOURCES}
