#!/bin/sh

CURDIR=`dirname $0`
# include test framework
. $CURDIR/test_framework.inc

if [[ -r ./$CURDIR/test_variables.rc ]]  ; then
   .  ./$CURDIR/test_variables.rc
else
  echo "  /!\\ Please make test's configuration in  $CURDIR/test_variables.rc"
  exit 1 
fi

# The script uses "su", it has to be run as root
if [[ `id -u` != 0 ]] ; then
  echo "  /!\\ This script must be run as root"
  exit 1 
fi

######################## INCLUDE MODULES HERE ####################
.  $CURDIR/allfs/testsuite.inc

# syntax: ONLY=2,3 ./run_test.sh [-j] <test_dir>

######################## DEFINE TEST LIST HERE ####################

run_allfs

