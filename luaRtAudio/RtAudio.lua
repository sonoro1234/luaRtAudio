
local suceed,lanes = pcall(require,"lanes")

if not suceed then
	print"RtAudio could not found lanes:"
	print(lanes) -- print error
	lanes = nil
else
	if lanes.configure then --initial
		lanes.configure({ nb_keepers = 1, with_timers = true, on_state_create = nil,track_lanes=true})
	end
end

local core = require("RtAudio.core")
local M = {}
setmetatable(M,{__index = core})

if lanes then 
	M.lanes = lanes
	if not _RtAudio_linda then -- it is not included from secondary lane
		M.linda = lanes.linda("RtAudio linda")
	end

local function lanegen(func_body,thread_name,post)
	thread_name = thread_name or "unamed thread"
	local function prstak(stk)
		local str=""
		for i,lev in ipairs(stk) do
			str= str..i..": \n"
			for k,v in pairs(lev) do
				str=str.."\t["..k.."]:"..v.."\n"
			end
		end
		print(str)
	end
	
	local function finalizer_func(err,stk)
		print(thread_name.." finalizer:")
		if err and type(err)~="userdata" then 
			print( thread_name.." after error: "..tostring(err) )
			print(thread_name.." finalizer stack table")
			prstak(stk)
		elseif type(err)=="userdata" then
			print( thread_name.." after cancel " )
		else
			print( thread_name.." after normal return" )
		end
	end
	
	local function outer_func_post(...)
		set_finalizer( finalizer_func ) 
		set_error_reporting("extended")
		set_debug_threadname(thread_name)
		local rta = require"RtAudio"

		callbackF = func_body(...)
		local state,ref = rta.RegisterLanesCallback(callbackF)
		--notify done and sleep forever
		local block_linda = rta.lanes.linda()
		_RtAudio_linda:send(thread_name,{block_linda, state, ref})
		block_linda:receive("unblock")
	end
	
	local function outer_func(...)
		set_finalizer( finalizer_func ) 
		set_error_reporting("extended")
		set_debug_threadname(thread_name)
		local rta = require"RtAudio"

		local callbackF = func_body(...)
		local state,ref = rta.RegisterLanesCallback(callbackF)
		--notify done and sleep forever
		local block_linda = rta.lanes.linda()
		_RtAudio_linda:send(thread_name,{block_linda, state, ref}) 
		block_linda:receive("unblock")
	end
	
	if post then
		return lanes.gen("*",{globals = {_RtAudio_linda = M.linda}},outer_func_post)
	else
		return lanes.gen("*",{globals = {_RtAudio_linda = M.linda}},outer_func)
	end
end

M.block_lindas = {}
function M.RtAudio(api)
	local dac = core.RtAudio(api)
	local meta = getmetatable(dac)
	function meta:setcb_lanes(cb,...)
		assert(type(cb)=="function","arg must be a function")
		local name = "cb_lanes "..tostring(dac)
		lanegen(cb,name)(M.linda,...)
		local key,val = M.linda:receive(name) -- block until  callback is set
		if M.block_lindas[name] then
			--perhaps we should stop stream before?
			M.block_lindas[name]:send("unblock",1)
		end
		M.block_lindas[name] = val[1]
		self:setCallbackLanes(val[2],val[3])
	end
	function meta:setcb_lanes_post(cb,...)
		assert(type(cb)=="function","arg must be a function")
		local name = "cb_post_lanes "..tostring(dac)
		lanegen(cb,name,true)(M.linda,...)
		local key,val = M.linda:receive(name) -- block until  callback is set
		if M.block_lindas[name] then
			--perhaps we should stop stream before?
			M.block_lindas[name]:send("unblock",1)
		end
		M.block_lindas[name] = val[1]
		self:setCallbackLanesPost(val[2],val[3])
	end
	function meta:getDeviceByName(name)
		for i=0,self:getDeviceCount()-1 do
			if name == self:getDeviceInfo(i).name then
				return i
			end
		end
	end
	return dac
end
end -- if(lanes)
return M