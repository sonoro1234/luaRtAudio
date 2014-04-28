extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
};

#if defined(USE_SNDFILE)
#include <set>
#endif

#include<iostream>
#define lua_userdata_cast(L, pos, T) reinterpret_cast<T*>(luaL_checkudata((L), (pos), #T))
void* operator new(size_t size, lua_State* L, const char* metatableName)
{
    void* ptr = lua_newuserdata(L, size);
    luaL_getmetatable(L, metatableName);
    // assert(lua_istable(L, -1)) if you're paranoid
    lua_setmetatable(L, -2);
    return ptr;
}
#define lua_pushobject(L, T) new(L, #T) T
#include "RtAudio.h"
#define RTAUDIO "RtAudio"

static lua_State *callback_state=0;
static std::string thecallback_name;
static lua_State *callback_state_post=0;
static std::string thecallback_name_post;

int setCallbackLanes(lua_State *L){
	lua_gc(L, LUA_GCSTOP,0);
	const char* str = luaL_checkstring(L, 1);
	thecallback_name = str;
	callback_state = L;
	return 0;
}
int setCallbackLanesPost(lua_State *L){
	lua_gc(L, LUA_GCSTOP,0);
	const char* str = luaL_checkstring(L, 1);
	thecallback_name_post = str;
	callback_state_post = L;
	return 0;
}
int setCallback(lua_State *L){
    const char *chunk = luaL_checkstring(L, 1);
	const char* str = luaL_checkstring(L, 2);
	thecallback_name = str;
    lua_State *L1 = luaL_newstate();
    if (L1 == NULL)
        luaL_error(L, "unable to create new state");
    if (luaL_loadstring(L1, chunk) != 0)
        luaL_error(L, "error starting thread: %s",lua_tostring(L1, -1));
    luaL_openlibs(L1); /* open standard libraries */
    if (lua_pcall(L1, 0, 0, 0) != 0) /* call main chunk */
        fprintf(stderr, "thread error: %s", lua_tostring(L1, -1));
    //lua_gc(L1, LUA_GCSTOP,0);
    callback_state = L1;
    return 0;
}

int setCallbackPost(lua_State *L){
    const char *chunk = luaL_checkstring(L, 1);
	const char* str = luaL_checkstring(L, 2);
	thecallback_name_post = str;
    lua_State *L1 = luaL_newstate();
    if (L1 == NULL)
        luaL_error(L, "unable to create new state");
    if (luaL_loadstring(L1, chunk) != 0)
        luaL_error(L, "error starting thread: %s",lua_tostring(L1, -1));
    luaL_openlibs(L1); /* open standard libraries */
    if (lua_pcall(L1, 0, 0, 0) != 0) /* call main chunk */
        fprintf(stderr, "thread error: %s", lua_tostring(L1, -1));
    //lua_gc(L1, LUA_GCSTOP,0);
    callback_state_post = L1;
    return 0;
}

int getCallback(lua_State *L){
    //lua_pushnumber(L,numero);
    return 0;
}
typedef struct callback_info
{
	unsigned int noutchannels;
	unsigned int sampleRate;
} callback_info;

#if defined(USE_SNDFILE)
	bool playSoundFiles(double *outputBuffer,unsigned int nFrames,int channels, double streamTime,double windowsize);
