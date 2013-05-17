//////////////////////////////////////////////////////////////////////
//
//  Crytek (C) 2005
//
//  CrySound Source Code
//
//  File: IAudioDevice.h  __common interface for Xenon and FmodEx400
//  Description: Sound low level routines interface.
// 
//  History:
//  - April 6, 2005 created by Tomas Neumann
//  - September 30, 2009 Moved to CryCommon
//
//////////////////////////////////////////////////////////////////////
#ifndef __IAUDIODEVICE_H__
#define __IAUDIODEVICE_H__

#include <IStreamEngine.h> // find a way to forward-declare IReadStreamPtr instead of include

typedef void*					SoundHandle;
typedef void*					ProjectHandle;
const ProjectHandle		PROJECTHANDLE_INVALID = 0;
typedef int						ProjectIndex;
const ProjectIndex		PROJECTINDEX_INVALID = -1;


class CSoundSystem;
class CSoundAssetManager;
class CSoundBuffer;
struct IPlatformSound;
struct ISound;
struct SSoundBufferProps;
class CTimeValue;


// for audio device  param types for comunication with the game
enum enumAudioDeviceParamSemantics
  {
  adpHARDWARECHANNELS,
  adpTOTALCHANNELS,
  adpEAX_STATUS,
  adpMUTE_STATUS,
  adpLISTENER_POSITION,
  adpLISTENER_VELOCITY,
  adpLISTENER_FORWARD,   // the 4 3d listener attributes
  adpLISTENER_TOP,
  adpWINDOW_HANDLE,
  adpSYSTEM_FREQUENCY,  // mixing rate
  adpMASTER_VOLUME,	// for fmod value is 0-1, 1 being max volume
	adpMASTER_PAUSE,	// sets pause to master library
  adpSPEAKER_MODE,
  adpPAUSED_STATUS,
  adpCPU_USED_PCT,
  adpSNDSYSTEM_MEM_USAGE_NOW,
  adpSNDSYSTEM_MEM_USAGE_HIGHEST,
  adpSNDSYSTEM_OUTPUTTYPE,
  adpSNDSYSTEM_DRIVER,
  adpDOPPLER,            // turns doppler on and off
  adpMIXING_BUFFER_SIZE,   // fmod sets a default value this changes that value in millesecons
  adpSTOPALL_CHANNELS,
  adpCHANNELS_PLAYING,
	adpEVENTCOUNT,
	adpGROUPCOUNT,
	adpPITCH,
  };

// for audiodevice to query for memory usage
enum enumAudioDeviceMemoryQuery
{
	admqALL,
	admqSOUNDALL,
	admqSOUNDMAIN,
	admqSOUNDSECONDARY,
	admqSYSTEM,
	admqCACHEFILLALL,
	admqCACHEFILLMAIN,
	admqCACHEFILLSECONDARY,
	admqCACHESIZEALL,
	admqCACHESIZEMAIN,
	admqCACHESIZESECONDARY,
};

struct AudioDeviceSettings
{
	AudioDeviceSettings()
		:	nSoftwareChannels(0),
			nHWChannels(0),
			nMPEGDecoders(0),
			nXMADecoders(0),
			nADPCMDecoders(0),
			nSpeakerConfig(0),
			fCacheSize(0.0f),
			bCacheStatic(false){}

	int nSoftwareChannels;
	int nHWChannels;
	int nMPEGDecoders;
	int nXMADecoders;
	int nADPCMDecoders;
	int nSpeakerConfig;
	float fCacheSize;
	bool bCacheStatic;
};

struct IAudioDevice
{
	virtual ~IAudioDevice(){}
	// init and shutdown functions
	//friend class CSoundSystem;

	// this initializes the AudioDevice
	virtual bool InitDevice(CSoundSystem* pSoundSystem) = 0;

	// loads a project (FEV file) and returns handle if Project is available 
	virtual ProjectIndex LoadProjectFile(const char* sProjectPath, const char*sProjectName, bool bAsync) = 0;

	// releases the data of a project but keeps it known to the system 
	virtual bool UnloadProjectFile(const ProjectIndex nProjectIndex) = 0;

	// shut down and free audio device
	virtual bool ShutDownDevice(void) = 0;

	// returns ptr to the sound library if possible, else NULL
	virtual void* GetSoundLibrary(void) = 0;

	// returns ptr to the event system if possible, else NULL
	virtual void* GetEventSystem(void) const = 0;

	virtual void GetInitSettings(AudioDeviceSettings *InitSettings) = 0;
	virtual void SetInitSettings(AudioDeviceSettings *InitSettings) = 0;

	virtual void Reset(bool bForInit) = 0;

	// major system attrib has been changed del all sounds clean system start
	virtual bool ResetAudio(void) = 0;

	// this is called every frame to update all listeners and must be called *before* SubSystemUpdate()
	virtual bool UpdateListeners(void)=0;

	// this is called every frame to update audio device
	virtual bool Update(void)=0;

	// return current Mem_Usage
	virtual void GetMemoryStats(enumAudioDeviceMemoryQuery eMemoryQuery, unsigned int *nMemoryUsed) = 0; 

	// returns percent of cpu usage 1.0 is 100%
	virtual bool GetCpuUsage(float *fDSP, float *fStream, float* fGeometry, float* fUpdate, float *fTotal) = 0;

	// sets sound system global frequency this is the mixing value like fmod is 44100
	virtual bool SetFrequency(int newFreq)=0; 

	// this sets up info for how many 2d and 3d hardware channels are setup
	virtual int  GetNumberSoundsPlaying(void)=0;

	// compute memory-consumption, returns rough estimate in MB
	virtual int GetMemoryUsage(class ICrySizer* pSizer) const= 0;

	// returns number of open file handles opened by the AudioDevice
	virtual int GetNumOpenFileHandle() = 0;

	// accesses wavebanks
	virtual IWavebank* GetWavebank(int nIndex) = 0;
	virtual IWavebank* GetWavebank(const char* sWavebankName) = 0;
	virtual int GetWavebankCount() = 0;

	// this creates a platform dependent Soundbuffer
	virtual CSoundBuffer* CreateSoundBuffer(const SSoundBufferProps &BufferProps)=0;
	// this creates a platform dependent PlatformSound
	virtual IPlatformSound* CreatePlatformSound(ISound* pSound, const char *sEventName)=0;

	// check for multiple record devices and expose them, write name to sName pointer
	virtual bool GetRecordDeviceInfo(const int nRecordDevice, char* sName, const int nNameLength) = 0;

	// sound system settings *** this is main io for setting and getting all audio parameters
	virtual bool GetParam(enumAudioDeviceParamSemantics enumtype, ptParam* pParam)=0; // get enum for type of info and ptr type of info gotton
	// the bool returns false if this info is available on this type
	virtual bool SetParam(enumAudioDeviceParamSemantics enumType, ptParam* pParam)=0;

	// writes output to screen in debug
	virtual void DrawInformation(IRenderer* pRenderer, float xpos, float ypos, int nSoundInfo) = 0;

	// retrieves the bandwidth calculations for the audio streaming
	virtual void GetBandwidthStats(float * fBandwidthRequested) = 0;
};


#endif