#!/bin/bash

msg() {
	echo -e "\e[32m[*] $1\e[0m"
}

SCHEMA='nemea-test-1'
THIS_DIR="$(dirname $0)"
TESTS=( test_conf test_inst_control test_module test_run_changes test_stats test_supervisor test_utils )
#TESTS=( test_inst_control test_module test_run_changes test_stats test_supervisor test_utils )


msg 'Building Makefile'
cmake .

msg "Compiling tests"
cmake --build .

for test_case in "${TESTS[@]}"; do
  msg "Reinstalling testing YANG schema '${SCHEMA}'"
  sudo sysrepoctl --uninstall -m nemea-test-1 1>/dev/null 2>/dev/null
  sudo sysrepoctl --install --yang "${THIS_DIR}/yang/${SCHEMA}.yang" \
  	--owner $(whoami):$(whoami) --permissions 644 || exit 1

  msg "Running '${test_case}'"
  ./${test_case} #&& rm ${test_case}
done

#msg "Cleaning up"
#sysrepoctl --uninstall --module=${SCHEMA} 2>/dev/null
#rm -r CMakeFiles CMakeCache.txt cmake_install.cmake