#endif
static int cbMix(void *outputBuffer, void *inputBuffer, unsigned int nFrames, double streamTime, RtAudioStreamStatus status, void *data){
	//std::cout << *(int *)data << std::endl;
	callback_info *cbinfo = (callback_info *)data;
	int channels = cbinfo->noutchannels; //*((int *)data);
	double windowsize = nFrames / cbinfo->sampleRate;
	memset(outputBuffer,0,nFrames * channels * sizeof(double));
    if (callback_state!=0){
        lua_State *L = callback_state;
        //lua_gc(L, LUA_GCSTOP,0);
        lua_getglobal(L, thecallback_name.c_str()); /* function to be called */
        lua_pushnumber(L, nFrames); /* push 1st argument */
        if (lua_pcall(L, 1, 1, 0) != 0){
            printf("error running function %s: %s\n",thecallback_name.c_str(),lua_tostring(L, -1));
            //luaL_error(L, "error running function 'thecallback': %s",lua_tostring(L, -1));
			lua_pop(L,1);
			return 1;
		}
        if(!lua_istable(L,-1)){
            printf("error running function %s: did not returned table\n",thecallback_name.c_str());
            //luaL_error(L, "error running function 'thecallback': did not returned table");
			return 1;
		}
        double dummy;
        double *buffer = (double *) outputBuffer;
		//interleaved is more simple
		for (int i=1; i<=nFrames*channels; i++ ) {
			lua_rawgeti(L, -1,i);
            *buffer++ = (double)luaL_checknumber(L, -1);
            lua_pop(L, 1);
		}
        lua_pop(L, 1); /* pop returned value */
    }else{
	#if defined(USE_SNDFILE)
		if(!playSoundFiles((double *)outputBuffer,nFrames,channels,streamTime,windowsize)){
			printf("error on playSoundFiles");
			return 1;
		}
	#endif
		if(callback_state_post!=0){
			lua_State *L = callback_state_post;
			lua_getglobal(L, thecallback_name_post.c_str());
			lua_pushnumber(L, nFrames);
			//table with data
			double *buffer = (double *) outputBuffer;
			lua_createtable(L,nFrames*channels,0);
			for (int i=1; i<=nFrames*channels; i++ ) {
				lua_pushnumber(L,(double)*buffer++);
				lua_rawseti(L, -2,i);
			}
			if (lua_pcall(L, 2, 1, 0) != 0){
				//printf("error running function %s: %s\n",thecallback_name_post.c_str(),lua_tostring(L, -1));
				luaL_error(L, "error running function 'thecallback': %s",lua_tostring(L, -1));
				lua_pop(L,1);
				return 1;
			}
			if(!lua_istable(L,-1)){
				//printf("error running function %s: did not returned table\n",thecallback_name.c_str());
				//luaL_error(L, "error running function 'thecallback': did not returned table");
				//return 1;
				//nothing, leave untouched
				lua_pop(L, 1);
			}else{
				double *buffer = (double *) outputBuffer;
				//interleaved is more simple
				for (int i=1; i<=nFrames*channels; i++ ) {
					lua_rawgeti(L, -1,i);
					*buffer++ = (double)luaL_checknumber(L, -1);
					lua_pop(L, 1);
				}
				lua_pop(L, 1); /* pop returned value */
			}
		}
	}
    return 0;
}

// Two-channel sawtooth wave generator.
int saw( void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
double streamTime, RtAudioStreamStatus status, void *userData )
{
    unsigned int i, j;
    double *buffer = (double *) outputBuffer;
    double *lastValues = (double *) userData;
    if ( status )
        std::cout << "Stream underflow detected!" << std::endl;
        // Write interleaved audio data.
    for ( i=0; i<nBufferFrames; i++ ) {
        for ( j=0; j<2; j++ ) {
            *buffer++ = lastValues[j];
            lastValues[j] += 0.005 * (j+1+(j*0.1));
            if ( lastValues[j] >= 1.0 ) lastValues[j] -= 2.0;
        }
    }
    return 0;
}

int saw32( void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
double streamTime, RtAudioStreamStatus status, void *userData )
{
    unsigned int i, j;
    long *buffer = (long *) outputBuffer;
    double *lastValues = (double *) userData;
    if ( status )
        std::cout << "Stream underflow detected!" << std::endl;
        // Write interleaved audio data.
    for ( i=0; i<nBufferFrames; i++ ) {
        for ( j=0; j<2; j++ ) {
            *buffer++ = lastValues[j]*LONG_MAX;
            lastValues[j] += 0.005 ;//* (j+1+(j*0.1));
            if ( lastValues[j] >= 1.0 ) lastValues[j] -= 2.0;
        }
    }
    return 0;
}

