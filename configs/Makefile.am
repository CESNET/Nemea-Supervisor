pkgdata_dir = $(pkgdatadir)
detectors_dir = $(pkgdata_dir)/detectors
datasources_dir = $(pkgdata_dir)/data-sources
reporters_dir = $(pkgdata_dir)/reporters
loggers_dir = $(pkgdata_dir)/loggers
others_dir = $(pkgdata_dir)/others

pkgdata__DATA = amplification_detection.mkdir \
	   dnstunnel_detection.mkdir \
	   flow_meter.mkdir \
	   hoststats.mkdir \
	   ipblacklistfilter.mkdir \
	   multopt.mkdir \
	   prepare_default_config.sh \
	   set_default_nic.sh \
	   supervisor.mkdir \
	   voipfrauddetection.mkdir \
	   vportscandetector.mkdir \
	   warning.sup \
	   template.sup \
	   example_with_comments.xml \
	   README.md

others__DATA = others/multopt.sup \
       others/traffic_repeater.sup

detectors__DATA = detectors/amplification_detection.sup \
	   detectors/dnstunnel_detection.sup \
	   detectors/hoststats.sup \
	   detectors/ipblacklistfilter.sup \
	   detectors/voipfrauddetection.sup \
	   detectors/vportscandetector.sup

datasources__DATA = data-sources/flow_meter.sup \
	   data-sources/ipfixcol.sup

reporters__DATA = reporters/amplification_reporter.sup \
	   reporters/dnstunnel_reporter.sup \
	   reporters/hoststats_reporter.sup \
	   reporters/ipblacklistfilter_reporter.sup \
	   reporters/voip_fraud_reporter.sup \
	   reporters/vportscan_reporter.sup

loggers__DATA = loggers/amplification_logger.sup \
	   loggers/dnstunnel_logger.sup \
	   loggers/hoststats_logger.sup \
	   loggers/ipblacklistfilter_logger.sup \
	   loggers/voip_fraud_logger.sup \
	   loggers/vportscan_aggr_logger.sup \
	   loggers/flowmeter_logger.sup


EXTRA_DIST = ipfixcol-startup.xml supervisor_config_template.xml ${pkgdata__DATA} ${detectors__DATA} ${datasources__DATA} ${others__DATA} ${reporters__DATA} ${loggers__DATA}

sysconf_DATA = supervisor_config_template.xml ipfixcol-startup.xml
bin_SCRIPTS=prepare_default_config.sh set_default_nic.sh

clean-local:
	@rm -f ${BUILT_SOURCES}

install-exec-hook:
	bash ./prepare_default_config.sh install 2> /dev/null || echo "Creation of /data failed. Probably a permission issue."

