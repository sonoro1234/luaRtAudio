rem set PATH=%PATH%;C:\mingw32-4.8.2-posix-dwarf\bin;

cmake  -G"MinGW Makefiles"  -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="toolchain.cmake"  ../luaRtAudio
