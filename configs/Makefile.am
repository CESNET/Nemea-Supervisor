detectorsdir = $(pkgdatadir)/detectors
datasourcesdir = $(pkgdatadir)/data-sources
loggersdir = $(pkgdatadir)/loggers
othersdir = $(pkgdatadir)/others

pkgdata_DATA = amplification_detection.mkdir \
	   dnstunnel_detection.mkdir \
	   booterfilter.mkdir \
	   brute_force_detector.mkdir \
	   brute_force_hist_detector.mkdir \
	   flow_meter.mkdir \
	   hoststats.mkdir \
	   haddrscandetector.mkdir \
	   ipblacklistfilter.mkdir \
	   ipv6stats.mkdir \
	   miner_detector.mkdir \
	   multopt.mkdir \
	   prepare_default_config.sh \
	   set_default_nic.sh \
	   sip_bf_detector.mkdir \
	   supervisor.mkdir \
	   voipfrauddetection.mkdir \
	   vportscandetector.mkdir \
	   warning.sup \
	   template.sup \
	   config_example.xml \
	   README.md

others_DATA = others/multopt.sup \
       others/traffic_repeater.sup \
       others/ipv6stats.sup

detectors_DATA = detectors/amplification_detection.sup \
	   detectors/brute_force_detector.sup \
	   detectors/brute_force_hist_detector.sup \
	   detectors/dnstunnel_detection.sup \
	   detectors/hoststats.sup \
	   detectors/haddrscandetector.sup \
	   detectors/ipblacklistfilter.sup \
	   detectors/miner_detector.sup \
	   detectors/sip_bf_detector.sup \
	   detectors/voipfrauddetection.sup \
	   detectors/vportscandetector.sup

datasources_DATA = data-sources/flow_meter.sup \
	   data-sources/ipfixcol.sup

loggers_DATA = loggers/amplification_logger.sup \
	   loggers/brute_force_hist_logger.sup \
	   loggers/brute_force_logger.sup \
	   loggers/dnstunnel_logger.sup \
	   loggers/flowmeter_logger.sup \
	   loggers/haddrscan_aggr_logger.sup \
	   loggers/hoststats_logger.sup \
	   loggers/ipblacklistfilter_logger.sup \
	   loggers/miner_detector_logger.sup \
	   loggers/sip_bf_logger.sup \
	   loggers/voip_fraud_logger.sup \
	   loggers/vportscan_aggr_logger.sup



EXTRA_DIST = ipfixcol-startup.xml supervisor_config_template.xml ${pkgdata_DATA} ${detectors_DATA} ${datasources_DATA} ${others_DATA} ${loggers_DATA}

sysconf_DATA = supervisor_config_template.xml ipfixcol-startup.xml
bin_SCRIPTS=prepare_default_config.sh set_default_nic.sh

clean-local:
	@rm -f ${BUILT_SOURCES}

install-data-hook:
	echo -e "\n\nRun script for creation of directories and setting permisions:\nprepare_default_config.sh install\n\n"

