#include "voice.h"

VoiceThread::VoiceThread(Voice* _voice) : voice(_voice) {}

void* VoiceThread::run()
{
	using namespace std::chrono_literals;

	BEGIN_DEBUG_EXCEPTION_HANDLER

		while (!stopRequested()) {
			streamMicrophoneOutput();
			std::this_thread::sleep_for(1ms);
		}

	voice->stream->flush();

	END_DEBUG_EXCEPTION_HANDLER

	return NULL;
}

void VoiceThread::streamMicrophoneOutput() {
	auto m_device = voice->m_device;
	if (!m_device) {
		return;
	}

	while (!stopRequested()) {
		ALCint samplesAvailable;
		alcGetIntegerv(m_device, ALC_CAPTURE_SAMPLES, sizeof(ALCint), &samplesAvailable);
		if (samplesAvailable <= 0)
			break;

		std::vector<short> samples(samplesAvailable * voice->m_numChannels);
		alcCaptureSamples(m_device, samples.data(), samplesAvailable);

		voice->stream->write((char*)samples.data(), samples.size()*sizeof(short));
	}
}

Voice::Voice(int sampleRate, int numChannels) : m_sampleRate(sampleRate), m_numChannels(numChannels) {}

Voice::~Voice() {
	cleanup();
}

void Voice::cleanup() {
	if (isRunning)
		stop();

	if (m_device) {
		alcCaptureCloseDevice(m_device);
	}
	m_device = nullptr;
}

bool Voice::init() {
	cleanup();

	if (m_inputDeviceName.empty()) {
		auto inputDevices = getInputDevices();
		if (inputDevices.empty())
			return false;
		m_inputDeviceName = inputDevices[0];
	}

	const ALCchar* deviceName = m_inputDeviceName.c_str();

	m_device = alcCaptureOpenDevice(
		deviceName,
		m_sampleRate,
		m_numChannels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16,
		4096);
	if (!m_device) {
		return false;
	}

	return true;
}

bool Voice::start(std::shared_ptr<Stream>& stream) {
	if (isRunning)
		return true;

	if (!m_device)
		if (!init())
			return false;

	if (!m_device)
		return false;

	isRunning = true;
	this->stream = stream;
	
	alcCaptureStart(m_device);
	thread = new VoiceThread(this);
	thread->start();
	return true;
}

bool Voice::stop() {
	if (!isRunning)
		return true;

	isRunning = false;

	thread->stop();
	thread->wait();

	delete thread;
	thread = nullptr;

	alcCaptureStop(m_device);
	
	return true;
}

std::vector<std::string> Voice::getInputDevices() {
	const ALCchar* devices = alcGetString(nullptr, ALC_CAPTURE_DEVICE_SPECIFIER);
	std::vector<std::string> deviceList;

	if (devices) {
		while (*devices) {
			deviceList.push_back(devices);

			std::string_view strv = devices;
			std::size_t len = strv.length();
			devices += len + 1;
		}
	}

	return deviceList;
}

bool Voice::setInputDevice(const std::string& deviceName) {
	if (deviceName.empty()) {
		return false;
	}

	m_inputDeviceName = deviceName;
	if (m_device)
		return init();

	return true;
}
