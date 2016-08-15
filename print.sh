#!/bin/bash

#set -e : exit when error
set -e
routingp_pid=`ps  | awk '/routingp/{print $1}'`

echo $#
echo $0
echo $$
if [ ! $routingp_pid ];then
  echo "routingp failed to run"
  exit
else
  echo $routingp_pid
  kill -USR1 $routingp_pid
fi

#kill $routingp_pid
