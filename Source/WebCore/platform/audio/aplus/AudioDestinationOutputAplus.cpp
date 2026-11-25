#include "config.h"

#if ENABLE(WEB_AUDIO)
#include "AudioDestinationOutputAplus.h"
#include <wtf/FastMalloc.h>
#include <stdio.h>
#define dprintf(...) fprintf(stderr, __VA_ARGS__)

#undef AHI_BASE_NAME
#define AHI_BASE_NAME m_ahiBase

#define D(x) 

namespace WebCore {


AudioDestinationOutputAplus::AudioDestinationOutputAplus(AudioDestinationRenderer &parent, float sampleRate)
	: m_renderer(parent)
	, m_sampleRate(static_cast<uint32_t>(sampleRate))
{
}

AudioDestinationOutputAplus::~AudioDestinationOutputAplus()
{
	ahiCleanup();
}

bool AudioDestinationOutputAplus::start()
{
	if (!ahiInit())
		return false;

	auto locker = Locker(m_pumpLock);
	m_running = true;
	m_pumpCondition.notifyAll();
	return true;
}

void AudioDestinationOutputAplus::stop()
{
	auto locker = Locker(m_pumpLock);
	m_running = false;
	m_pumpCondition.notifyAll();
}

bool AudioDestinationOutputAplus::ahiInit()
{
	if (m_output)
		return true;

	m_ahiSampleLength = 1024;

	for (int i = 0; i < 2; i++)
	{
		m_buffer[i] = static_cast<int16_t *>(fastAlignedMalloc(16, (m_ahiSampleLength * 4) + 16));
		bzero(m_buffer[i], m_ahiSampleLength * 4);
	}

	m_output = makeUnique<AplusAudioOutput>("aplus-browser-webaudio", m_sampleRate);
	m_ahiSampleBeingPlayed = 0;
	m_ahiThreadShuttingDown = false;

	m_ahiThread = Thread::create("WebAudio Pump"_s, [this] {
		ahiThreadEntryPoint();
	});

	return true;
}

void AudioDestinationOutputAplus::ahiCleanup()
{
	if (m_ahiThread)
	{
		{
			auto locker = Locker(m_pumpLock);
			m_ahiThreadShuttingDown = true;
			m_running = false;
			m_pumpCondition.notifyAll();
		}
		m_ahiThread->waitForCompletion();
		m_ahiThread = nullptr;
	}

	m_output = nullptr;

	for (int i = 0; i < 2; i++)
	{
		if (m_buffer[i])
			fastFree(m_buffer[i]);
		m_buffer[i] = nullptr;
	}
}

void AudioDestinationOutputAplus::ahiThreadEntryPoint()
{
	for (;;)
	{
		{
			auto locker = Locker(m_pumpLock);
			while (!m_ahiThreadShuttingDown && !m_running)
				m_pumpCondition.waitFor(m_pumpLock, 100_ms);
			if (m_ahiThreadShuttingDown)
				break;
		}

		uint32_t index = m_ahiSampleBeingPlayed % 2;
		m_renderer.render(m_buffer[index], m_ahiSampleLength);
		m_ahiSampleBeingPlayed++;

		m_output->write(m_buffer[(index + 1) % 2], m_ahiSampleLength, [this] {
			return m_ahiThreadShuttingDown || !m_running;
		});
	}
}


}
#endif

