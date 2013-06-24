#!/bin/bash

LIBTOOL="libtool --tag=CC"
$LIBTOOL --mode=compile cc -c lua_bufflib.c -o bufflib.lo
$LIBTOOL --mode=link cc -module -rpath /usr/local/lib/lua/5.1 -o bufflib.la bufflib.lo
mv .libs/bufflib.so.0.0.0 bufflib.so
echo You can now move bufflib.so to your package.cpath.
