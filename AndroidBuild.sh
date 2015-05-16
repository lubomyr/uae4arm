#!/bin/sh


LOCAL_PATH=`dirname $0`
LOCAL_PATH=`cd $LOCAL_PATH && pwd`

JOBS=1

ln -sf libsdl_ttf.so $LOCAL_PATH/../../../obj/local/armeabi-v7a/libSDL_ttf.so
ln -sf libguichan.so $LOCAL_PATH/../../../obj/local/armeabi-v7a/libguichan_sdl.so
ln -sf libsdl_ttf.so $LOCAL_PATH/../../../obj/local/armeabi-v7a-hard/libSDL_ttf.so
ln -sf libguichan.so $LOCAL_PATH/../../../obj/local/armeabi-v7a-hard/libguichan_sdl.so

#../setEnvironment.sh sh -c "make -j1 " && mv -f uae4all libapplication.so

#if [ "$1" = armeabi ]; then
#../setEnvironment.sh sh -c "make --makefile=Makefile.arm -j1" && mv -f uae4all libapplication.so
#fi
#if [ "$1" = armeabi-v7a ]; then
#env CFLAGS="-funsafe-math-optimizations -Ofast" \
#../setEnvironment-armeabi-v7a.sh sh -c "make --makefile=Makefile.arm-v7a -j$JOBS" && mv -f uae4all-v7a libapplication-armeabi-v7a.so
#fi
#if [ "$1" = armeabi-v7a-hard ]; then
#env CFLAGS="-funsafe-math-optimizations -O2" \
#../setEnvironment-armeabi-v7a-hard.sh sh -c "make --makefile=Makefile.arm-v7a-hard -j$JOBS" && mv -f uae4all-v7a-hard libapplication-armeabi-v7a-hard.so
#fi
if [ "$1" = armeabi-v7a ]; then
env CFLAGS="-mfpu=neon -O3" \
../setEnvironment-armeabi-v7a.sh sh -c "make --makefile=Makefile.android.armv7a-neon -j$JOBS" && mv -f uae4arm libapplication-armeabi-v7a.so
fi
if [ "$1" = armeabi-v7a-hard ]; then
env CFLAGS="-mfpu=neon -O3" \
../setEnvironment-armeabi-v7a-hard.sh sh -c "make --makefile=Makefile.android.armv7a-neon-hardfp -j$JOBS" && mv -f uae4arm libapplication-armeabi-v7a.so
fi
