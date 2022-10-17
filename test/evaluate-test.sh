#!/bin/bash

test_name=$1
rc=$2

if [ $rc -eq 2 ]; then
	result="UNSUPPORTED"
else
	if [ $rc -eq 0 ]; then
		result="PASS"
	else
		result="FAIL"
	fi
fi

echo -e "    $result: $test_name"
