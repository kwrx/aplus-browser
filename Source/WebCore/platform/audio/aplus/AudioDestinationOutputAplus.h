#pragma once
#include "AudioBus.h"
#include <wtf/MessageQueue.h>
#include <wtf/Threading.h>
#include <wtf/threads/BinarySemaphore.h>
#include "AplusAudioOutput.h"
#include <atomic>
#include <wtf/Condition.h>
#include <wtf/Lock.h>

namespace WebCore {

class AudioDestinationRenderer
{
public:
	virtual void render(int16_t *samplesStereo16s, size_t count) = 0;
};

class AudioDestinationOutputAplus
{
public:
	AudioDestinationOutputAplus(AudioDestinationRenderer &parent, float sampleRate);
	~AudioDestinationOutputAplus();

	bool start();
	void stop();


protected:
	bool ahiInit();
	void ahiCleanup();
	void ahiThreadEntryPoint();


protected:
	AudioDestinationRenderer& m_renderer;
	uint32_t        m_sampleRate;

	std::unique_ptr<AplusAudioOutput> m_output;
	int16_t        *m_buffer[2] = { nullptr, nullptr };
	uint32_t        m_ahiSampleLength = 0;
	uint32_t        m_ahiSampleBeingPlayed = 0;
	Lock            m_pumpLock;
	Condition       m_pumpCondition;
	std::atomic<bool> m_running { false };
	RefPtr<Thread>  m_ahiThread;
	std::atomic<bool> m_ahiThreadShuttingDown { false };
};

} // namespace
