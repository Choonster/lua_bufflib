-- Minimal test suite for lua_bufflib.

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
assert(type(bufflib.new()) == "userdata", "bufflib.new() failed to return a userdata")
assert(type(getmetatable(bufflib.new())) == "table", "new buffer doesn't have a metatable")

-- String conversion checks
assert(tostring(bufflib.new(teststr)) == teststr, "string conversion (string) failed")
assert(tostring(bufflib.new(testtab))  == testtabstr, "string conversion (table) failed")
assert(tostring(bufflib.new(testfunc))   == testfuncstr, "string conversion (function) failed")
assert(tostring(bufflib.new(testlongstr)) == testlongstr, "string conversion (long string) failed")

-- add/addsep checks
assert(tostring(bufflib.new():add(teststr)) == tostring(bufflib.new(teststr)), "add (string) failed")
assert(tostring(bufflib.new():add(teststr, testtab)) == (teststr .. testtabstr), "add (string, table) failed")
assert(tostring(bufflib.new():addsep(testsep, teststr, testtab, testfunc)) == (teststr .. testsep .. testtabstr .. testsep .. testfuncstr), "addsep failed")

-- Length checks
assert(#bufflib.new() == 0, "empty buffer has non-zero length")
assert(#bufflib.new(teststr) == #teststr, "length (string) failed")
assert(#bufflib.new(testtab)  == #testtabstr, "length (table) failed")
assert(#bufflib.new(testfunc)   == #testfuncstr, "length (function) failed")
assert(#bufflib.new(teststr, testtab, testfunc) == #(teststr .. testtabstr .. testfuncstr), "length (string, table, function) failed")
assert(#bufflib.new(testlongstr, testtab, testfunc) == #(testlongstr .. testtabstr .. testfuncstr), "length (long string, table, function) failed")

-- Concatenatation checks
assert(tostring(bufflib.new(teststr) .. bufflib.new(teststr)) == (teststr .. teststr), "concatenation (buffer(string) .. buffer(string)) failed")
assert(tostring(bufflib.new(testtab) .. testtabstr) == (testtabstr .. testtabstr), "concatenation (buffer(table) .. string) failed")
assert(tostring(bufflib.new(teststr, testtab) .. bufflib.new(testfunc)) == (teststr .. testtabstr .. testfuncstr), "concatenation (buffer(string, table) .. buffer(function)) failed")

do
	local buffstr = tostring(bufflib.new(testlongstr, testfunc) .. bufflib.new(teststr, testtab))
	local resstr = testlongstr .. testfuncstr .. teststr .. testtabstr
	assert(buffstr == resstr, ("concatenation (buffer(long string, function) .. buffer(string, table)) faile -- %q (len %d) ~= %q (len %d)"):format(buffstr:sub(1, 25) .. "..." .. buffstr:sub(-25), #buffstr, resstr:sub(1, 25) .. "..." .. resstr:sub(-25), #resstr))
end

-- Equality checks
assert(bufflib.new(teststr) == bufflib.new(teststr), "equality (string) failed")
assert(bufflib.new(testfunc, testtab) == bufflib.new(testfunc, testtab), "equality (function, table) failed")
assert(bufflib.new(testlongstr, testfunc, testtab) == bufflib.new(testlongstr, testfunc, testtab), "equality (long string, function, table) failed")

-- Reset checks
assert(tostring(bufflib.new(teststr):reset()) == "", "reset (string) failed")
assert(tostring(bufflib.new(testlongstr, testlongstr, testlongstr, testfunc):reset()) == "", "reset (long string x3, function) failed")

-- isbuffer checks
assert(bufflib.isbuffer(bufflib.new()), "isbuffer (buffer) failed")
assert(not bufflib.isbuffer(1), "isbuffer (number) failed")
assert(not bufflib.isbuffer(testtab), "isbuffer (table) failed")

local initialGC = collectgarbage("count")
collectgarbage()
local gcdiff =  initialGC - collectgarbage("count")

print(("lua_bufflib tests completed successfully. Collected %.2f KB of garbage."):format(gcdiff))