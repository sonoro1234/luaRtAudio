local lanes = require"lanes"
function lanegen(func_body,thread_name)
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
	local function outer_func(...)
		--lanes = require "lanes" --.configure()
		--lanes.require"lanes"
		set_finalizer( finalizer_func ) 
		set_error_reporting("extended")
		set_debug_threadname(thread_name)
		func_body(...)
	end
	return lanes.gen("*",{required={"lanes"}},outer_func)
end
function Sleep(dur)
	local linda2 = lanes.linda()
	lanes.timer(linda2,"wait",dur,0)
	linda2:receive("wait")
end