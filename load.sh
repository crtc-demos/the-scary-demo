#!/bin/sh
if [ "$HOSTNAME" != batbox ] && [ "$HOSTNAME" != six ] && [ "$HOSTNAME" != octopus ]; then
  exec ssh batbox "cd /home/jules/rex-home/code/cubedemo; ./load.sh \"$@\""
fi
DOL=$1
if [ ! "$DOL" ]; then
  DOL=demo.dol
fi
psolore -i 192.168.2.251 "$DOL"
netcat 192.168.2.254 2000
