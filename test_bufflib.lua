-- Minimal test suite for lua_bufflib.

local bufflib = require("bufflib")

local teststr = "abc123ABC"
local testformatstr = "%s %d %q"
local testsep = "SEP"
local testtab = {}
local testtabstr = tostring(testtab)
local testfunc = function() end
local testfuncstr = tostring(testfunc)
local testlongstr = teststr:rep(2000)

print("Testing lua_bufflib. Initial buffer size: " .. bufflib.buffersize)

-- Basic creation tests
assert(type(bufflib.new()) == "userdata", "bufflib.new() failed to return a userdata")
assert(type(getmetatable(bufflib.new())) == "table", "new buffer doesn't have a metatable")
print("Basic tests passed")

-- String conversion tests
assert(tostring(bufflib.new(teststr)) == teststr, "string conversion (string) failed")
assert(tostring(bufflib.new(testtab))  == testtabstr, "string conversion (table) failed")
assert(tostring(bufflib.new(testfunc))   == testfuncstr, "string conversion (function) failed")
assert(tostring(bufflib.new(testlongstr)) == testlongstr, "string conversion (long string) failed")
print("String conversion tests passed")

-- add/addsep tests
assert(tostring(bufflib.new():add(teststr)) == tostring(bufflib.new(teststr)), "add (string) failed")
assert(tostring(bufflib.new():add(teststr, testtab)) == (teststr .. testtabstr), "add (string, table) failed")
assert(tostring(bufflib.new():addsep(testsep, teststr, testtab, testfunc)) == (teststr .. testsep .. testtabstr .. testsep .. testfuncstr), "addsep failed")
print("add/addsep tests passed")

-- Length tests
assert(#bufflib.new() == 0, "empty buffer has non-zero length")
assert(#bufflib.new(teststr) == #teststr, "length (string) failed")
assert(#bufflib.new(testtab)  == #testtabstr, "length (table) failed")
assert(#bufflib.new(testfunc)   == #testfuncstr, "length (function) failed")
assert(#bufflib.new(teststr, testtab, testfunc) == #(teststr .. testtabstr .. testfuncstr), "length (string, table, function) failed")
assert(#bufflib.new(testlongstr, testtab, testfunc) == #(testlongstr .. testtabstr .. testfuncstr), "length (long string, table, function) failed")
print("Length tests passed")

-- Concatenatation tests
assert(tostring(bufflib.new(teststr) .. bufflib.new(teststr)) == (teststr .. teststr), "concatenation (buffer(string) .. buffer(string)) failed")
assert(tostring(bufflib.new(testtab) .. testtabstr) == (testtabstr .. testtabstr), "concatenation (buffer(table) .. string) failed")
assert(tostring(bufflib.new(teststr, testtab) .. bufflib.new(testfunc)) == (teststr .. testtabstr .. testfuncstr), "concatenation (buffer(string, table) .. buffer(function)) failed")

do -- This test seems to be the most likely to fail when something goes wrong, so we give it a detailed error message
	local buffstr = tostring(bufflib.new(testlongstr, testfunc) .. bufflib.new(teststr, testtab))
	local resstr = testlongstr .. testfuncstr .. teststr .. testtabstr
	if buffstr ~= resstr then
		for i = 1, math.max(#buffstr, #resstr) do
			local buffchar, reschar = buffstr:sub(i, i), resstr:sub(i, i)
			if buffchar ~= reschar then
				print(("Position %d: %s ~= %s"):format(i, tostring(buffchar), tostring(reschar)))
			end
		end
		error(("concatenation (buffer(long string, function) .. buffer(string, table)) failed -- %q (len %d) ~= %q (len %d)"):format(buffstr:sub(1, 25) .. "..." .. buffstr:sub(-25), #buffstr, resstr:sub(1, 25) .. "..." .. resstr:sub(-25), #resstr))
	end
end

print("Concatenatation tests passed")

-- Equality tests
assert(bufflib.new(teststr) == bufflib.new(teststr), "equality (string) failed")
assert(bufflib.new(testfunc, testtab) == bufflib.new(testfunc, testtab), "equality (function, table) failed")
assert(bufflib.new(testlongstr, testfunc, testtab) == bufflib.new(testlongstr, testfunc, testtab), "equality (long string, function, table) failed")
print("Equality tests passed")

-- Reset tests
assert(tostring(bufflib.new(teststr):reset()) == "", "reset (string) failed")
assert(tostring(bufflib.new(testlongstr, testlongstr, testlongstr, testfunc):reset()) == "", "reset (long string x3, function) failed")
print("Reset tests passed")

-- isbuffer tests
assert(bufflib.isbuffer(bufflib.new()), "isbuffer (buffer) failed")
assert(not bufflib.isbuffer(1), "isbuffer (number) failed")
assert(not bufflib.isbuffer(testtab), "isbuffer (table) failed")
print("isbuffer tests passed")

-- string method checks
-- We reuse the same buffer for most of these tests, since we've already established that the basic buffer operations work and we just want to test that the string methods call and return from the original function properly.
-- We don't test string.char or string.dump because they can't be used as string methods due to their arguments. We don't test string.gmatch because it can't be tested with a simple equality check.

local testbuff = bufflib.new(teststr)
assert(testbuff:s_byte(2) == teststr:byte(2), "s_byte failed")
assert(testbuff:s_find("123", 1, true) == teststr:find("123", 1, true), "s_find (plain) failed")
assert(testbuff:s_find("%d+") == teststr:find("%d+"), "s_find (pattern) failed")
assert(bufflib.new(testformatstr):s_format(teststr, 123, teststr) == testformatstr:format(teststr, 123, teststr), "s_format failed")
assert(testbuff:s_gsub("123", "000") == teststr:gsub("123", "000"), "s_gsub failed")
assert(testbuff:s_len() == teststr:len(), "s_len failed") -- This test is fairly similar to the length operator tests we performed above, but we perform it anyway
assert(testbuff:s_lower() == teststr:lower(), "s_lower failed")
assert(testbuff:s_match("%d+") == teststr:match("%d+"), "s_match failed")
assert(testbuff:s_rep(42) == teststr:rep(42), "s_rep failed")
assert(testbuff:s_reverse() == teststr:reverse(), "s_reverse failed")
assert(testbuff:s_sub(2, 4) == teststr:sub(2, 4), "s_sub failed")
assert(testbuff:s_upper() == teststr:upper(), "s_upper failed")

print("String method tests passed")

local initialGC = collectgarbage("count")
collectgarbage()
local gcdiff =  initialGC - collectgarbage("count")

print(("lua_bufflib tests completed successfully. Collected %.2f KB of garbage."):format(gcdiff))