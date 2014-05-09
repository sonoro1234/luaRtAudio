Lua binding of RtAudio and libsndfile

It makes possible to play and record audio files but also to write sound generators and filters in Lua language.

There are still no docs but you can try lua samples to understand the api

Remember to use git submodule update for getting RtAudioORIG folder (it is a submodule pointing to the original RtAudio github)

Build directory has binaries from mingw and also init_cmake.bat and toolchain.cmake with CMake variables to be modified for building on any system
