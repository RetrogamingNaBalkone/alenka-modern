#!/bin/sh
set -eu
# Installed application data is read-only; profiles belong to the current user.
if [ -n "${XDG_DATA_HOME:-}" ]; then
    data_home=$XDG_DATA_HOME
else
    data_home=${HOME:?HOME is not set}/.local/share
fi
case "$data_home" in /*) ;; *) echo 'XDG_DATA_HOME must be an absolute path' >&2; exit 1;; esac
mkdir -p "$data_home/alenka-modern"
cd "$data_home/alenka-modern"
exec /usr/lib/alenka-modern/alenka --data /usr/share/alenka-modern "$@"
