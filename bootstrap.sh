#!/bin/sh

THIS_DIR="$(dirname $0)"

tools=( sysrepoctl sysrepocfg )
for tool in ${tools[@]}; do
    which $tool &>/dev/null
    if [ $? -ne 0 ]; then
        echo "${tool} from sysrepo project is missing, please install sysrepo before you continue!"
        exit 1
    fi
done

sudo sysrepoctl --install --yang "${THIS_DIR}/yang/nemea.yang"
sysrepocfg --import="${THIS_DIR}/yang/data/nemea-staas-startup-config.data.xml" \
            --format=xml --datastore startup nemea

echo "Successfully bootstraped."