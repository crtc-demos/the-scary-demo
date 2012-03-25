#!/bin/sh
DOL=$1
if [ ! "$DOL" ]; then
  DOL=demo.dol
fi
export WIILOAD=tcp:192.168.1.66
/opt/devkitpro/devkitPPC/bin/wiiload "$DOL"
echo "Waiting for demo to set itself up a network connection..."
sleep 1
netcat 192.168.1.66 2000