int saw16( void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
double streamTime, RtAudioStreamStatus status, void *userData )
{
    int i, j;
    signed short *buffer = (signed short *) outputBuffer;
    double *lastValues = (double *) userData;
    if ( status )
        std::cout << "Stream underflow detected!" << std::endl;
        // Write interleaved audio data.
    for ( i=0; i<nBufferFrames; i++ ) {
        //for ( j=0; j<2; j++ ) {
        for ( j=1; j>=0; j-- ) {
            *buffer++ = (signed short)(lastValues[j]*32767*0.1);
            lastValues[j] += 0.005 ;//* (j+1+(j*0.1));
            if ( lastValues[j] >= 1.0 ) lastValues[j] -= 2.0;
        }
    }
    return 0;
}

typedef struct NamedConst  
{
  const char *str;
  unsigned int value;
} NamedConst;

static const NamedConst rtaudio_Const[] = {
    {"RtAudio_UNSPECIFIED",             RtAudio::UNSPECIFIED},
    {"RtAudio_LINUX_ALSA",             RtAudio::LINUX_ALSA}, 
//#if VERSION
    {"RtAudio_LINUX_PULSE",             RtAudio::LINUX_PULSE}, 
//#endif
    {"RtAudio_LINUX_OSS",             RtAudio::LINUX_OSS}, 
    {"RtAudio_UNIX_JACK",             RtAudio::UNIX_JACK}, 
    {"RtAudio_MACOSX_CORE",             RtAudio::MACOSX_CORE}, 
    {"RtAudio_WINDOWS_ASIO",             RtAudio::WINDOWS_ASIO},
    {"RtAudio_WINDOWS_DS",             RtAudio::WINDOWS_DS},
	{"RtAudio_WINDOWS_WASAPI",          RtAudio::WINDOWS_WASAPI},
    {"RtAudio_RTAUDIO_DUMMY",          RtAudio::RTAUDIO_DUMMY},      
    { 0, 0}
};
static const NamedConst rtaudio_formats[] = {
    {"RTAUDIO_SINT8",             RTAUDIO_SINT8},
    {"RTAUDIO_SINT16",             RTAUDIO_SINT16}, 
    {"RTAUDIO_SINT24",             RTAUDIO_SINT24}, 
    {"RTAUDIO_SINT32",             RTAUDIO_SINT32}, 
    {"RTAUDIO_FLOAT32",            RTAUDIO_FLOAT32}, 
    {"RTAUDIO_FLOAT64",            RTAUDIO_FLOAT64}, 
    { 0, 0}
};
void registerConstants(lua_State *L,const NamedConst* rtaudio_const){
    for (; rtaudio_const->str; rtaudio_const++) 
    {
    lua_pushstring(L, rtaudio_const->str);
    lua_pushinteger(L, rtaudio_const->value);
    lua_settable(L, -3);
    }
}
const char * getStringConstants(unsigned int val){
    const NamedConst* rtaudio_const = rtaudio_Const;
    for (; rtaudio_const->str; rtaudio_const++) 
    {
        if (val == rtaudio_const->value)
            return rtaudio_const->str;
    }
    return NULL;
}
int lua_RtAudio(lua_State *L)
{
    if (lua_gettop(L)== 0)
        lua_pushobject(L, RtAudio)();
    else
        lua_pushobject(L, RtAudio)((RtAudio::Api)lua_tointeger(L,1));
    return 1;
}
int Destroy(lua_State *L)
{
	RtAudio *dac = lua_userdata_cast(L,1,RtAudio);
	dac->~RtAudio();
	return 0;
}
int getDeviceCount_lua(lua_State *L)
{
    RtAudio *dac = lua_userdata_cast(L,1,RtAudio);
    lua_pushinteger(L, dac->getDeviceCount());
    return 1;
}

