#!/bin/bash

dir=$(dirname $0)/../../yang

sysrepoctl --uninstall --module=nemea
sysrepoctl --install --yang $dir/nemea.yang --owner $(whoami):$(whoami) --permissions 644
sysrepocfg --import=$dir/nemea-staas-startup-config.xml --datastore startup nemea

