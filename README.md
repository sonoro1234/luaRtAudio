Lua binding of RtAudio and libsndfile

It makes possible to play and record audio files but also to write sound generators and filters in Lua language.

There are still no docs but you can try lua samples to understand the api

Remember to use git submodule update for getting rtaudio folder (it is a submodule pointing to the original RtAudio github)

To build you have two options:  
first option expecting cmake to find lua and sndfile:  
	1- cmake path_to_repository  
	2- make  
second option:  
	1- provide the paths nedded in build/toolchain.cmake  
	2- use init_cmake.bat (or adapt to linux and macosx)  
	3- make install  
	