int getDeviceInfo_lua(lua_State *L)
{
    RtAudio *dac = lua_userdata_cast(L,1,RtAudio);
    int ndevice = luaL_checkint(L, 2);
    RtAudio::DeviceInfo info;
    try {
        info = dac->getDeviceInfo(ndevice);
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    lua_newtable(L);
    lua_pushstring(L, "probed");
    lua_pushboolean(L, info.probed);
    lua_settable(L,-3);
    lua_pushstring(L, "name");
    lua_pushstring(L, info.name.c_str());
    lua_settable(L,-3);
    lua_pushstring(L, "outputChannels");
    lua_pushinteger(L, info.outputChannels);
    lua_settable(L,-3);
    lua_pushstring(L, "inputChannels");
    lua_pushinteger(L, info.inputChannels);
    lua_settable(L,-3);
    lua_pushstring(L, "duplexChannels");
    lua_pushinteger(L, info.duplexChannels);
    lua_settable(L,-3);
    lua_pushstring(L, "isDefaultOutput");
    lua_pushboolean(L, info.isDefaultOutput);
    lua_settable(L,-3);
    lua_pushstring(L, "isDefaultInput");
    lua_pushboolean(L, info.isDefaultInput);
    lua_settable(L,-3);
    lua_pushstring(L, "nativeFormats");
    lua_pushinteger(L, info.nativeFormats);
    lua_settable(L,-3);
    //samplerates
    lua_pushstring(L, "sampleRates");
    lua_newtable(L);
    int count = 0;
    for (std::vector<unsigned int>::iterator it = info.sampleRates.begin() ; it != info.sampleRates.end(); ++it){
        count++;
        lua_pushinteger(L,*it);
        lua_rawseti(L,-2,count);
    }
    lua_settable(L,-3);
    return 1;
}
int getCompiledApi_lua(lua_State *L)
{
    std::vector<RtAudio::Api> apis;
    try {
        RtAudio::getCompiledApi(apis);
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    lua_newtable(L);
    int count = 0;
    for (std::vector<RtAudio::Api>::iterator it = apis.begin() ; it != apis.end(); ++it){
        count++;
        //lua_pushinteger(L,*it);
        const char *str = getStringConstants(*it);
        //std::cout << str << "is the string constant" << std::endl;
        if (str == NULL)
            luaL_error(L,"cant find constant");
        lua_pushstring(L,str );
        lua_rawseti(L,-2,count);
    }
    return 1;
}

int getCurrentApi_lua(lua_State *L)
{
    RtAudio *dac = lua_userdata_cast(L,1,RtAudio);
    RtAudio::Api api;
    try {
        api = dac->getCurrentApi();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    const char *str = getStringConstants(api);
    //std::cout << str << "is the string constant" << std::endl;
    if (str == NULL)
        luaL_error(L,"cant find constant");
    lua_pushstring(L,str );
    return 1;
}

RtAudio::StreamParameters* getParameters(lua_State *L,int n)
{
    RtAudio::StreamParameters *output_parameters;
    RtAudio::StreamParameters *output = new RtAudio::StreamParameters;
    if (lua_istable(L, n)){
        //std::cout  << "istable" << std::endl; 
        lua_rawgeti(L, n, 1);
        output->deviceId = luaL_checkint(L, -1);
        //std::cout << output->deviceId << "is outputlll" << std::endl; 
        lua_rawgeti(L, n, 2);
        output->nChannels = luaL_checkint(L, -1);
        lua_rawgeti(L, n, 3);
        if (lua_isnil(L, -1)) { 
            lua_pop(L, 1);
        }else{
            output->firstChannel = luaL_checkint(L, -1);
        }
        output_parameters = output;
        
        //std::cout << output->nChannels << "is outputlll" << std::endl; 
        //std::cout << output->firstChannel << "is outputlll" << std::endl; 
    }else{
        if(lua_isnil(L,n)){
            output_parameters = NULL;
        }else{
            luaL_error(L,"arg %d should be a table or nil",n);
        }
    }
    return output_parameters;
}

int openStream_lua(lua_State *L)
{
    RtAudio *dac = lua_userdata_cast(L,1,RtAudio);
    RtAudio::StreamParameters *output_parameters = getParameters(L, 2);
    RtAudio::StreamParameters *input_parameters = getParameters(L, 3);
    //RtAudioFormat format = luaL_checkint(L, 4);
    unsigned int samplerate = luaL_checkint(L, 4);
    unsigned int bufferFrames = luaL_checkint(L, 5);
    //double data[2];
    //data[0]=0;data[1]=0;
    //double *data = (double *) calloc( 2, sizeof( double ) );
    RtAudio::StreamOptions options;
    options.flags = RTAUDIO_HOG_DEVICE;
    options.flags |= RTAUDIO_SCHEDULE_REALTIME;
    //options.flags |= RTAUDIO_NONINTERLEAVED;
	callback_info* cbinfo = new callback_info;
	//int *channels = new int;
	cbinfo->noutchannels = output_parameters->nChannels;
	cbinfo->sampleRate = samplerate;
	//std::cout << "nChannels " << *channels << std::endl;
    try {
        dac->openStream(output_parameters,input_parameters,RTAUDIO_FLOAT64,samplerate,&bufferFrames,cbMix,(void *)cbinfo,&options);
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
	delete output_parameters;
	delete input_parameters;
    lua_pushnumber(L, bufferFrames);
    return 1;
}
int startStream_lua(lua_State *L)
{
    RtAudio *dac = lua_userdata_cast(L,1,RtAudio);
    try {
        dac->startStream();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    return 0;
}

int stopStream_lua(lua_State *L)
{
    RtAudio *dac = lua_userdata_cast(L,1,RtAudio);
    try {
        dac->stopStream();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    return 0;
}

int abortStream_lua(lua_State *L)
{
    RtAudio *dac = lua_userdata_cast(L,1,RtAudio);
    try {
        dac->abortStream();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    return 0;
}

int closeStream_lua(lua_State *L)
{
    RtAudio *dac = lua_userdata_cast(L,1,RtAudio);
    try {
        dac->closeStream();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    return 0;
}

int getStreamTime_lua(lua_State *L)
{
    RtAudio *dac = lua_userdata_cast(L,1,RtAudio);
    double time;
    try {
        time = dac->getStreamTime();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    lua_pushnumber(L,time);
    return 1;
}

int getVersion_lua(lua_State *L)
{
    std::string str = RtAudio::getVersion();
    lua_pushstring(L, str.c_str());
    return 1;
}

int getDefaultOutputDevice_lua(lua_State *L)
{
    RtAudio *dac = lua_userdata_cast(L,1,RtAudio);
	unsigned int id;
    try {
        id = dac->getDefaultOutputDevice();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    lua_pushnumber(L,id);
    return 1;
}

int getDefaultInputDevice_lua(lua_State *L)
{
	RtAudio *dac = lua_userdata_cast(L,1,RtAudio);
    unsigned int id;
    try {
        id = dac->getDefaultInputDevice();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    lua_pushnumber(L,id);
    return 1;
}

int isStreamOpen_lua(lua_State *L)
{
	RtAudio *dac = lua_userdata_cast(L,1,RtAudio);
    bool open;
    try {
        open = dac->isStreamOpen();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    lua_pushboolean(L,open);
    return 1;
}

int isStreamRunning_lua(lua_State *L)
{
	RtAudio *dac = lua_userdata_cast(L,1,RtAudio);
    bool open;
    try {
        open = dac->isStreamRunning();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    lua_pushboolean(L,open);
    return 1;
}

int getStreamLatency_lua(lua_State *L)
{
	RtAudio *dac = lua_userdata_cast(L,1,RtAudio);
    long latency;
    try {
        latency = dac->getStreamLatency();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    lua_pushinteger(L,latency);
    return 1;
}


int getStreamSampleRate_lua(lua_State *L)
{
	RtAudio *dac = lua_userdata_cast(L,1,RtAudio);
    unsigned int sr;
    try {
        sr = dac->getStreamSampleRate();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    lua_pushinteger(L,sr);
    return 1;
}

int showWarnings_lua(lua_State *L)
{
	RtAudio *dac = lua_userdata_cast(L,1,RtAudio);
	if(!lua_isboolean(L, 2))
		luaL_error(L, "arg should be a boolean\n");
	bool show = lua_toboolean(L, 2);
    try {
        dac->showWarnings(show);
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    return 0;
}
#if defined(USE_SNDFILE)
#include<sndfile.h>

/*
typedef struct soundFileSt
{
	SNDFILE *sndfile=NULL;
	SF_INFO sf_info;
	std::string filename;
	double level;
}soundFileSt;
*/
class soundFileSt{
public:
	SNDFILE *sndfile=NULL;
	SF_INFO sf_info;
	std::string filename;
	double level;
	double timeoffset;
	soundFileSt():level(1),timeoffset(0.0){}
	inline double gettime(){
		return (double)sf_seek(this->sndfile, 0, SEEK_CUR)/(double)this->sf_info.samplerate;
	}
};
#define LIBSNDFILE "soundFileSt"

static std::set<soundFileSt *> playing_files;

int soundFile(lua_State *L)
{
	std::string str = luaL_checkstring(L, 1);
	//soundFileSt *sndf = (soundFileSt *)lua_newuserdata(L, sizeof(soundFileSt));
	//sndf = new(sndf) soundFileSt();
	//luaL_getmetatable(L, LIBSNDFILE);
    //lua_setmetatable(L, -2);
	soundFileSt *sndf = (soundFileSt *)lua_pushobject(L, soundFileSt)();
	
	sndf->filename = str;
	sndf->sf_info.format = 0;
	//std::cout << "trying to open " << str << std::endl;
	sndf->sndfile = sf_open(str.c_str(),SFM_READ,&(sndf->sf_info));
	if (sndf->sndfile == NULL){
		luaL_error(L, "Error opening %s :%s",str.c_str(),sf_strerror(NULL));
	}
	return 1;
}

int seek_sndfile(lua_State *L)
{
	soundFileSt *sndf = (soundFileSt *)luaL_checkudata(L, 1, LIBSNDFILE);
	int offset = luaL_checkinteger(L, 2);
	int type_seek;
	if (lua_gettop(L)==2){
		type_seek = SEEK_SET;
	}else{
		type_seek = luaL_checkinteger(L, 3);
	}
	int res = sf_seek(sndf->sndfile, offset, type_seek) ;
	if (res == -1)
		luaL_error(L,"error in seek");
	lua_pushinteger(L, res);
	return 1;
}

int close_sndfile(lua_State *L)
{

	soundFileSt *sndf = (soundFileSt *)luaL_checkudata(L, 1, LIBSNDFILE);
		//std::cout << "closing file " << sndf->filename << std::endl;
	playing_files.erase(sndf);
	int err = sf_close(sndf->sndfile);
	if(err!=0)
		luaL_error(L, "error closing file %s : %s",sndf->filename.c_str(),sf_error_number(err));
	return 0;
}

int destroy_sndfile(lua_State *L)
{
	soundFileSt *sndf = (soundFileSt *)luaL_checkudata(L, 1, LIBSNDFILE);
	playing_files.erase(sndf);
	int err = sf_close(sndf->sndfile);
	if(err!=0)
		luaL_error(L, "error closing file %s : %s",sndf->filename.c_str(),sf_error_number(err));
	sndf->~soundFileSt();
	return 0;
}

int info_sndfile(lua_State *L)
{
	soundFileSt *sndf = (soundFileSt *)luaL_checkudata(L, 1, LIBSNDFILE);
	lua_newtable(L);
	lua_pushstring(L, "frames");
    lua_pushinteger(L, sndf->sf_info.frames);
    lua_settable(L,-3);
	lua_pushstring(L, "samplerate");
    lua_pushinteger(L, sndf->sf_info.samplerate);
    lua_settable(L,-3);
	lua_pushstring(L, "channels");
    lua_pushinteger(L, sndf->sf_info.channels);
    lua_settable(L,-3);
	lua_pushstring(L, "format");
    lua_pushinteger(L, sndf->sf_info.format);
    lua_settable(L,-3);
	
	return 1;
}
 

int gettime_sndfile(lua_State *L)
{
	soundFileSt *sndf = (soundFileSt *)luaL_checkudata(L, 1, LIBSNDFILE);
	double time = sndf->gettime();//(double)sf_seek(sndf->sndfile, 0, SEEK_CUR)/(double)sndf->sf_info.samplerate;
	lua_pushnumber(L, time);
	return 1;
}

int play_sndfile(lua_State *L)
{
	soundFileSt *sndf = (soundFileSt *)luaL_checkudata(L, 1, LIBSNDFILE);
	if (lua_isnumber(L, 2))
		sndf->level = lua_tonumber(L, 2); 
	if (lua_isnumber(L, 3))
		sndf->timeoffset = lua_tonumber(L, 3); 
	playing_files.insert(sndf);
	return 0;
}

int stop_sndfile(lua_State *L)
{
	soundFileSt *sndf = (soundFileSt *)luaL_checkudata(L, 1, LIBSNDFILE);
	playing_files.erase(sndf);
	return 0;
}
//TODO dont read after end of file
bool playSoundFiles(double *outputBuffer,unsigned int nFrames,int channels, double streamTime, double windowsize)
{
	double BUFFER[nFrames*channels];
	memset(BUFFER,0,nFrames*channels*sizeof(double));
	for (std::set<soundFileSt*>::iterator it=playing_files.begin(); it!=playing_files.end(); ++it){
		if ((*it)->sf_info.channels == channels){
			if((*it)->timeoffset <= streamTime){ //already set
				unsigned int read = sf_readf_double((*it)->sndfile,BUFFER,nFrames);
				//if read < nFrames delete
				//std::cout << (*it)->filename << std::endl;
				for(int i=0; i< nFrames*channels; i++){
					outputBuffer[i] += BUFFER[i]*(*it)->level;
				}
			}else{
				if((*it)->timeoffset < streamTime + windowsize){ //set it if in window
					int frames = (streamTime + windowsize - (*it)->timeoffset) * (*it)->sf_info.samplerate;
					int res = sf_seek((*it)->sndfile, 0, SEEK_SET) ;
					if (res == -1)
						return false;
					unsigned int read = sf_readf_double((*it)->sndfile,BUFFER,frames);
					for(int i = (nFrames - frames)*channels,j=0; i< nFrames*channels; i++,j++){
						outputBuffer[i] += BUFFER[j]*(*it)->level;
					}
				}
			}
		}
	}
	return true;
}
static const struct luaL_Reg funcs_libsndfile[] = {
	{"stop", stop_sndfile },
	{"play", play_sndfile },
	{"close", close_sndfile },
	{"info", info_sndfile },
	{"seek", seek_sndfile},
	{"gettime", gettime_sndfile},
	{"__gc", destroy_sndfile },
    {NULL, NULL}
};
static const NamedConst libsndfile_const[] = {
    {"SEEK_SET",             SEEK_SET},
    {"SEEK_CUR",             SEEK_CUR}, 
    {"SEEK_END",             SEEK_END}, 
    { 0, 0}
};
#endif

int setStreamTime_lua(lua_State *L)
{
    RtAudio *dac = lua_userdata_cast(L,1,RtAudio);
    double time = (double)luaL_checknumber(L, 2);;
    try {
        dac->setStreamTime(time);
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
	#if defined(USE_SNDFILE)
	//set the soundfiles
	for (std::set<soundFileSt*>::iterator it=playing_files.begin(); it!=playing_files.end(); ++it){
		if ((*it)->timeoffset <= time){
			int frames = (time - (*it)->timeoffset) * (*it)->sf_info.samplerate;
			int res = sf_seek((*it)->sndfile, frames, SEEK_SET) ;
			//if (res == -1) //TODO will happen if seeking after end
			//	luaL_error(L,"error in setStreamTime_lua seek: %s",sf_strerror((*it)->sndfile));
		}
	}
	#endif
    return 0;
}
static const struct luaL_Reg funcs[] = {
    {"getDeviceCount", getDeviceCount_lua },
    {"getDeviceInfo", getDeviceInfo_lua },
    {"getCurrentApi", getCurrentApi_lua },
    {"openStream", openStream_lua },
    {"startStream", startStream_lua },
	{"stopStream", stopStream_lua },
	{"abortStream", abortStream_lua },
	{"closeStream", closeStream_lua },
    {"getStreamTime", getStreamTime_lua },
	{"setStreamTime", setStreamTime_lua },
	{"getDefaultOutputDevice",getDefaultOutputDevice_lua},
	{"getDefaultInputDevice",getDefaultInputDevice_lua},
	{"isStreamOpen",isStreamOpen_lua},
	{"isStreamRunning",isStreamRunning_lua},
	{"getStreamLatency",getStreamLatency_lua},
	{"getStreamSampleRate",getStreamSampleRate_lua},
	{"showWarnings",showWarnings_lua},
	{"__gc", Destroy },
    {NULL, NULL}
};
static const struct luaL_Reg thislib[] = {
    {"RtAudio", lua_RtAudio },
    {"getCompiledApi", getCompiledApi_lua},
    {"getVersion", getVersion_lua},
    {"setCallback",setCallback},
	{"setCallbackPost",setCallbackPost},
    {"setCallbackLanes",setCallbackLanes},
	{"setCallbackLanesPost",setCallbackLanesPost},
    {"getCallback",getCallback},
#if defined(USE_SNDFILE)
	{"soundFile",soundFile},
#endif
    {NULL, NULL}
};

extern "C" LUALIB_API int luaopen_RtAudio (lua_State* L) {

#if defined(USE_SNDFILE)
	luaL_newmetatable(L, LIBSNDFILE);
    /* metatable.__index = metatable */
    lua_pushvalue(L, -1); /* duplicates the metatable */
    lua_setfield(L, -2, "__index");
    luaL_register(L, NULL, funcs_libsndfile);
#endif
    //luaL_register(L, "RtAudio", funcs);
    luaL_newmetatable(L, RTAUDIO);
    /* metatable.__index = metatable */
    lua_pushvalue(L, -1); /* duplicates the metatable */
    lua_setfield(L, -2, "__index");
    luaL_register(L, NULL, funcs);
    luaL_register(L, "RtAudio", thislib);
    registerConstants(L, rtaudio_Const);
    registerConstants(L, rtaudio_formats);
#if defined(USE_SNDFILE)
	lua_newtable(L);		//RtAudio, table
	registerConstants(L, libsndfile_const);
	lua_pushstring(L, "libsndfile");//RtAudio, table libsndfile
    lua_pushvalue(L, -2);//RtAudio, table libsndfile table
    lua_settable(L,-4);//RtAudio, table
	lua_pop(L, 1);//RtAudio
#endif
    return 1;
}