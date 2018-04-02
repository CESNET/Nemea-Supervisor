#!/bin/bash

msg() {
	echo -e "\e[32m[*] $1\e[0m"
}

SCHEMA='nemea-test-1'
THIS_DIR="$(dirname $0)"
TESTS=( test_conf test_inst_control test_module test_run_changes test_stats test_supervisor test_utils )

msg "Installing testing YANG schema '${SCHEMA}'"
sysrepoctl --install --yang "${THIS_DIR}/yang/${SCHEMA}.yang" \
	--owner $(whoami):$(whoami) --permissions 644 || exit 1

msg 'Building Makefile'
cmake .

msg "Compiling tests"
cmake --build .

for test_case in "${TESTS[@]}"; do
  msg "Running test '${test_case}'"
  ./${test_case} && rm ${test_case}
done

msg "Cleaning up"
sysrepoctl --uninstall --module=${SCHEMA} 2>/dev/null
rm -r CMakeFiles CMakeCache.txt cmake_install.cmake
