#!/bin/bash
set -e
ulimit -n 10240
ulimit -c 0

# When running as root, run the compiler as the owner of the working directory
# so that the output file on the host is not owned by root.
if [[ "$(id -u)" == "0" ]]; then
    owner_uid="$(stat -c %u .)"
    owner_gid="$(stat -c %g .)"

    if [[ "${owner_uid}" != "0" ]]; then
        exec setpriv --reuid="${owner_uid}" --regid="${owner_gid}" --clear-groups \
            env HOME=/tmp python3 /opt/prod/src/compile.py "$@"
    fi
fi

python3 /opt/prod/src/compile.py "$@"
