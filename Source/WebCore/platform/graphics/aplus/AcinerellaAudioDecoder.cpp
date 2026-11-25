#include "AcinerellaAudioDecoder.h"
#include "AcinerellaContainer.h"
#include "MediaPlayerAplus.h"

#if ENABLE(VIDEO)
#include <stdio.h>
#define dprintf(...) fprintf(stderr, __VA_ARGS__)


namespace WebCore {
namespace Acinerella {

// #pragma GCC optimize ("O0")

#undef AHI_BASE_NAME
#define AHI_BASE_NAME m_ahiBase

#define D(x)
#define DSYNC(x) 
#define DSPAM(x) 
#define DSPAMTS(x) 

AcinerellaAudioDecoder::AcinerellaAudioDecoder(AcinerellaDecoderClient* client, RefPtr<AcinerellaPointer> acinerella, RefPtr<AcinerellaMuxedBuffer> buffer, int index, const ac_stream_info &info, bool isLiveStream, bool isHLS)
	: AcinerellaDecoder(client, acinerella, buffer, index, info, isLiveStream, isHLS)
	, m_audioRate(ac_get_audio_rate(acinerella->decoder(index)))
	, m_audioChannels(info.additional_info.audio_info.channel_count)
	, m_audioBits(info.additional_info.audio_info.bit_depth)
{
	D(dprintf("[AD]%s: %p\n", __func__, this));
	for (int i = 0; i < 2; i++)
	{
		m_ahiSample[i].ahisi_Address = nullptr;
		m_ahiSampleTimestamp[i] = 0.f;
	}
}

void AcinerellaAudioDecoder::startPlaying()
{
	initializeAudio();

	if (m_output)
	{
		auto locker = Locker(m_pumpLock);
		m_playing = true;
		m_pumpCondition.notifyAll();
	}
}

void AcinerellaAudioDecoder::stopPlaying()
{
	auto locker = Locker(m_pumpLock);
	m_playing = false;
	m_pumpCondition.notifyAll();
}

void AcinerellaAudioDecoder::onCoolDown()
{
	ahiCleanup();
}

void AcinerellaAudioDecoder::doSetVolume(double volume)
{
	m_volume = volume;
}

bool AcinerellaAudioDecoder::isReadyToPlay() const
{
	return isWarmedUp() && m_output;
}

bool AcinerellaAudioDecoder::isWarmedUp() const
{
	return (bufferSize() >= readAheadTime()) || m_decoderEOF;
}

bool AcinerellaAudioDecoder::isPlaying() const
{
	return m_playing;
}


bool AcinerellaAudioDecoder::initializeAudio()
{
	if (m_output)
		return true;

	m_ahiSampleLength = std::max(256, m_audioRate / 20);

	for (int i = 0; i < 2; i++)
	{
		m_ahiSample[i].ahisi_Address = calloc(m_ahiSampleLength, 4);
		m_ahiSampleTimestamp[i] = 0.0;
	}

	if (!m_ahiSample[0].ahisi_Address || !m_ahiSample[1].ahisi_Address)
	{
		for (int i = 0; i < 2; i++)
		{
			free(m_ahiSample[i].ahisi_Address);
			m_ahiSample[i].ahisi_Address = nullptr;
		}
		return false;
	}

	m_output = makeUnique<AplusAudioOutput>("aplus-browser", m_audioRate);
	m_ahiSampleBeingPlayed = 0;
	m_ahiThreadShuttingDown = false;
	m_ahiFrameOffset = 0;
	m_didFlushBuffers = true;
	m_writtenEnd = -1.0;
	m_announcedPosition = -1.0;
	m_resetRequested = false;
	m_playing = false;

	m_ahiThread = Thread::create("Acinerella Audio Pump"_s, [this] {
		ahiThreadEntryPoint();
	});

	return true;
}

void AcinerellaAudioDecoder::onThreadShutdown()
{
	ahiCleanup();
}

void AcinerellaAudioDecoder::onGetReadyToPlay()
{
	D(dprintf("[AD]%s: waplay %d readying %d warmup %d warmedup %d\n", __func__, m_waitingToPlay, m_readying, m_warminUp, isWarmedUp()));
	initializeAudio();
}

void AcinerellaAudioDecoder::ahiCleanup()
{
	if (m_ahiThread)
	{
		{
			auto locker = Locker(m_pumpLock);
			m_ahiThreadShuttingDown = true;
			m_playing = false;
			m_pumpCondition.notifyAll();
		}
		m_ahiThread->waitForCompletion();
		m_ahiThread = nullptr;
	}

	m_output = nullptr;

	for (int i = 0; i < 2; i++)
	{
		free(m_ahiSample[i].ahisi_Address);
		m_ahiSample[i].ahisi_Address = nullptr;
	}
}

void AcinerellaAudioDecoder::onFrameDecoded(const AcinerellaDecodedFrame &frame)
{
	auto *avframe = frame.frame();
	m_bufferedSamples += avframe->buffer_size / 4; // 16bitStereo = 4BPF
	m_bufferedSeconds = double(m_bufferedSamples) / double(m_audioRate);

	if (m_isHLS)// && avframe->timecode <= 0.0)
	{
		auto *nonconstframe = const_cast<ac_decoder_frame *>(avframe);
		nonconstframe->timecode = m_liveTimeCode;
		m_liveTimeCode += (double((avframe->buffer_size / 4)) / double(m_audioRate));
	}

	DSPAM(dprintf("[AD]%s: buffered %f\n", __func__, m_bufferedSeconds));
}

double AcinerellaAudioDecoder::position() const
{
	return m_position;
}

void AcinerellaAudioDecoder::flush()
{
	{
		auto locker = Locker(m_pumpLock);

		AcinerellaDecoder::flush();

		m_ahiFrameOffset = 0;
		m_bufferedSeconds = 0;
		m_bufferedSamples = 0;
		m_didFlushBuffers = true;
		m_position = 0;
		m_liveTimeCode = 0;
		m_writtenEnd = -1.0;
		m_announcedPosition = -1.0;
		m_resetRequested = true;

		for (int i = 0; i < 2; i++)
		{
			if (m_ahiSample[i].ahisi_Address)
				bzero(m_ahiSample[i].ahisi_Address, m_ahiSampleLength * 4);
		}

		m_pumpCondition.notifyAll();
	}

	if (m_playing && m_output)
		decodeUntilBufferFull();
}

void AcinerellaAudioDecoder::dumpStatus()
{
	auto lock = Locker(m_lock);
	dprintf("[\033[33mA]: WM %d IR %d PL %d BUF %f BS %d DF %d POS %f LIV %d HLS %d\033[0m\n", isWarmedUp(), isReadyToPlay(), isPlaying(),
		float(bufferSize()), m_bufferedSamples, m_decodedFrames.size(), float(position()), m_isLive, m_isHLS);
}


void AcinerellaAudioDecoder::fillBuffer(int index)
{
	EP_SCOPE(fillBuffer);
	uint32_t offset = 0;
	uint32_t bytesLeft = m_ahiSampleLength * 4;
	bool didPopFrames = false;
	bool didSetTimecode = false;

	DSPAM(dprintf("[AD]%s: sample %d, next index: %d\n", __func__, m_ahiSampleBeingPlayed, index));

	{
		auto lock = Locker(m_lock);
		while (!m_decodedFrames.empty() && bytesLeft)
		{
			if (0 == bytesLeft)
				break;

			if (m_decodedFrames.empty())
				break;
				
			if (m_didUnderrun)
			{
				D(dprintf("[AD]%s: Underrun, %f buddered\n", __func__, float(bufferSize())));
				if (!isWarmedUp())
					break;
				m_didUnderrun = false;
			}

			const auto *frame = m_decodedFrames.front().frame();

			if (!m_isLive && m_position > frame->timecode)
			{
				m_bufferedSamples -= frame->buffer_size / 4; // 16bitStereo = 4BPF
				m_bufferedSeconds = double(m_bufferedSamples) / double(m_audioRate);
				m_decodedFrames.pop();
				didPopFrames = true;
				continue;
			}

			size_t copyBytes = std::min(frame->buffer_size - m_ahiFrameOffset, bytesLeft);
			
			DSPAM(dprintf("[AD]%s: cb %d of %d afo %d bs %d\n", __func__, copyBytes, offset, m_ahiFrameOffset, frame->buffer_size));
			
			memcpy(reinterpret_cast<uint8_t*>(m_ahiSample[index].ahisi_Address) + offset, frame->pBuffer + m_ahiFrameOffset, copyBytes);
			
			m_ahiFrameOffset += copyBytes;
			offset += copyBytes;
			bytesLeft -= copyBytes;

			if (!didSetTimecode)
			{
				m_ahiSampleTimestamp[index] = frame->timecode;
				if (m_didFlushBuffers) // index should always be 0 with initial
				{
					m_ahiSampleTimestamp[1] = m_ahiSampleTimestamp[0] = frame->timecode;
					m_didFlushBuffers = false;
				}
				didSetTimecode = true;
				DSPAMTS(dprintf("[AD]%s: set timecode %f\n", __func__, float(frame->timecode)));
			}
		
			if (frame->buffer_size == int(m_ahiFrameOffset))
			{
				DSPAM(dprintf("[AD]%s: pop frame sized %d at %f\n", __func__, frame->buffer_size, float(frame->timecode)));
				m_bufferedSamples -= frame->buffer_size / 4; // 16bitStereo = 4BPF
				m_bufferedSeconds = double(m_bufferedSamples) / double(m_audioRate);
				m_decodedFrames.pop();
				m_ahiFrameOffset = 0;
				didPopFrames = true;
			}
		}
	}

	if (bytesLeft > 0)
	{
		// Clear remaining buffer in case of an underrun
		bzero(reinterpret_cast<uint8_t*>(m_ahiSample[index].ahisi_Address) + offset, bytesLeft);
		m_didUnderrun = true;
	}


	{
		double positionToAnnounce = index == 0 ? m_ahiSampleTimestamp[1] : m_ahiSampleTimestamp[0];
		m_chunkTimed = didSetTimecode;
		if (m_announcedPosition >= 0.0)
			positionToAnnounce = m_announcedPosition;
		
		DSYNC(dprintf("[A] %s: PTS %f, bleft %d offset %d timeleft %f dp %d eof %d live %d\n", __func__, positionToAnnounce, bytesLeft, offset, m_bufferedSeconds, didPopFrames, m_decoderEOF, m_isLive));

		if (didPopFrames)
		{
			dispatch([this, positionToAnnounce, protectedThis(Ref(*this))]() {
				if (!m_ahiThreadShuttingDown)
				{
					m_position = positionToAnnounce;
					onPositionChanged();
					decodeUntilBufferFull();
				}
			});
		}
	}
	
	if ((bytesLeft == m_ahiSampleLength * 4) && (m_decoderEOF || (m_position + 1.5f > m_duration && !m_isHLS)))
	{
		dispatch([this, protectedThis(Ref(*this))]() {
			stopPlaying();
			if (!m_ahiThreadShuttingDown)
			{
				m_position = m_duration;
				onEnded();
			}
		});
	}
}

void AcinerellaAudioDecoder::ahiThreadEntryPoint()
{
	Vector<int16_t> chunk;

	for (;;)
	{
		{
			auto locker = Locker(m_pumpLock);

			while (!m_ahiThreadShuttingDown && !(m_playing && m_output))
				m_pumpCondition.waitFor(m_pumpLock, 100_ms);

			if (m_ahiThreadShuttingDown)
				break;

			if (m_resetRequested)
			{
				m_output->reset();
				m_resetRequested = false;
				m_writtenEnd = -1.0;
			}

			if (m_writtenEnd >= 0.0)
				m_announcedPosition = std::max(0.0, m_writtenEnd - m_output->latency());

			uint32_t index = m_ahiSampleBeingPlayed % 2;
			m_chunkTimed = false;
			fillBuffer(index);

			double chunkSeconds = double(m_ahiSampleLength) / double(m_audioRate);
			if (m_chunkTimed)
				m_writtenEnd = m_ahiSampleTimestamp[index] + chunkSeconds;
			else if (m_writtenEnd >= 0.0)
				m_writtenEnd += chunkSeconds;

			const int16_t *source = reinterpret_cast<const int16_t *>(m_ahiSample[index].ahisi_Address);
			double volume = m_volume;
			chunk.resize(m_ahiSampleLength * 2);
			if (volume >= 0.999)
			{
				memcpy(chunk.mutableSpan().data(), source, m_ahiSampleLength * 4);
			}
			else
			{
				int32_t scale = int32_t(std::max(0.0, volume) * 65536.0);
				for (size_t k = 0; k < chunk.size(); k++)
					chunk[k] = int16_t((int32_t(source[k]) * scale) >> 16);
			}

			m_ahiSampleBeingPlayed++;
		}

		m_output->write(chunk.span().data(), m_ahiSampleLength, [this] {
			return m_ahiThreadShuttingDown || !m_playing || m_resetRequested;
		});
	}
}

}
}

#undef D
#endif
