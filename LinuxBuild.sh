#!/bin/bash
#
# Builds uae4arm for the Linux PC, to try out emulation problems quickly: the
# old Pandora path on plain SDL 1.2, without the Android wrapper or the JIT.
# Needs libsdl1.2-dev libsdl-ttf2.0-dev libsdl-image1.2-dev libpng-dev
# libxml2-dev libflac-dev libmpg123-dev libmpeg2-4-dev zlib1g-dev.
#
# ./LinuxBuild.sh            build ./uae4arm-linux, logging to the console
# WITH_LOGGING= ./LinuxBuild.sh   the same without the log
# ./LinuxBuild.sh clean      remove the build
#
# The first build also unpacks AndroidData into build-linux/run; run it from
# there:  cd build-linux/run && ../../uae4arm-linux

cd "`dirname $0`" || exit 1
JOBS=`nproc`

if [ "$1" = "clean" ]; then
	rm -rf src-linux build-linux uae4arm-linux
	exit 0
fi

# guichan from the androidsdl tree, as a static library
GUICHAN=../../guichan
if [ ! -f build-linux/libguichan.a ]; then
	mkdir -p build-linux/guichan
	# contrib/ is left out: uae4arm has its own sdltruetypefont, and this one
	# pulls in android/log.h
	for f in `cd $GUICHAN && find src -name '*.cpp' -not -path 'src/contrib/*'`; do
		o=build-linux/guichan/`echo $f | tr / _`.o
		echo "$f"
		g++ -O2 -g `sdl-config --cflags` -I$GUICHAN/include -c $GUICHAN/$f -o $o &
		[ `jobs -p | wc -l` -ge $JOBS ] && wait -n 2>/dev/null
	done
	wait
	ar rcs build-linux/libguichan.a build-linux/guichan/*.o || exit 1
fi

# The log goes to the console, and this build is for finding problems, so it
# is on unless WITH_LOGGING= is given. make does not see a change of flags,
# so a change of it rebuilds everything.
WITH_LOGGING=${WITH_LOGGING-1}
if [ "`cat build-linux/logging 2>/dev/null`" != "$WITH_LOGGING" ]; then
	rm -rf src-linux uae4arm-linux
	echo "$WITH_LOGGING" > build-linux/logging
fi

# the same tree of links to src/ as AndroidBuild.sh
if [ ! -d src-linux ]; then
	for d in archivers/7z archivers/dms archivers/lha archivers/lzx archivers/mp2 \
		archivers/wrp archivers/zip jit machdep osdep/caps osdep/gui sounddep; do
		mkdir -p src-linux/$d
	done
	ln -sr src/* src-linux 2>/dev/null
	for d in archivers archivers/7z archivers/dms archivers/lha archivers/lzx \
		archivers/mp2 archivers/wrp archivers/zip jit machdep osdep osdep/caps \
		osdep/gui sounddep; do
		ln -sr src/$d/* src-linux/$d 2>/dev/null
	done
fi
# files added to src/ since the tree was made
for d in . archivers jit machdep osdep osdep/gui sounddep; do
	for f in src/$d/*; do
		[ -f "$f" ] && [ ! -e "src-linux/$d/`basename $f`" ] && ln -sr "$f" "src-linux/$d/"
	done
done

make -j$JOBS arch=linux KEEPSYMBOLS=1 WITH_LOGGING=$WITH_LOGGING || exit 1

# somewhere to run it, with the data the Android app unpacks on first start
if [ ! -d build-linux/run/data ]; then
	mkdir -p build-linux/run/kickstarts build-linux/run/savestates build-linux/run/screenshots
	(cd build-linux/run && unzip -oq ../../AndroidData/data*.zip)
fi
