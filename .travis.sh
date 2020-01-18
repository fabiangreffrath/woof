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
	autoreconf -fiv
	./configure --enable-werror
	make -j4
	make install DESTDIR=/tmp/whatever
	make dist
fi
