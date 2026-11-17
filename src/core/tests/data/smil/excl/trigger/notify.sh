#!/usr/bin/env bash
# Sends notify(<event>) to a running player for excl/trigger/7_notify.smil.
# Usage: ./notify.sh [event] [host:port]    defaults: heidewitzka localhost:8080
set -euo pipefail

event=${1:-heidewitzka}
host=${2:-localhost:8080}

answer=$(curl -sS -f -d "grant_type=password&username=admin&password=" "http://$host/v2/oauth2/token") ||
    { echo "no player answers on $host" >&2; exit 1; }
token=$(sed -nE 's/.*"access_token" *: *"([^"]+)".*/\1/p' <<<"$answer")
[ -n "$token" ] || { echo "no access_token in: $answer" >&2; exit 1; }

curl -sS -f -d "access_token=$token&smilEvent=$event" "http://$host/v2/task/notify"
echo
