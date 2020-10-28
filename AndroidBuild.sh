#!/bin/sh


LOCAL_PATH=`dirname $0`
LOCAL_PATH=`cd $LOCAL_PATH && pwd`

JOBS=4

ln -sf libsdl_ttf.so $LOCAL_PATH/../../../obj/local/$1/libSDL_ttf.so
ln -sf libguichan.so $LOCAL_PATH/../../../obj/local/$1/libguichan_sdl.so

mkdir src-$1
mkdir src-$1/archivers
mkdir src-$1/archivers/7z
mkdir src-$1/archivers/dms
mkdir src-$1/archivers/lha
mkdir src-$1/archivers/lzx
mkdir src-$1/archivers/mp2
mkdir src-$1/archivers/wrp
mkdir src-$1/archivers/zip
mkdir src-$1/jit
mkdir src-$1/machdep
mkdir src-$1/osdep
mkdir src-$1/osdep/caps
mkdir src-$1/osdep/gui
mkdir src-$1/sounddep

ln -sr src/* src-$1
ln -sr src/archivers/* src-$1/archivers
ln -sr src/archivers/7z/* src-$1/archivers/7z
ln -sr src/archivers/dms/* src-$1/archivers/dms
ln -sr src/archivers/lha/* src-$1/archivers/lha
ln -sr src/archivers/lzx/* src-$1/archivers/lzx
ln -sr src/archivers/mp2/* src-$1/archivers/mp2
ln -sr src/archivers/wrp/* src-$1/archivers/wrp
ln -sr src/archivers/zip/* src-$1/archivers/zip
ln -sr src/jit/* src-$1/jit
ln -sr src/machdep/* src-$1/machdep
ln -sr src/osdep/* src-$1/osdep
ln -sr src/osdep/caps/* src-$1/osdep/caps
ln -sr src/osdep/gui/* src-$1/osdep/gui
ln -sr src/sounddep/* src-$1/sounddep

env LDFLAGS=-L`pwd`/../../../obj/local/$1 \
../setEnvironment-$1.sh sh -c "make -j$JOBS arch=$1" && mv -f uae4arm-$1  libapplication-$1.so

