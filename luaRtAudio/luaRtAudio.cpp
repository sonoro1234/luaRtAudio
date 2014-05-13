extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
};



#include<iostream>
#include<cstring>
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
////////////////////////////
#if defined(USE_SNDFILE)
#include<sndfile.h>
struct DAC;
class soundFileSt{
public:
	SNDFILE *sndfile=NULL;
	SF_INFO sf_info;
	std::string filename;
	double level;
	double timeoffset;
	DAC * dac;
	soundFileSt():level(1),timeoffset(0.0),dac(NULL){}
	inline double gettime(){
		return (double)sf_seek(this->sndfile, 0, SEEK_CUR)/(double)this->sf_info.samplerate;
	}
};
#define LIBSNDFILE "soundFileSt"
#endif //USE_SNDFILE

#include "RtAudio.h"
#define RTAUDIO "RtAudio"

#if defined(USE_SNDFILE)
#include <set>
#endif

struct DAC
{
	RtAudio * rtaudio;
	RtAudio::StreamParameters* outparams;
	RtAudio::StreamParameters* inparams;
	unsigned int sampleRate;
	lua_State *callback_state;
	int thecallback_ref;
	lua_State *callback_state_post;
	int thecallback_post_ref;
#if defined(USE_SNDFILE)
	std::set<soundFileSt *> playing_files;
	SNDFILE *recordfile;
#endif
	//DAC(RtAudio::Api api=UNSPECIFIED):rtaudio(api),callback_state(NULL),thecallback_ref(0),callback_state_post(NULL),thecallback_post_ref(0){};
	DAC(RtAudio::Api api):callback_state(NULL),thecallback_ref(0),callback_state_post(NULL),thecallback_post_ref(0),sampleRate(0)
	{
		outparams = NULL;
		inparams = NULL;
		recordfile = NULL;
		rtaudio = new RtAudio(api);
	};
	~DAC(){
		if(this->rtaudio->isStreamOpen())
			this->rtaudio->closeStream();
		if(this->outparams)
			delete this->outparams;
		if(this->inparams)
			delete this->inparams;
		#if defined(USE_SNDFILE)
			if(this->recordfile){
				sf_write_sync(this->recordfile);
				sf_close(this->recordfile);
			}
		#endif
		this->rtaudio->~RtAudio();
	}
#if defined(USE_SNDFILE)
	bool playSoundFiles(double *outputBuffer,unsigned int nFrames,int channels, double streamTime,double windowsize);
#endif
};

typedef struct NamedConst  
{
  const char *str;
  unsigned int value;
} NamedConst;

#if defined(USE_SNDFILE)
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
	if(sndf->dac)
		sndf->dac->playing_files.erase(sndf);
	int err = sf_close(sndf->sndfile);
	if(err!=0)
		luaL_error(L, "error closing file %s : %s",sndf->filename.c_str(),sf_error_number(err));
	return 0;
}

