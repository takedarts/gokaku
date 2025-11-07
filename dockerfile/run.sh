#!/bin/bash
set -e
ulimit -n 10240
ulimit -c unlimited

. /opt/venv/bin/activate
python /opt/gokaku/src/run.py $@
