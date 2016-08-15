#!/bin/bash

#set -e : exit when error
set -e
routingp_pid=`ps  | awk '/routingp/{print $1}'`


kill $routingp_pid

