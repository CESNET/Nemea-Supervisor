#!/bin/bash

msg() {
	echo -e "\e[32m[*] $1\e[0m"
}

SR_MODULE='nemea-test-1'
EXE='test-45923'
THIS_DIR="$(dirname $0)"
STARTUP_CONFIGS=(${SR_MODULE}-startup-config-1.xml)


#msg "Generating Makefile"
#cmake .
#msg "Compiling tests"
#make

# LOAD TESTING SYSREPO SCHEMA
#  it's the same as nemea.yang schema, but with different name so it doesn't
#  mess with configuration in place

sysrepoctl --uninstall --module=${SR_MODULE} 2>/dev/null

msg "Installing testing yang scheme"
sysrepoctl --install --yang "${THIS_DIR}/yang/${SR_MODULE}.yang" \
	--owner $(whoami):$(whoami) --permissions 644 || exit 1
msg "Schema installed"
exit 0

for config in ${STARTUP_CONFIGS[@]}; do
	msg "Importing config: ${config}"
	sysrepocfg --import="${THIS_DIR}/yang/${config}" \
		--datastore startup ${SR_MODULE} || exit 1
	
	msg "Running test conf_test"
	#"$THIS_DIR"/conf_test

done



make clean 2>/dev/null
#sysrepoctl --uninstall --module=${SR_MODULE}
