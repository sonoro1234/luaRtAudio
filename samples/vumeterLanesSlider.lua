--lanes = require"lanes"--lanes.configure({ nb_keepers = 1, with_timers = true, on_state_create = nil,track_lanes=true})local rt = require"RtAudio"soundfileName = [[C:\Users\victor\Desktop\musicas victor\resonator.wav]]dac = rt.RtAudio() --rt.RtAudio_WINDOWS_DS) function vumeter(rtlinda,nsamp)	local sum = 0	local buf = {}	local count = 1	return function(nFrames,data)		for i=1,nFrames*2 do			local squared = data[i]^2			sum = sum + squared			sum = sum - (buf[count] or 0)			buf[count] = squared			count = count + 1			if count > nsamp then 				count = 1 				rtlinda:set("sum",math.sqrt(sum/nsamp))				--linda:send("sum",math.sqrt(sum/nsamp))			end		end		return nil	endendrt.setcb_lanes_post(vumeter,512*2)dac:openStream({0,2},nil,44100,512)dac:startStream()sndfile = rt.soundFile(soundfileName)sndfile:play()--iup stuff -------------------------------require("iuplua")require("iupluacontrols")gauge = iup.gauge{}slider = iup.val{}button = iup.button{title = "setcb"}dlg = iup.dialog{iup.vbox{gauge,slider,button}; title="Vumeter"}function button:action()	rt.setcb_lanes_post(vumeter,512*2)endfunction slider:valuechanged_cb()	print(self.value)	sndfile:play(self.value)endfunction idle_cb2()    local val = rt.linda:get("sum")	--local key,val = linda:receive(0,"sum")	if val then		gauge.value = val	end    return iup.DEFAULTendiup.timer{TIME = 100, run = "YES", action_cb = idle_cb2}dlg:showxy(iup.CENTER, iup.CENTER)iup.MainLoop()