int destroy_sndfile(lua_State *L)
{
	soundFileSt *sndf = (soundFileSt *)luaL_checkudata(L, 1, LIBSNDFILE);
	if(sndf->dac)
		sndf->dac->playing_files.erase(sndf);
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
	DAC *dac = lua_userdata_cast(L,2,DAC);
	bool open;
    try {
        open = dac->rtaudio->isStreamOpen();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
	if(!open || dac->outparams == NULL){
		luaL_error(L, "this dac is not opened for play\n");
		return 0;
	}
	if(sndf->sf_info.channels != dac->outparams->nChannels){
		luaL_error(L, "number of dac channels:%d and sndfile channels:%d dont match\n",dac->outparams->nChannels,sndf->sf_info.channels);
		return 0;
	}
	
	if(sndf->dac != NULL && sndf->dac != dac){
		luaL_error(L, "this sndfile is already opened in another dac\n");
		return 0;
	}else{
		sndf->dac = dac;
	}
	if (lua_isnumber(L, 3))
		sndf->level = lua_tonumber(L, 3); 
	if (lua_isnumber(L, 4))
		sndf->timeoffset = lua_tonumber(L, 4); 
	sndf->dac->playing_files.insert(sndf);
	return 0;
}


int record(lua_State *L){
	DAC *dac = lua_userdata_cast(L,1,DAC);
    std::string str = luaL_checkstring(L, 2);
	
	bool open;
    try {
        open = dac->rtaudio->isStreamOpen();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
	if(!open || dac->outparams == NULL){
		luaL_error(L, "this dac is not opened.\n");
		return 0;
	}
	
	SF_INFO sfinfo;
	sfinfo.samplerate = dac->sampleRate;
	sfinfo.channels = dac->outparams? dac->outparams->nChannels : dac->inparams->nChannels;
	sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_DOUBLE;
	dac->recordfile = sf_open(str.c_str(),SFM_WRITE,&sfinfo);
	if (dac->recordfile == NULL){
		luaL_error(L, "Error opening %s :%s",str.c_str(),sf_strerror(NULL));
	}
    return 0;
}

int stop_sndfile(lua_State *L)
{
	soundFileSt *sndf = (soundFileSt *)luaL_checkudata(L, 1, LIBSNDFILE);
	if(sndf->dac)
		sndf->dac->playing_files.erase(sndf);
	return 0;
}
//TODO dont read after end of file
bool DAC::playSoundFiles(double *outputBuffer,unsigned int nFrames,int channels, double streamTime, double windowsize)
{
	double BUFFER[nFrames*channels];
	memset(BUFFER,0,nFrames*channels*sizeof(double));
	for (std::set<soundFileSt*>::iterator it=this->playing_files.begin(); it!=this->playing_files.end(); ++it){
		if ((*it)->sf_info.channels == channels){ //it was already checked in sndfile:play but...
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
#endif //USE_SNDFILE





static int cbMix(void *outputBuffer, void *inputBuffer, unsigned int nFrames, double streamTime, RtAudioStreamStatus status, void *data){

	if(status)
		printf("RtAudio callback status %d\n",status);
		
	DAC *dac = (DAC *)data;
	
	if(inputBuffer && outputBuffer){ //duplex , must have the same channels
		memcpy( outputBuffer, inputBuffer, nFrames * dac->outparams->nChannels * sizeof(double) );
	}
	
	
	int channels = dac->outparams->nChannels; //*((int *)data);
	double windowsize = nFrames / dac->sampleRate;
	memset(outputBuffer,0,nFrames * channels * sizeof(double));
	
	if(outputBuffer){
		if (dac->callback_state!=0){
			lua_State *L = dac->callback_state;
			lua_rawgeti(L, LUA_REGISTRYINDEX, dac->thecallback_ref);
			lua_pushnumber(L, nFrames); /* push 1st argument */
			if (lua_pcall(L, 1, 1, 0) != 0){
				//printf("Error running callback function : %s\n",lua_tostring(L, -1));
				luaL_error(L, "error running callback function: %s",lua_tostring(L, -1));
				lua_pop(L,1);
				return 1;
			}
			if(!lua_istable(L,-1)){
				printf("error running callback function: did not returned table\n");
				//luaL_error(L, "error running function 'thecallback': did not returned table");
				return 1;
			}
			double *buffer = (double *) outputBuffer;
			//interleaved is more simple
			for (int i=1; i<=nFrames*channels; i++ ) {
				lua_rawgeti(L, -1,i);
				*buffer++ = (double)luaL_checknumber(L, -1);
				lua_pop(L, 1);
			}
			lua_pop(L, 1); /* pop returned value */
		}
#if defined(USE_SNDFILE)
		if(!dac->playSoundFiles((double *)outputBuffer,nFrames,channels,streamTime,windowsize)){
			printf("error on playSoundFiles");
			return 1;
		}
#endif
		if(dac->callback_state_post!=0){
			lua_State *L = dac->callback_state_post;
			lua_rawgeti(L, LUA_REGISTRYINDEX, dac->thecallback_post_ref);
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
				luaL_error(L, "error running callback post function : %s",lua_tostring(L, -1));
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
		if(dac->recordfile){
			sf_writef_double(dac->recordfile, (double *)outputBuffer, nFrames);
		}
	}
    return 0;
}

static int cbMixjit(void *outputBuffer, void *inputBuffer, unsigned int nFrames, double streamTime, RtAudioStreamStatus status, void *data){

	if(status)
		printf("RtAudio callback status %d\n",status);
		
	DAC *dac = (DAC *)data;
	
	if(inputBuffer && outputBuffer){ //duplex , must have the same channels
		memcpy( outputBuffer, inputBuffer, nFrames * dac->outparams->nChannels * sizeof(double) );
	}
	
	int channels = dac->outparams->nChannels; //*((int *)data);
	double windowsize = nFrames / dac->sampleRate;
	memset(outputBuffer,0,nFrames * channels * sizeof(double));
	
	if(outputBuffer){
		if (dac->callback_state!=0){
			lua_State *L = dac->callback_state;
			lua_rawgeti(L, LUA_REGISTRYINDEX, dac->thecallback_ref);
			lua_pushnumber(L, nFrames); /* push 1st argument */
			lua_pushlightuserdata (L, outputBuffer);
			if (lua_pcall(L, 2, 0, 0) != 0){
				//printf("Error running callback function : %s\n",lua_tostring(L, -1));
				luaL_error(L, "error running callback function: %s",lua_tostring(L, -1));
				lua_pop(L,1);
				return 1;
			}
		}
#if defined(USE_SNDFILE)
		if(!dac->playSoundFiles((double *)outputBuffer,nFrames,channels,streamTime,windowsize)){
			printf("error on playSoundFiles");
			return 1;
		}
#endif
		if(dac->callback_state_post!=0){
			lua_State *L = dac->callback_state_post;
			lua_rawgeti(L, LUA_REGISTRYINDEX, dac->thecallback_post_ref);
			lua_pushnumber(L, nFrames);
			lua_pushlightuserdata (L, outputBuffer);
			if (lua_pcall(L, 2, 0, 0) != 0){
				//printf("error running function %s: %s\n",thecallback_name_post.c_str(),lua_tostring(L, -1));
				luaL_error(L, "error running callback post function : %s",lua_tostring(L, -1));
				lua_pop(L,1);
				return 1;
			}
		}
		if(dac->recordfile){
			sf_writef_double(dac->recordfile, (double *)outputBuffer, nFrames);
		}
	}
    return 0;
}

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
        lua_pushobject(L, DAC)(RtAudio::UNSPECIFIED);
    else
        lua_pushobject(L, DAC)((RtAudio::Api)lua_tointeger(L,1));
    return 1;
}
int Destroy(lua_State *L)
{
	DAC *dac = lua_userdata_cast(L,1,DAC);
	dac->~DAC();
	return 0;
}
int getDeviceCount_lua(lua_State *L)
{
    DAC *dac = lua_userdata_cast(L,1,DAC);
    lua_pushinteger(L, dac->rtaudio->getDeviceCount());
    return 1;
}

int getDeviceInfo_lua(lua_State *L)
{
    DAC *dac = lua_userdata_cast(L,1,DAC);
    int ndevice = luaL_checkint(L, 2);
    RtAudio::DeviceInfo info;
    try {
        info = dac->rtaudio->getDeviceInfo(ndevice);
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
    DAC *dac = lua_userdata_cast(L,1,DAC);
    RtAudio::Api api;
    try {
        api = dac->rtaudio->getCurrentApi();
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
    
    if (lua_istable(L, n)){
        RtAudio::StreamParameters *output = new RtAudio::StreamParameters;
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
    int nargs = lua_gettop(L);
	DAC *dac = lua_userdata_cast(L,1,DAC);
    dac->outparams = getParameters(L, 2);
    dac->inparams = getParameters(L, 3);
    //RtAudioFormat format = luaL_checkint(L, 4);
    dac->sampleRate = luaL_checkint(L, 4);
    unsigned int bufferFrames = luaL_checkint(L, 5);
	
	int use_luajit = 0;
	if(nargs > 5){
		use_luajit = lua_toboolean(L, 6);
	}
	RtAudioCallback cb = (use_luajit==1) ? cbMixjit : cbMix;
	
    RtAudio::StreamOptions options;
    options.flags = RTAUDIO_HOG_DEVICE;
    options.flags |= RTAUDIO_SCHEDULE_REALTIME;
    //options.flags |= RTAUDIO_NONINTERLEAVED;
	
	if(!dac->outparams && !dac->inparams){
		luaL_error(L, "openStream has nil output and input\n");
		return 0;
	}
	
	//check channels match in duplex mode
	if(dac->outparams && dac->inparams)
		if(dac->outparams->nChannels != dac->inparams->nChannels){
			luaL_error(L, "out channels %d and input channels %d dont match\n", dac->outparams->nChannels, dac->inparams->nChannels);
			return 0;
		}

    try {
        dac->rtaudio->openStream(dac->outparams,dac->inparams,RTAUDIO_FLOAT64,dac->sampleRate,&bufferFrames,cb,(void *)dac,&options);
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }

    lua_pushnumber(L, bufferFrames);
    return 1;
}
int startStream_lua(lua_State *L)
{
    DAC *dac = lua_userdata_cast(L,1,DAC);
    try {
        dac->rtaudio->startStream();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    return 0;
}

int stopStream_lua(lua_State *L)
{
    DAC *dac = lua_userdata_cast(L,1,DAC);
    try {
        dac->rtaudio->stopStream();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    return 0;
}

int abortStream_lua(lua_State *L)
{
    DAC *dac = lua_userdata_cast(L,1,DAC);
    try {
        dac->rtaudio->abortStream();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    return 0;
}

int closeStream_lua(lua_State *L)
{
    DAC *dac = lua_userdata_cast(L,1,DAC);
    try {
        dac->rtaudio->closeStream();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
	delete dac->outparams;
	delete dac->inparams;
	dac->outparams = NULL;
	dac->inparams = NULL;
	
	#if defined(USE_SNDFILE)
	if(dac->recordfile){
		sf_write_sync(dac->recordfile);
		sf_close(dac->recordfile);
		dac->recordfile = NULL;
	}
	//set the soundfiles
	for (std::set<soundFileSt*>::iterator it=dac->playing_files.begin(); it!=dac->playing_files.end(); ++it){
		dac->playing_files.erase(*it);
	}
	#endif
    return 0;
}

int getStreamTime_lua(lua_State *L)
{
    DAC *dac = lua_userdata_cast(L,1,DAC);
    double time;
    try {
        time = dac->rtaudio->getStreamTime();
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
    DAC *dac = lua_userdata_cast(L,1,DAC);
	unsigned int id;
    try {
        id = dac->rtaudio->getDefaultOutputDevice();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    lua_pushnumber(L,id);
    return 1;
}

int getDefaultInputDevice_lua(lua_State *L)
{
	DAC *dac = lua_userdata_cast(L,1,DAC);
    unsigned int id;
    try {
        id = dac->rtaudio->getDefaultInputDevice();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    lua_pushnumber(L,id);
    return 1;
}

int isStreamOpen_lua(lua_State *L)
{
	DAC *dac = lua_userdata_cast(L,1,DAC);
    bool open;
    try {
        open = dac->rtaudio->isStreamOpen();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    lua_pushboolean(L,open);
    return 1;
}

int isStreamRunning_lua(lua_State *L)
{
	DAC *dac = lua_userdata_cast(L,1,DAC);
    bool open;
    try {
        open = dac->rtaudio->isStreamRunning();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    lua_pushboolean(L,open);
    return 1;
}

int getStreamLatency_lua(lua_State *L)
{
	DAC *dac = lua_userdata_cast(L,1,DAC);
    long latency;
    try {
        latency = dac->rtaudio->getStreamLatency();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    lua_pushinteger(L,latency);
    return 1;
}


int getStreamSampleRate_lua(lua_State *L)
{
	DAC *dac = lua_userdata_cast(L,1,DAC);
    unsigned int sr;
    try {
        sr = dac->rtaudio->getStreamSampleRate();
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    lua_pushinteger(L,sr);
    return 1;
}

int showWarnings_lua(lua_State *L)
{
	DAC *dac = lua_userdata_cast(L,1,DAC);
	if(!lua_isboolean(L, 2))
		luaL_error(L, "arg should be a boolean\n");
	bool show = lua_toboolean(L, 2);
    try {
        dac->rtaudio->showWarnings(show);
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
    return 0;
}


int setStreamTime_lua(lua_State *L)
{
    DAC *dac = lua_userdata_cast(L,1,DAC);
    double time = (double)luaL_checknumber(L, 2);;
    try {
        dac->rtaudio->setStreamTime(time);
    }catch ( RtAudioError& e ) {
        return luaL_error(L, e.what());
    }
	#if defined(USE_SNDFILE)
	//set the soundfiles
	for (std::set<soundFileSt*>::iterator it=dac->playing_files.begin(); it!=dac->playing_files.end(); ++it){
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


int RegisterLanesCallback(lua_State *L){
	if(!lua_isfunction(L,-1))
		luaL_error(L,"RegisterLanesCallback did not return a function\n");
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);
	lua_pushlightuserdata (L, L);
	lua_pushinteger(L, ref);
	return 2;
}

int setCallbackLanes(lua_State *L){
	//lua_gc(L, LUA_GCSTOP,0);
	DAC *dac = lua_userdata_cast(L,1,DAC);
	dac->thecallback_ref = luaL_checkinteger(L,3);
	if(!lua_islightuserdata(L, 2)){
		luaL_error(L,"second arg is not lightuserdata\n");
		return 0;
	}
	dac->callback_state = (lua_State *)lua_touserdata(L, 2);
	return 0;
}

int setCallbackLanesPost(lua_State *L){
	DAC *dac = lua_userdata_cast(L,1,DAC);
	dac->thecallback_post_ref = luaL_checkinteger(L,3);
	if(!lua_islightuserdata(L, 2)){
		luaL_error(L,"second arg is not lightuserdata\n");
		return 0;
	}
	dac->callback_state_post = (lua_State *)lua_touserdata(L, 2);
	return 0;
}

int setCallback(lua_State *L){
	size_t len;
	DAC *dac = lua_userdata_cast(L,1,DAC);
    const char *chunk = luaL_checklstring(L, 2, &len);
    lua_State *L1 = luaL_newstate();
    if (L1 == NULL)
        luaL_error(L, "unable to create new state");

    luaL_openlibs(L1); /* open standard libraries */
	//if (luaL_loadstring(L1, chunk) != 0)
	if (luaL_loadbuffer(L1, chunk, len, "setCallback chunk") != 0)
        luaL_error(L, "error loading chunk: %s",lua_tostring(L1, -1));
    if (lua_pcall(L1, 0, 1, 0) != 0){ /* call main chunk must return a function callback*/
        //fprintf(stderr, "thread error: %s", lua_tostring(L1, -1));
		luaL_error(L,"chunk was run with error: %s\n", lua_tostring(L1, -1));
		return 0;
	}
	if(!lua_isfunction(L1,-1))
		luaL_error(L,"chunk did not return a function\n");
	dac->thecallback_ref = luaL_ref(L1, LUA_REGISTRYINDEX);
    dac->callback_state = L1;
    return 0;
}

int setCallbackPost(lua_State *L){
	size_t len;
	DAC *dac = lua_userdata_cast(L,1,DAC);
    const char *chunk = luaL_checklstring(L, 2, &len);
    lua_State *L1 = luaL_newstate();
    if (L1 == NULL)
        luaL_error(L, "unable to create new state");
    if (luaL_loadbuffer(L1, chunk, len, "setCallback chunk") != 0)
        luaL_error(L, "error loading chunk: %s",lua_tostring(L1, -1));
    luaL_openlibs(L1); /* open standard libraries */
    if (lua_pcall(L1, 0, 1, 0) != 0) {/* call main chunk */
        //fprintf(stderr, "thread error: %s", lua_tostring(L1, -1));
		luaL_error(L,"chunk was run with error: %s\n", lua_tostring(L1, -1));
		return 0;
	}
	if(!lua_isfunction(L1,-1)){
		luaL_error(L,"chunk did not return a function\n");
		return 0;
	}
	dac->thecallback_post_ref = luaL_ref(L1, LUA_REGISTRYINDEX);
    dac->callback_state_post = L1;
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
	{"setCallback",setCallback},
	{"setCallbackPost",setCallbackPost},
    {"setCallbackLanes",setCallbackLanes},
	{"setCallbackLanesPost",setCallbackLanesPost},
#if defined(USE_SNDFILE)
	{"record",record},
#endif
	{"__gc", Destroy },
    {NULL, NULL}
};
static const struct luaL_Reg thislib[] = {
    {"RtAudio", lua_RtAudio },
    {"getCompiledApi", getCompiledApi_lua},
    {"getVersion", getVersion_lua},
	{"RegisterLanesCallback",RegisterLanesCallback},
#if defined(USE_SNDFILE)
	{"soundFile",soundFile},
#endif
    {NULL, NULL}
};

extern "C" LUALIB_API int luaopen_RtAudio_core (lua_State* L) {

#if defined(USE_SNDFILE)
	luaL_newmetatable(L, LIBSNDFILE);
    /* metatable.__index = metatable */
    lua_pushvalue(L, -1); /* duplicates the metatable */
    lua_setfield(L, -2, "__index");
    luaL_register(L, NULL, funcs_libsndfile);
#endif
    //luaL_register(L, "RtAudio", funcs);
    luaL_newmetatable(L, "DAC");
    /* metatable.__index = metatable */
    lua_pushvalue(L, -1); /* duplicates the metatable */
    lua_setfield(L, -2, "__index");
    luaL_register(L, NULL, funcs);
    luaL_register(L, "DAC", thislib);
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