[![Build Status](https://travis-ci.org/Choonster/lua_bufflib.png)](https://travis-ci.org/Choonster/lua_bufflib)

lua\_bufflib
===========
lua\_bufflib is a library that provides string buffers for Lua 5.1 and 5.2. The buffer code is largely based on Lua 5.2's [luaL_Buffer][] code.



Installation
============
lua\_bufflib can be installed through LuaRocks using `luarocks install lua_bufflib`.

If you prefer to compile it yourself, just link lua_bufflib.c with Lua and copy the shared library to your `package.cpath`.

I've included a Visual Studio 2012 solution that can be used to compile lua_bufflib on Windows.
The solution includes predefined configurations for Lua 5.1, 5.2 and LuaJIT; but you may need to change the include directories (Properties > C/C++ > General > Additional Include Directories) and the Lua import library path (Properties > Linker > Input > Additional Dependencies) to match your setup.



Documentation
=============
Documentation can be found [here](http://choonster.github.io/lua_bufflib/)

[luaL_Buffer]: http://www.lua.org/manual/5.2/manual.html#luaL_Buffer