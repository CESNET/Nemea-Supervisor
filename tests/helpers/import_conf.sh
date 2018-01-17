#!/bin/bash

YANG_MODULE="nemea-test-1"
dir="$(dirname "$0")"

if [ "$1" = "-s" ]; then
  # Load startup config
  sysrepocfg --import="$dir/../yang/$2" --datastore startup $YANG_MODULE
elif [ "$1" = "-r" ]; then
  # Load running config
  sysrepocfg --import="$dir/../yang/$2" --datastore running $YANG_MODULE
fi
