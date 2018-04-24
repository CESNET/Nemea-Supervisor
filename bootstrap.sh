#!/bin/sh

THIS_DIR="$(dirname $0)"

sysrepoctl --install --yang "${THIS_DIR}/yang/nemea.yang"
sysrepocfg --import="${THIS_DIR}/yang/data/nemea-staas-startup-config.data.xml" \
            --format=xml --datastore startup nemea
