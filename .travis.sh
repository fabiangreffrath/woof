#!/bin/sh
if [ "$ANALYZE" = "true" ]
then
	cppcheck --error-exitcode=1 -j2 -DRANGECHECK -D_WIN32 -Isrc src toolsrc 2> stderr.txt
	RET=$?
	if [ -s stderr.txt ]
	then
		cat stderr.txt
	fi
	exit $RET
else
	set -e
	export VERBOSE=1
	rm -rf build/ CMakeCache.txt CMakeFiles/
	mkdir build && cd build
	if [ -n "$RELEASE" ]
	then
		BUILD_TYPE="-DCMAKE_BUILD_TYPE=Release"
	fi
	cmake -G "Ninja" "$BUILD_TYPE" "$CROSSRULE" .. -DENABLE_WERROR=ON
	ninja -v
	DESTDIR=/tmp/whatever ninja -v install/strip
	ninja -v package
fi
