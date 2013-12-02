#!/bin/bash
#Author: Erwin Meza <emezav@gmail.com>

default=$2

program=$(which $1 2>/dev/null)

if [ "x$program" == "x" ]; then
   program=$default   
fi

echo $program