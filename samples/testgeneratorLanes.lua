rt = require"RtAudio"dac = rt.RtAudio(rt.RtAudio_WINDOWS_DS)function fun1(rtlinda)	local om = 0	local freq = 200	return function(nFrames)		local key,val = rtlinda:receive(0,"freq")		freq = key and val or freq		local res = {}		local sin = math.sin		local hh = 2*math.pi*freq/44100		for i=1,nFrames do			om = om + hh			local val = sin(om)*sin(om*0.01)			table.insert(res,val)			table.insert(res,val)		end		return res	endendrt.setcb_lanes(fun1)busiz = dac:openStream({0,2},nil,44100,512)dac:startStream()while true do	print"type freq in Hz or quit to exit"	freq = io.read"*l"	if freq == "quit" then break end	rt.linda:send("freq",freq)end