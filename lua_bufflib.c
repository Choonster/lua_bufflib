/*
** A library for string buffers in Lua.
** Copyright (c) 2013 Choonster
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
** THE SOFTWARE.
**
** Major portions taken verbatim or adapted from Lua 5.2's auxilary library.
** Copyright (C) 1994-2013 Lua.org, PUC-Rio. See Copyright Notice at the page below:
**     http://www.lua.org/license.html
*/

/**
A library for string buffers in Lua.

The buffer code in this library is largely adapted from Lua 5.2's luaL\_Buffer code.
The main difference is that Buffers store their contents in the registry instead of the stack when their length exceeds LUAL\_BUFFERSIZE.
This prevents the C char array holding the current contents from being garbage collected until a larger array is required or the Buffer is reset or garbage collected.
You don't need to know any of this to use the library, it's just extra information for people curious about the implementation.


Just like regular strings in Lua, string buffers can contain embedded nulls (\0).


Similar to Lua's string library, most Buffer methods can be called as `buff:method(...)` or `bufflib.method(buff, ...)` (where `buff` is a Buffer).
Note that not all methods use the same name in the Buffer metatable and the `bufflib` table.
The primary examples of this are the metamethods, which use the required metamethod names in the metatable and more descriptive names in the `bufflib` table (e.g. the `__len` metamethod is the same as `bufflib.length`).


In addition to the functions shown here, you can call any method from the global `string` table (not just functions from the string library) on a Buffer (either as a method or a function from the `bufflib` table) by prefixing the name with `s_`.
When you call a Buffer method with the `s_` prefix, it calls the equivalent `string` function with the Buffer's contents as the first argument followed by any other arguments supplied to the method. None of these methods modify the original Buffer.

For example, `bufflib.s_gsub(buff, ...)` and `buff:s_gsub(...)` are both equivalent to `str:gsub(...)` (where `buff` is a Buffer and `str` is the Buffer's contents as a string).

Buffers define metamethods for equality (==), length (#), concatenation (..) and tostring(). See the documentation of each metamethod for details.

@module bufflib
*/

#include <string.h>

#include "lua.h"
#include "lauxlib.h"

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT extern
#endif

/* The registry key used to store the Buffer metatable */
#define BUFFERTYPE "bufflib_buffer"

/* The prefix used to access string library methods on Buffers */
#define STRINGPREFIX "s_"
#define STRINGPREFIXLEN 2

/*
	Compatiblity with 5.1.
	The functions in this section were copied from Lua 5.2's auxilary library.
*/
#if LUA_VERSION_NUM < 502
#define LIBNAME "bufflib"

#define luaL_setfuncs(L, l, n) luaL_register(L, NULL, (l))
#define luaL_newlib(L, l) luaL_register(L, LIBNAME, (l))

static void luaL_setmetatable (lua_State *L, const char *tname) {
  luaL_getmetatable(L, tname);
  lua_setmetatable(L, -2);
}

