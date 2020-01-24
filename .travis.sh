#!/bin/sh
if [ "$ANALYZE" = "true" ] ; then
	cppcheck --error-exitcode=1 -j2 -DRANGECHECK -DDOGS -DBETA -ISource Source 2> stderr.txt
	RET=$?
	if [ -s stderr.txt ]
	then
		cat stderr.txt
	fi
	exit $RET
else
	set -e
	mkdir build && cd build
	cmake ..
	make
	make install DESTDIR=/tmp/whatever
	make package
fi
