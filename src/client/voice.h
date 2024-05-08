#pragma once

#if defined(_WIN32)
#include <al.h>
#include <alc.h>
//#include <alext.h>
#elif defined(__APPLE__)
#define OPENAL_DEPRECATED
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
//#include <OpenAL/alext.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#endif

#include <vector>
#include <string>
#include <thread>
#include <memory>
#include "util/thread.h"
#include "../util/stream.h"

class Voice;

class VoiceThread : public Thread {
public:
	VoiceThread(Voice *voice);

	void* run();
	void stop() {
		Thread::stop();
	}

private:
	
	void streamMicrophoneOutput();

	Voice* voice = nullptr;
	
};

class Voice {
public:
	Voice(int sampleRate = 22050, int numChannels = 1);
	~Voice();

	bool init(); // Initialize recording
	bool start(std::shared_ptr<Stream>& stream); // Start recording
	bool stop(); // Stop recording

	inline const std::string& getCurrentDeviceName() {
		return m_inputDeviceName;
	}

	// Function to retrieve microphone input devices
	static std::vector<std::string> getInputDevices();

	// Function to set microphone input device
	bool setInputDevice(const std::string& deviceName);

	inline bool getIsRunning() {
		return isRunning;
	}

	int m_sampleRate;
	int m_numChannels;

private:
	void cleanup();

private:
	friend class VoiceThread;

	std::shared_ptr<Stream> stream = nullptr;
	std::string m_inputDeviceName;
	ALCdevice* m_device = nullptr;
	bool isRunning = false;
	VoiceThread* thread = nullptr;
};
