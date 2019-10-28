PROJECT(luaRtAudio)
cmake_minimum_required(VERSION 3.13)
set(CMAKE_SHARED_LIBRARY_PREFIX "")
#first take care of RtAudio CMake
if (WIN32)
	set(RTAUDIO_API_DS ON CACHE INTERNAL "use windows ds!")
	set(RTAUDIO_API_ASIO ON CACHE INTERNAL "use windows asio!")
	set(RTAUDIO_API_WASAPI ON CACHE INTERNAL "use windows wasapi!")
endif(WIN32)

if (APPLE)
    set(RTAUDIO_API_CORE ON CACHE INTERNAL "use OSX_CORE!")
endif (APPLE)

if (UNIX AND NOT APPLE)
	set(RTAUDIO_API_OSS ON CACHE INTERNAL "AUDIO_LINUX_OSS!")
	set(RTAUDIO_API_ALSA ON CACHE INTERNAL "AUDIO_LINUX_ALSA!")
	set(RTAUDIO_API_PULSE ON CACHE INTERNAL "AUDIO_LINUX_PULSE!")
	set(RTAUDIO_API_JACK ON CACHE INTERNAL "AUDIO_UNIX_JACK!")
endif ()

set(BUILD_TESTING OFF CACHE INTERNAL "dont build tests!")
add_subdirectory(rtaudio)# EXCLUDE_FROM_ALL)

#Lua
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake_modules/")
if(NOT LUA_INCLUDE_DIR)
find_package(Lua51 REQUIRED)
endif()
#sndfile
if(NOT SNDFILE_INCLUDE_DIR)
find_package(Sndfile)
endif()

#now luaRtAudio
get_directory_property(LINKLIBS DIRECTORY rtaudio DEFINITION LINKLIBS)
#get_directory_property(rtaudiomodule DIRECTORY rtaudio rtaudio)
get_property(rtaudiomodule DIRECTORY rtaudio PROPERTY rtaudio)
message(STATUS rtaudiomodule ${rtaudiomodule})
add_definitions(-DLUA_BUILD_AS_DLL -DLUA_LIB)
if(SNDFILE_INCLUDE_DIR)
	add_definitions(-DUSE_SNDFILE)
	INCLUDE_DIRECTORIES(${SNDFILE_INCLUDE_DIR})
	set(LINKSNDFILE ${SNDFILE_LIBRARY})
endif()
INCLUDE_DIRECTORIES(${LUA_INCLUDE_DIR})
INCLUDE_DIRECTORIES(rtaudio)
ADD_LIBRARY(luaRtAudio SHARED luaRtAudio/luaRtAudio.cpp)
set_target_properties(luaRtAudio PROPERTIES PREFIX "" OUTPUT_NAME core)

#TARGET_LINK_LIBRARIES(luaRtAudio ${LUA_LIBRARIES} rtaudio_static ${LINKLIBS} ${LINKSNDFILE})
TARGET_LINK_LIBRARIES(luaRtAudio ${LUA_LIBRARIES} rtaudio ${LINKLIBS} ${LINKSNDFILE})

if(INSTALL_CMOD)
install(TARGETS luaRtAudio DESTINATION ${INSTALL_CMOD}/RtAudio)
INSTALL(FILES luaRtAudio/RtAudio.lua DESTINATION ${INSTALL_LMOD})
install(TARGETS rtaudio 
               RUNTIME DESTINATION  ${INSTALL_CMOD}
               LIBRARY DESTINATION  ${INSTALL_CMOD}
              # ARCHIVE DESTINATION ${INSTALL_CMOD}/lib 
			  #PUBLIC_HEADER DESTINATION ${INSTALL_CMOD}/lib
               )

INSTALL (FILES ${LINKSNDFILE} DESTINATION ${INSTALL_CMOD})
endif()
