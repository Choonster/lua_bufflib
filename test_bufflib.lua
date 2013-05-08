--[[
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
]]

local bufflib = require("bufflib")

local teststr = "abc123ABC"
local testsep = "SEP"
local testtab = {}
local testtabstr = tostring(testtab)
local testfunc = function() end
local testfuncstr = tostring(testfunc)
local testlongstr = teststr:rep(2000)

print("Testing lua_bufflib. Initial buffer size: " .. bufflib.buffersize)

-- Basic creation checks
assert(type(bufflib.new()) == "userdata")
assert(type(getmetatable(bufflib.new())) == "table")

-- String conversion checks
assert(tostring(bufflib.new(teststr)) == teststr)
assert(tostring(bufflib.new(testtab))  == testtabstr)
assert(tostring(bufflib.new(testfunc))   == testfuncstr)
assert(tostring(bufflib.new(testlongstr)) == testlongstr)

-- add/addsep checks
assert(tostring(bufflib.new():add(teststr)) == tostring(bufflib.new(teststr)))
assert(tostring(bufflib.new():add(teststr, testtab)) == (teststr .. testtabstr))
assert(tostring(bufflib.new():addsep(testsep, teststr, testtab, testfunc)) == (teststr .. testsep .. testtabstr .. testsep .. testfuncstr))

-- Length checks
assert(#bufflib.new() == 0)
assert(#bufflib.new(teststr) == #teststr)
assert(#bufflib.new(testtab)  == #testtabstr)
assert(#bufflib.new(testfunc)   == #testfuncstr)
assert(#bufflib.new(teststr, testtab, testfunc) == #(teststr .. testtabstr .. testfuncstr))
assert(#bufflib.new(testlongstr, testtab, testfunc) == #(testlongstr .. testtabstr .. testfuncstr))

-- Concatenatation checks
assert(tostring(bufflib.new(teststr) .. bufflib.new(teststr)) == (teststr .. teststr))
assert(tostring(bufflib.new(testtabstr) .. testtabstr) == (testtabstr .. testtabstr))
assert(tostring(bufflib.new(teststr, testtab) .. bufflib.new(testfunc)) == (teststr .. testtabstr .. testfuncstr))

do
	local buffstr = tostring(bufflib.new(testlongstr, testfunc) .. bufflib.new(teststr, testtab))
	local resstr = testlongstr .. testfuncstr .. teststr .. testtabstr
	assert(buffstr == resstr, ("%q (len %d) ~= %q (len %d)"):format(buffstr:sub(1, 25) .. "..." .. buffstr:sub(-25), #buffstr, resstr:sub(1, 25) .. "..." .. resstr:sub(-25), #resstr))
end

-- Equality checks
assert(bufflib.new(teststr) == bufflib.new(teststr))
assert(bufflib.new(testfunc, testtab) == bufflib.new(testfunc, testtab))
assert(bufflib.new(testlongstr, testfunc, testtab) == bufflib.new(testlongstr, testfunc, testtab))

-- Reset checks
assert(tostring(bufflib.new(teststr):reset()) == "")
assert(tostring(bufflib.new(testlongstr, testlongstr, testlongstr, testfunc):reset()) == "")

-- isbuffer checks
assert(bufflib.isbuffer(bufflib.new()))
assert(not bufflib.isbuffer(1))
assert(not bufflib.isbuffer(testtab))

local initialGC = collectgarbage("count")
collectgarbage()
local gcdiff =  initialGC - collectgarbage("count")

print(("lua_bufflib tests completed successfully. Collected %.2f KB of garbage."):format(gcdiff))