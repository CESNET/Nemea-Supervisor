#!/bin/sh

THIS_DIR="$(dirname $0)"

tools=( sysrepoctl sysrepocfg )
for tool in ${tools[@]}; do
    command -v $tool &>/dev/null
    if [ $? -ne 0 ]; then
        echo "${tool} from sysrepo project is missing, please install sysrepo before you continue!"
        exit 1
    fi
done

if [[ "$(whoami)" != "root" ]]; then
    sudo sysrepoctl --install --yang "${THIS_DIR}/yang/nemea.yang"
else
    sysrepoctl --install --yang "${THIS_DIR}/yang/nemea.yang"
fi
if [[ $? -ne 0 ]]; then  exit 1; fi

echo "Successfully bootstraped."