#!/bin/bash

dir="$(dirname "$0")"
test_pf="-test-1"
test_module_yang="$dir/../yang/nemea$test_pf.yang"
test_trap_yang="$dir/../yang/trap-interfaces$test_pf.yang"

cp "$dir/../../yang/nemea.yang" "$test_module_yang"
cp "$dir/../../yang/trap-interfaces.yang" "$test_trap_yang"

sed -i 's/module nemea/module nemea-test-1/' "$test_module_yang"
sed -i 's/namespace "urn:ietf:params:xml:ns:yang:nemea"/namespace "urn:ietf:params:xml:ns:yang:nemea-test-1"/' "$test_module_yang"
sed -i 's/prefix nemea/prefix "nemea-test-1"/g' "$test_module_yang"
sed -i 's/include trap-interfaces/include trap-interfaces-test-1/' "$test_module_yang"

sed -i 's/submodule trap-interfaces/submodule trap-interfaces-test-1/g' "$test_trap_yang"
sed -i 's/belongs-to nemea/belongs-to nemea-test-1/g' "$test_trap_yang"
sed -i 's/prefix nemea/prefix nemea-test-1/g' "$test_trap_yang"
sed -i 's/nemea:nemea-key-name/nemea-test-1:nemea-key-name/g' "$test_trap_yang"
sed -i 's/nemea:full-unix-path/nemea-test-1:full-unix-path/g' "$test_trap_yang"

