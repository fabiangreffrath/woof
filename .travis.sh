#!/bin/sh
if [ "$ANALYZE" = "true" ] ; then
	cppcheck --error-exitcode=1 -j2 -DRANGECHECK -ISource Source toolsrc 2> stderr.txt
	RET=$?
	if [ -s stderr.txt ]
	then
		cat stderr.txt
	fi
	exit $RET
else
	set -e
	rm -rf build/ CMakeCache.txt CMakeFiles/
	mkdir build && cd build
	cmake -G "Ninja" "$CROSSRULE" .. -DENABLE_WERROR=ON
	ninja -v
	DESTDIR=/tmp/whatever ninja -v install/strip
	ninja -v package
fi
