fileName = [[C:\Users\victor\Desktop\musicas victor\resonator.wav]]rt = require"RtAudio"local socket = require"socket"dac = rt.RtAudio(rt.RtAudio_WINDOWS_DS) chunk = [[	nsamp = 512	local socket = require"socket"	udp2 = socket.udp()	succes,err = udp2:setpeernamet("127.0.0.1",57120)		local sum = 0	local buf = {}	local count = 1	function callback(nFrames,data)		for i=1,nFrames*2 do			local squared = data[i]^2			sum = sum + squared			sum = sum - (buf[count] or 0)			buf[count] = squared			count = count + 1			if count > nsamp then 				count = 1 				udp2:send(math.sqrt(sum/nsamp))			end		end		return nil	end]]-- if you path is changed copy it in the new lua statechunk = "package.path = [["..package.path .. "]]" .. chunkrt.setCallbackPost(chunk,"callback")busiz = dac:openStream({0,2},nil,44100,512)dac:startStream()sndfile = rt.soundFile(fileName)sndfile:play(1)-- iup stuf ---------------------------require("iuplua")require("iupluacontrols")gauge = iup.gauge{}dlg = iup.dialog{gauge; title="Vumeter"}udp1 = socket.udp()succes,err = udp1:setsockname("127.0.0.1",57120)if not succes then error(err) endudp1:settimeout(0)function idle_cb()	while true do		local msg,err = udp1:receive()		if msg then			--print (val)			gauge.value = tonumber(msg)		else			break		end	end    return iup.DEFAULTend--choose one of them--iup.SetIdle(idle_cb)iup.timer{TIME = 100, run = "YES", action_cb = idle_cb}dlg:showxy(iup.CENTER, iup.CENTER)iup.MainLoop()