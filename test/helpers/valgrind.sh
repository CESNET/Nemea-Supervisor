#!/bin/bash

dir=$(dirname $0)/../..

valgrind --leak-check=full --show-leak-kinds=all --suppressions="$dir/.valgrind-suppressions.txt" "$@"
