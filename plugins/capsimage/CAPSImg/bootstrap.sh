#!/bin/sh -e
echo "Boostrapping libcapsimage..."
rm -Rf autom4te.cache
echo "Running autoheader"
autoheader
echo "Running autoconf"
autoconf
echo "Bootstrap done, you can now run ./configure"