static void *luaL_testudata(lua_State *L, int ud, const char *tname) {
  void *p = lua_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (lua_getmetatable(L, ud)) {  /* does it have a metatable? */
      luaL_getmetatable(L, tname);  /* get correct metatable */
      if (!lua_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      lua_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}

static const char *luaL_tolstring (lua_State *L, int idx, size_t *len) {
  if (!luaL_callmeta(L, idx, "__tostring")) {  /* no metafield? */
    switch (lua_type(L, idx)) {
      case LUA_TNUMBER:
      case LUA_TSTRING:
        lua_pushvalue(L, idx);
        break;
      case LUA_TBOOLEAN:
        lua_pushstring(L, (lua_toboolean(L, idx) ? "true" : "false"));
        break;
      case LUA_TNIL:
        lua_pushliteral(L, "nil");
        break;
      default:
        lua_pushfstring(L, "%s: %p", luaL_typename(L, idx),
                                            lua_topointer(L, idx));
        break;
    }
  }
  return lua_tolstring(L, -1, len);
}

#endif

/**
A class representing a string buffer.
@type Buffer
*/
typedef struct Buffer {
	int ref; /* position of the buffer's userdata in the registry */
	char *b;  /* buffer address */
	size_t size;  /* buffer size */
	size_t n;  /* number of characters in buffer */
	lua_State *L;
	char initb[LUAL_BUFFERSIZE];  /* initial buffer */
} Buffer;

/* Add s to the Buffer's character count */
#define addsize(B,s)       ((B)->n += (s))

/* If the Buffer is storing its contents as a userdata in the registry, unreference it and allow it to be collected */
#define buff_unref(L, B) if ((B)->ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, (B)->ref)

/*
	Prepares the Buffer for a string of length sz to be added and returns a pointer that the string can be copied into with memcpy().
	If there isn't enough space in the current char array, creates a new one as a userdata and stores it in the registry.
	Releases the previously stored userdata (if any), allowing it be garbage collected.
*/
static char *prepbuffsize (Buffer *B, size_t sz) {
	lua_State *L = B->L;
	if (B->size - B->n < sz) {  /* not enough space? */
		char *newbuff;
		size_t newsize = B->size * 2;  /* double buffer size */
		if (newsize - B->n < sz)  /* not big enough? */
			newsize = B->n + sz;
		if (newsize < B->n || newsize - B->n < sz)
			luaL_error(L, "buffer too large");
		/* create larger buffer */
		newbuff = (char *)lua_newuserdata(L, newsize * sizeof(char));
		/* move content to new buffer */
		memcpy(newbuff, B->b, B->n * sizeof(char));
		buff_unref(L, B);  /* remove old buffer */
		B->ref = luaL_ref(L, LUA_REGISTRYINDEX);
		B->b = newbuff;
		B->size = newsize;
	}
	return &B->b[B->n];
}

/*
	Initialise a Buffer for use with the given lua_State.
	The Buffer must only be used with the lua_State it was initialised with.
*/
static void buffinit(lua_State *L, Buffer *B) {
	B->L = L;
	B->b = B->initb;
	B->n = 0;
	B->size = LUAL_BUFFERSIZE;
	B->ref = LUA_NOREF;
}

/*
	Creates a new Buffer as a userdata, sets its metatable and initialises it.
	The new Buffer will be left on the top of the stack.
*/
static Buffer *newbuffer(lua_State *L) {
	Buffer *B = (Buffer *)lua_newuserdata(L, sizeof(Buffer));
	luaL_setmetatable(L, BUFFERTYPE);
	buffinit(L, B);
	return B;
}

/* Returns a pointer to the Buffer at index i */
#define getbuffer(L, i) ((Buffer *)luaL_checkudata(L, i, BUFFERTYPE))

/* Is the value at index i a Buffer? */
#define isbuffer(L, i) (luaL_testudata(L, i, BUFFERTYPE) != NULL)

/* Call luaL_toslstring and pop the string from the stack */
#define tolstring(L, i, l) luaL_tolstring(L, i, l); lua_pop(L, 1)

/* Add string arguments to the buffer, starting from firstarg and ending at (numargs-offset). */
static void addstrings(Buffer *B, int firstarg, int offset) {
	lua_State *L = B->L;
	int numargs = lua_gettop(L) - offset;

	size_t len;
	const char *str;
	char *b;

	int i;
	for (i = firstarg; i <= numargs; i++) {
		len = -1;
		str = tolstring(L, i, &len);
		b = prepbuffsize(B, len);
		memcpy(b, str, len * sizeof(char));
		addsize(B, len);
	}
}

/*
	Add strings to the buffer, with a separator string between each one.
	Assumes the separator is argument 2 and arguments 3 to numargs are the strings to be added.
*/
static void addsepstrings(Buffer *B) {
	lua_State *L = B->L;
	int numargs = lua_gettop(L);

	size_t seplen = -1;
	const char *sep;

	size_t len = -1;
	const char *str;
	char *b;

	int i;

	sep = tolstring(L, 2, &seplen); /* Get the separator string */

	for (i = 3; i < numargs; i++) { /* For arguments 3 to (numargs-1), add the string argument followed by the separator */
		len = -1;
		str = tolstring(L, i, &len);

		b = prepbuffsize(B, len + seplen); /* Prepare the buffer for the length of the string plus the length of the separator */
		memcpy(b, str, len * sizeof(char));
		addsize(B, len);

		b += len; /* Get a pointer to the position after the string */
		memcpy(b, sep, seplen * sizeof(char));
		addsize(B, seplen);
	}

	len = -1;
	str = tolstring(L, numargs, &len); /* Add the final string */
	b = prepbuffsize(B, len);
	memcpy(b, str, len * sizeof(char));
	addsize(B, len);
}

/* Pushes the argument at index i (which should be a Buffer userdata) onto the stack to return it. */
static int pushbuffer(lua_State *L, int i) {
	lua_pushvalue(L, i);
	return 1;
}

/**
Add some strings to the @{Buffer}.
All non-string arguments are converted to strings following the same rules as the `tostring()` function.

@function add
@param ... Some values to add to the Buffer.
@treturn Buffer The Buffer object.
*/
static int bufflib_add(lua_State *L) {
	Buffer *B = getbuffer(L, 1);
	addstrings(B, 2, 0);
	return pushbuffer(L, 1);
}

/**
Add some strings to the @{Buffer}, each separated by the specified separator string.
All non-string arguments are converted to strings following the same rules as the `tostring()` function.

@function addsep
@string sep The string to use as a separator.
@param ... Some values to add to the Buffer.
@treturn Buffer The Buffer object.
*/
static int bufflib_addsep(lua_State *L) {
	Buffer *B = getbuffer(L, 1);
	addsepstrings(B);
	return pushbuffer(L, 1);
}

/**
Reset the @{Buffer} to its initial (empty) state.
If the Buffer was storing its contents in the registry, this is removed so it can be garbage collected.

@function reset
@treturn Buffer The Buffer object.
*/
static int bufflib_reset(lua_State *L) {
	Buffer *B = getbuffer(L, 1);
	buff_unref(L, B); /* If the buffer is storing its contents in the registry, remove it (and allow it to be garbage collected) before resetting */
	buffinit(L, B); /* Re-initialise the Buffer */
	return pushbuffer(L, 1);
}

/**
Converts the @{Buffer} to a string representing its current conents.
The Buffer remains unchanged.

@function __tostring
@treturn string The contents of the Buffer
*/
static int bufflib_tostring(lua_State *L) {
	Buffer *B = getbuffer(L, 1);
	lua_pushlstring(L, B->b, B->n);
	return 1;
}

/**
Metamethod for the `#` (length) operation.
Returns the length of the @{Buffer}'s current contents.

@function __len
@treturn int length
*/
static int bufflib_len(lua_State *L) {
	Buffer *B = getbuffer(L, 1);
	lua_pushinteger(L, B->n);
	return 1;
}

/**
Metamethod for the `..` (concatenation) operation.
If both arguments are @{Buffer|Buffers}, creates a new Buffer from their joined contents.
Otherwise adds a value to the Buffer, converting it to a string following the same rules as the `tostring()` function.

@function __concat
@param B A Buffer to concatenate with this one or some value to to add to it.
@treturn Buffer Either the new Buffer created from the two Buffer arguments or the Buffer that the non-Buffer argument was added to.
*/
static int bufflib_concat(lua_State *L) {
	int arg1IsBuffer = isbuffer(L, 1);
	int arg2IsBuffer = isbuffer(L, 2);

	if (arg1IsBuffer && arg1IsBuffer) { /* If both arguments are Buffers, combine them into a new Buffer */
		Buffer *destbuff = newbuffer(L);

		size_t len1 = -1, len2 = -1;
		const char *str1, *str2;
		char *b;

		str1 = tolstring(L, 1, &len1);
		str2 = tolstring(L, 2, &len2);

		b = prepbuffsize(destbuff, len1 + len2); /* Prepare the Buffer for the combined length of the strings */
		memcpy(b, str1, len1 * sizeof(char)); /* Add the first string */
		addsize(destbuff, len1);

		b += len1; /* Get a pointer to the position after the first string */
		memcpy(b, str2, len2 * sizeof(char)); /* Add the second string */
		addsize(destbuff, len2);

		return 1; /* The new Buffer is already on the stack */
	} else { /* If only one argument is a Buffer, add the non-Buffer argument to it */
		size_t len = -1;
		const char *str;
		char *b;
		int buffindex;
		Buffer *destbuff;

		if (arg1IsBuffer) {
			destbuff = getbuffer(L, 1);
			str = tolstring(L, 2, &len);
			buffindex = 1;
		} else {
			destbuff = getbuffer(L, 2);
			str = tolstring(L, 1, &len);
			buffindex = 2;
		}

		b = prepbuffsize(destbuff, len);
		memcpy(b, str, len);
		addsize(destbuff, len);

		return pushbuffer(L, buffindex);
	}
}

/**
Metamethod for the `==` (equality) operation.
Returns true if the @{Buffer|Buffers} hold the same contents, false if they don't.
The contents of each Buffer are converted to Lua strings and then the two strings are compared to obtain the result.

@function __eq
@tparam Buffer buff2 The Buffer to compare with this one.
@treturn bool equal Do the Buffers hold the same contents?
*/
static int bufflib_equal(lua_State *L){
	Buffer *buff1 = getbuffer(L, 1);
	Buffer *buff2 = getbuffer(L, 2);

	lua_pushlstring(L, buff1->b, buff1->n); /* Push the contents of each Buffer onto the stack */
	lua_pushlstring(L, buff2->b, buff2->n);

	lua_pushboolean(L, lua_rawequal(L, -1, -2)); /* Return the result of a lua_rawequal comparison of the strings */
	return 1;
}

/*
Garbage collection metamethod. This should never be called by the user, so it's not included in the documentation.
If the Buffer is storing its contents as a userdata in the registry, unreferences it; allowing it to be collected
*/
static int bufflib_gc(lua_State *L){
	Buffer *B = getbuffer(L, 1);
	buff_unref(L, B);
	return 0;
}

/**
Buffer Manipulation.

The functions in this section are available in the `bufflib` table, returned from `require"bufflib"`.

@section manipulation
*/

/**
Creates a new Buffer object, optionally adding some strings to the new Buffer.
All non-string arguments are converted to strings following the same rules as the `tostring()` function.

@function newbuffer
@param[opt] ... Some values to add to the new Buffer.
@treturn Buffer The new Buffer object.
*/
static int bufflib_newbuffer(lua_State *L){
	Buffer *B = newbuffer(L);
	addstrings(B, 1, 1); /* Pass addstrings an offset of 1 to account for the new userdata at the top of the stack. */
	return 1; /* The new Buffer is already on the stack */
}

/**
Tests whether or not the argument is a @{Buffer}.

@function isbuffer
@param arg The value to check.
@treturn bool Is this a Buffer?
*/
static int bufflib_isbuffer(lua_State *L){
	lua_pushboolean(L, isbuffer(L, 1));
	return 1;
}

/*
	Calls a function stored at upvalue 1 with the Buffer's string contents as the first argument and any other arguments passed to the function after that.
*/
static int bufflib_stringop(lua_State *L){
	Buffer *B = getbuffer(L, 1);
	int numargs = lua_gettop(L);
	
	lua_remove(L, 1); /* Remove the Buffer from the stack */
	
	lua_pushvalue(L, lua_upvalueindex(1)); /* String function to call */
	lua_insert(L, 1); /* Insert it before the arguments */
	
	lua_pushlstring(L, B->b, B->n); /* Buffer contents as a string */
	lua_insert(L, 2); /* Insert it between the function and the arguments */
	
	lua_call(L, numargs, LUA_MULTRET); /* Call the function */
	return lua_gettop(L); /* The function's return values are the only values on the stack, we return them all from this function */
}

/*
	Function for the __index metamethod.
	First it looks up the key in the Buffer metatable and returns its value if it has one.
	If the key doesn't exist in the metatable and it starts with "s_", looks up the rest of the key in the global table "string" (if it exists).
	If the key's value is a function, creates a closure of the stringop function with the string function as the first upvalue.
	This closure is stored with the original key in both the metatable and the library table and then returned.
	Returns nil if the above conditions aren't met.
*/
static int bufflib_index(lua_State *L){
	const char *key = luaL_checkstring(L, 2);
	const char *pos, *strkey;
	
	lua_getmetatable(L, 1); /* Get the Buffer's metatable */
	lua_getfield(L, -1, key); /* mt[key] */

	if (!lua_isnil(L, -1)){ /* If the key exists in the metatable, return its value */
		return 1; 
	} else {
		lua_pop(L, 1); /* Pop the nil from the stack */
	}
	
	pos = strstr(key, STRINGPREFIX);
	if (pos == NULL || (pos - key) != 0){ /* If the key doesn't start with "s_", return nil */
		lua_pushnil(L);
		return 1;
	}

	lua_getglobal(L, "string");
	if (!lua_istable(L, -1)){ /* If there's no string table, return nil */
		lua_pushnil(L);
		return 1;
	}
	
	strkey = key + STRINGPREFIXLEN; /* Move past the prefix */
	lua_getfield(L, -1, strkey); /* _G.string[strkey] */
	lua_remove(L, -2); /* Remove the string table */
	
	if (!lua_isfunction(L, -1)){ /* If there's no function at strkey, return nil */
		lua_pushnil(L);
		return 1;
	}

	lua_pushcclosure(L, bufflib_stringop, 1); /* Push the stringop function as a closure with the string function as the first upvalue */
	
	lua_pushvalue(L, -1); /* Push a copy of the closure */
	lua_setfield(L, 3, key); /* mt[key] = closure (Pops the first copied closure) */
	
	lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
	lua_getfield(L, -1, "bufflib");

	lua_pushvalue(L, -3); /* Push a second copy of the closure */
	lua_setfield(L, -2, key); /* bufflib[key] = closure (Pops the second copied closure) */
	lua_pop(L, 2); /* Pop the _LOADED and bufflib tables */

	return 1; /* Return the original closure */
}

/**
Equivalent to @{Buffer:add|`buff:add(...)`}.
@function add
@tparam Buffer buff The Buffer to add the values to.
@param ... Some values to add to the Buffer.
@treturn Buffer The Buffer object.
*/

/**
Equivalent to @{Buffer:addsep|`buff:addsep(sep, ...)`}.
@function addsep
@tparam Buffer buff The Buffer to add the values to.
@string sep The string to use as a separator.
@param ... Some values to add to the Buffer.
@treturn Buffer The Buffer object.
*/

/**
Equivalent to @{Buffer:reset|`buff:reset()`}.
@function reset
@tparam Buffer buff The Buffer to reset.
@treturn Buffer The emptied Buffer object.
*/

/**
Equivalent to `tostring(buff)` or @{Buffer:__tostring|`buff:__tostring()`}.
@function tostring
@tparam Buffer buff The Buffer to convert to a string.
@treturn string The Buffer's contents as a string.
*/

/**
Equivalent to `#buff` or @{Buffer:__len|`buff:__len()`}.
@function length
@tparam Buffer buff The Buffer to get the length of.
@treturn int length
*/

/**
Equivalent to `buff1 .. buff2` or @{Buffer:__concat|`buff1:__concat(buff2)`}.
@function concat
@param buff1 A Buffer or some other value.
@param buff2 A Buffer or some other value.
@treturn Buffer Either the new Buffer created from the Buffer arguments or the Buffer that the non-Buffer argument was added to.
*/

/**
Equivalent to `buff1 == buff2` or @{Buffer:__eq|`buff1:__eq(buff2)`}.
@function equal
@tparam Buffer buff1 A Buffer
@tparam Buffer buff2 Another Buffer
@treturn bool Do the Buffers hold the same contents?
*/

/**
The initial length of the char array used by @{Buffer|Buffers}.
This is set by the value of the LUAL_BUFFERSIZE macro defined in luaconf.h.
This field is not actually used by lua_bufflib, so changing it won't affect the length of new Buffers.
@int buffersize
*/

static struct luaL_Reg metareg[] = {
	{"__concat", bufflib_concat},
	{"__eq", bufflib_equal},
	{"__len", bufflib_len},
	{"__tostring", bufflib_tostring},
	{"__gc", bufflib_gc},
	{"add", bufflib_add},
	{"addsep", bufflib_addsep},
	{"reset", bufflib_reset},
	{NULL, NULL}
};

static struct luaL_Reg libreg[] = {
	{"add", bufflib_add},
	{"addsep", bufflib_addsep},
	{"equal", bufflib_equal},
	{"concat", bufflib_concat},
	{"length", bufflib_len},
	{"new", bufflib_newbuffer},
	{"reset", bufflib_reset},
	{"tostring", bufflib_tostring},
	{"isbuffer", bufflib_isbuffer},
	{NULL, NULL}
};

EXPORT int luaopen_bufflib(lua_State *L) {
	int strIndex, metaIndex, libIndex;
	
	luaL_newmetatable(L, BUFFERTYPE); /* Create the metatable */
	luaL_setfuncs(L, metareg, 0);
	lua_pushcfunction(L, bufflib_index);
	lua_setfield(L, -2, "__index"); /* mt.__index = bufflib_index */
	
	luaL_newlib(L, libreg); /* Create the library table */
	lua_pushinteger(L, LUAL_BUFFERSIZE);
	lua_setfield(L, -2, "buffersize");
	
	lua_getglobal(L, "string");
	if (lua_isnil(L, -1)){ /* If there's no string table, return now */
		lua_pop(L, 1);
		return 1;
	}

	strIndex = lua_gettop(L); /* lua_next probably won't like relative (negative) indices, so record the absolute index of the string table */
	metaIndex = strIndex - 2; /* Record the absolute values of the metatable and library table so we don't have to deal with relative indices in the loop */
	libIndex = strIndex - 1;
	
	lua_pushnil(L);
	while (lua_next(L, strIndex) != 0){
		const char *newkey;

		if (lua_type(L, -2) != LUA_TSTRING || !lua_isfunction(L, -1)){ /* If the key isn't a string or the value isn't a function, pop the value and jump to the next pair */
			lua_pop(L, 1);
			continue;
		}
		
		lua_pushliteral(L, STRINGPREFIX); /* Push the prefix string */
		lua_pushvalue(L, -3); /* Push a copy of the key */
		lua_concat(L, 2); /* prefix .. key (pops the two strings)*/
		newkey = lua_tostring(L, -1);
		lua_pop(L, 1); /* Pop the new string */
		
		lua_pushcclosure(L, bufflib_stringop, 1); /* Push the stringop function as a closure with the string function as the first upvalue */
		lua_pushvalue(L, -1); /* Push a copy of the closure */
				
		lua_setfield(L, metaIndex, newkey); /* Pops the copied closure */
		lua_setfield(L, libIndex, newkey); /* Pops the original closure, the original key is left on the top of the stack */
	}
	
	lua_pop(L, 1); /* Pop the string table */

	return 1; /* Return the library table */
}