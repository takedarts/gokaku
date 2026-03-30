#!/bin/bash
set -e
ulimit -n 10240
ulimit -c 0

python3 /opt/gokaku/src/run.py "$@"
