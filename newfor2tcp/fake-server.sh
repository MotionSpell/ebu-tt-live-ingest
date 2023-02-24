#!/usr/bin/env bash
# Under Debian: apt install ucspi-tcp
set -eou pipefail
serverPid=""

readonly tmpDir=/tmp/newfor-fake-server-$$
trap 'rm -rf $tmpDir ; test -z "$serverPid" || kill $serverPid' EXIT
mkdir -p "$tmpDir"

readonly scriptDir=$(dirname $0)
g++ $scriptDir/newfor-server-cgi.cpp -o $tmpDir/fake-newfor-server-cgi.exe
echo "Newfor server ready: http://127.0.0.1:9000/"
tcpserver -D 127.0.0.1 9000 $tmpDir/fake-newfor-server-cgi.exe &
serverPid=$!
wait $serverPid
