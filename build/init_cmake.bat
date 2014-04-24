set PATH=%PATH%;C:\mingw32-4.8.2-posix-dwarf\bin;

"C:\Program Files\CMake 2.8\bin\cmake"  -G"MinGW Makefiles"  -DCMAKE_BUILD_TYPE=Release -DLIBSNDFILE_INCLUDE_DIR="C:/LUA/libsndfile-1.0.25/src" -DLIBSNDFILE_LIB="C:/LUA/libsndfile-1.0.25/src/.libs/libsndfile-1.dll"  -DCMAKE_TOOLCHAIN_FILE="toolchain.cmake"  ..
