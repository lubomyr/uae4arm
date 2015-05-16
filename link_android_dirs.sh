#!/bin/bash

cd ./src

# Link to our config.h
if ! [ -e "config.h" ]; then
  ln -s od-sdl-guichan/config.h config.h
fi

# Link to our sysconfig.h
if ! [ -e "sysconfig.h" ]; then
  ln -s od-sdl-guichan/sysconfig.h sysconfig.h
fi

# Link to our target.h
if ! [ -e "target.h" ]; then
  ln -s od-sdl-guichan/target.h target.h
fi

# Link od-pandora to osdep
if ! [ -d "osdep" ]; then
  ln -s od-sdl-guichan osdep
fi

# We use SDL-threads, so link td-sdl to threaddep
if ! [ -d "threaddep" ]; then
  ln -s td-sdl threaddep
fi

# Link md-arm to machdep
if ! [ -d "machdep" ]; then
  ln -s md-arm machdep
fi

# Link od-sound to sounddep
if ! [ -d "sounddep" ]; then
  ln -s sd-sdl sounddep
fi

cd ..
