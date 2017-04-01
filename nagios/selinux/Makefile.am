EXTRA_DIST_FILES=nemea_nrpe.te

.PHONY: selinux
selinux: nemea_nrpe.pp

nemea_nrpe.mod: nemea_nrpe.te
	checkmodule -M -m -o $@ $<

nemea_nrpe.pp: nemea_nrpe.mod
	semodule_package -o $@ -m $<

clean-local:
	rm -f *.pp *.mod

