#define SYSTEM_PRIVATE
#include "AcinerellaVideoDecoder.h"
#include "AcinerellaContainer.h"

#if ENABLE(VIDEO)

#include "GraphicsContext.h"
#include "GraphicsContextCairo.h"

#include <stdio.h>
#define dprintf(...) fprintf(stderr, __VA_ARGS__)

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include <cairo.h>


#define D(x)
#define DSYNC(x) 
#define DOVL(x) 
#define DFRAME(x) 

// #pragma GCC optimize ("O0")
// #define FORCEDECODE


namespace WebCore {
namespace Acinerella {

AcinerellaVideoDecoder::AcinerellaVideoDecoder(AcinerellaDecoderClient* client, RefPtr<AcinerellaPointer> acinerella, RefPtr<AcinerellaMuxedBuffer> buffer, int index, const ac_stream_info &info, bool isLiveStream, bool isHLS)
	: AcinerellaDecoder(client, acinerella, buffer, index, info, isLiveStream, isHLS)
{
	m_fps = info.additional_info.video_info.frames_per_second;
	m_frameDuration = 1.f / m_fps;
	m_frameWidth = info.additional_info.video_info.frame_width;
	m_frameHeight = info.additional_info.video_info.frame_height;
	D(dprintf("\033[35m[VD]%s: %p fps %f %dx%d\033[0m\n", __func__, this, float(m_fps), m_frameWidth, m_frameHeight));
	
	auto decoder = acinerella->decoder(index);
	ac_set_output_format(decoder, AC_OUTPUT_RGBA32);
	m_pullThread = Thread::create("Acinerella Video Pump"_s, [this] {
		pullThreadEntryPoint();
	});
	
}

AcinerellaVideoDecoder::~AcinerellaVideoDecoder()
{
	D(dprintf("\033[35m[VD]%s: %p\033[0m\n", __func__, this));
	if (!!m_pullThread)
	{
		m_pullEvent.signal();
		m_pullThread->waitForCompletion();
		m_pullThread = nullptr;
	}
}

void AcinerellaVideoDecoder::onDecoderChanged(RefPtr<AcinerellaPointer> acinerella)
{
	auto decoder = acinerella->decoder(m_index);
	ac_set_output_format(decoder, AC_OUTPUT_RGBA32);
    ac_decoder_set_loopfilter(decoder, int(m_client->streamSettings().m_loopFilter));
}

bool AcinerellaVideoDecoder::isReadyToPlay() const
{
	return isWarmedUp() && m_didShowFirstFrame;
}

bool AcinerellaVideoDecoder::isWarmedUp() const
{
	return (bufferSize() >= readAheadTime()) || m_decoderEOF;
}

bool AcinerellaVideoDecoder::isPlaying() const
{
	return m_playing;
}

double AcinerellaVideoDecoder::position() const
{
	return m_position;
}

void AcinerellaVideoDecoder::startPlaying()
{
	D(dprintf("\033[35m[VD]%s: %p ovw %p\033[0m\n", __func__, this, m_overlayWindow));
	if (isReadyToPlay())
	{
		m_playing = true;
		onPositionChanged();
        ac_decoder_set_loopfilter(m_lastDecoder, int(m_client->streamSettings().m_loopFilter));
		m_pullEvent.signal();
		m_frameEvent.signal();
	}
}

void AcinerellaVideoDecoder::onGetReadyToPlay()
{
	D(dprintf("\033[35m[VD]%s: %p\033[0m\n", __func__, this));
	m_client->onDecoderWantsToRender(Ref(*this));
}

void AcinerellaVideoDecoder::stopPlaying()
{
	D(dprintf("\033[35m[VD]%s: %p\033[0m\n", __func__, this));
	m_playing = false;
	m_pullEvent.signal();

	auto lock = Locker(m_audioLock);
	m_hasAudioPosition = false;
}

void AcinerellaVideoDecoder::onThreadShutdown()
{
	D(dprintf("\033[35m[VD]%s: %p\033[0m\n", __func__, this));
	m_pullEvent.signal();
	m_frameEvent.signal();
	D(dprintf("\033[35m[VD]%s: %p done\033[0m\n", __func__, this));
}

void AcinerellaVideoDecoder::onTerminate()
{
	D(dprintf("\033[35m[VD]%s: %p\033[0m\n", __func__, this));
	m_pullEvent.signal();
	m_frameEvent.signal();
	if (!!m_pullThread)
		m_pullThread->waitForCompletion();
	m_pullThread = nullptr;
	D(dprintf("\033[35m[VD]%s: %p done\033[0m\n", __func__, this));

	auto lock = Locker(m_lock);
}

void AcinerellaVideoDecoder::onFrameDecoded(const AcinerellaDecodedFrame &frame)
{
	m_bufferedSeconds += m_frameDuration;
	DFRAME(dprintf("\033[35m[VD]%s: %p [>> %f pts %f]\033[0m\n", __func__, this, float(m_bufferedSeconds), float(frame.pts())));

	auto *avframe = frame.frame();
	if (m_isHLS)// && avframe->timecode <= 0.0)
	{
		auto *nonconstframe = const_cast<ac_decoder_frame *>(avframe);
		nonconstframe->timecode = m_liveTimeCode;
		m_liveTimeCode += m_frameDuration;
	}

	m_pullEvent.signal();
	if (m_paintQueueFront && m_decodedFrames.empty())
	{
		dispatch([this] {
			m_client->onDecoderWantsToRender(Ref(*this));
		});
	}
	if (!m_didShowFirstFrame)
	{
	// caled under locks!
		showFirstFrame(false);
	}
}

void AcinerellaVideoDecoder::flush()
{
	DSYNC(dprintf("\033[35m[VD]%s: %p\033[0m\n", __func__, this));
	AcinerellaDecoder::flush();
	{
		auto lock = Locker(m_lock);
		m_paintQueueFront = true;
	}
	m_hasAudioPosition = false;
	m_bufferedSeconds = 0;
	m_frameCount = 0;
	m_liveTimeCode = 0;
}

void AcinerellaVideoDecoder::dumpStatus()
{
	auto lock = Locker(m_lock);
	dprintf("[\033[35mV]: WM %d IR %d PL %d BUF %f POS %f FIRSTFRAME %d DECFR %d LIVE %d EOF %d\033[0m\n",
		isWarmedUp(), isReadyToPlay(), isPlaying(), float(bufferSize()), float(position()), m_didShowFirstFrame, m_decodedFrames.size(), m_isLive, m_decoderEOF);
}

void AcinerellaVideoDecoder::setAudioPresentationTime(double apts)
{
	DSYNC(dprintf("\033[35m[VD]%s: %p -> %f\033[0m\n", __func__, this, float(apts)));
	float delta;
	{
		auto lock = Locker(m_audioLock);
		m_audioPositionRealTime = MonotonicTime::now();
		delta = fabs(m_audioPosition - apts);
		m_audioPosition = apts;
		m_hasAudioPosition = true;
	}

	if (delta > 2.0)
		m_frameEvent.signal(); // abort a possible long sleep
}

bool AcinerellaVideoDecoder::getAudioPresentationTime(double &time)
{
	auto lock = Locker(m_audioLock);

	if (m_hasAudioPosition)
	{
		// Audio should be here...
		time = m_audioPosition + (MonotonicTime::now() - m_audioPositionRealTime).value();
		
		// In case of a stall, don't over-report the audio position, but cause a stall on the video pipeline too!
		time = std::min(time, m_audioPosition + 1.5);
		
		return true;
	}
	
	DSYNC(dprintf("\033[35m[VD]%s: no audio pos\033[0m\n", __func__, this));
	
	return false;
}

void AcinerellaVideoDecoder::setOverlayWindowCoords(struct ::Window *w, int scrollx, int scrolly, int mleft, int mtop, int mright, int mbottom, int width, int height)
{
}

void AcinerellaVideoDecoder::showFirstFrame(bool locks)
{
	bool didShowFrame = false;

	if (locks)
	{
		auto lock = Locker(m_lock);
		if (m_decodedFrames.size())
		{
			m_position = m_decodedFrames.front().pts();
			blitFrameLocked();
			didShowFrame = true;
		}
	}
	else
	{
		if (m_decodedFrames.size())
		{
			m_position = m_decodedFrames.front().pts();
			blitFrameLocked();
			didShowFrame = true;
		}
	}

	if (didShowFrame)
	{
		m_didShowFirstFrame = true;
		onPositionChanged();
		onReadyToPlay();
	}
}

void AcinerellaVideoDecoder::onCoolDown()
{
	setOverlayWindowCoords(nullptr, 0, 0, 0, 0, 0, 0, 0, 0);
}

void AcinerellaVideoDecoder::updateOverlayCoords()
{
}

void AcinerellaVideoDecoder::paint(GraphicsContext& gc, const FloatRect& rect)
{
	auto lock = Locker(m_lock);
	AcinerellaDecodedFrame* paintFrame = nullptr;
	if (!m_paintQueueFront && m_currentFrame.frame())
	{
		paintFrame = &m_currentFrame;
	}
	else if (m_decodedFrames.size())
	{
		paintFrame = &m_decodedFrames.front();
	}
	else if (m_currentFrame.frame())
	{
		paintFrame = &m_currentFrame;
	}
	if (paintFrame)
	{
		WebCore::GraphicsContextCairo *context = gc.platformContext();
		cairo_t* cr = context->cr();



		// measurements of 360p video displayed inline 711x400 / 853x480 theather mode
		// 1.6Ghz -> 1100 us / 1400 us
		// optimization: ffmpeg is 3x faster when scaling to even width
		int corrwidth = rect.width(); if (corrwidth & 1) corrwidth++;
		ac_scale_to_scaled_rgb_decoder_frame(paintFrame->frame(), paintFrame->pointer()->decoder(m_index), corrwidth, rect.height());
		AVFrame *avFrame = ac_get_frame_scaled(paintFrame->pointer()->decoder(m_index));




		// 1.6Ghz ->  300 us /  450 us
		{
			auto surface = cairo_image_surface_create_for_data(avFrame->data[0], CAIRO_FORMAT_RGB24, corrwidth, rect.height(), avFrame->linesize[0]);
			if (surface)
			{
				cairo_save(cr);
				cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
				// optimizaton: operate on integer coords to allow for plain blit instead of re-scaling (300 us us vs 3000 us)
				cairo_translate(cr, (int)rect.x(), (int)rect.y());
				cairo_rectangle(cr, 0, 0, (int)rect.width(), (int)rect.height());
				// optimization: remove rounded-edge clip, saves ~3500 us, as blit is done to rectangle, not polygon
				cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
				cairo_set_source_surface(cr, surface, 0, 0);
				cairo_fill(cr);
				cairo_restore(cr);
				cairo_surface_destroy(surface);
			}
		}


	}
}

void AcinerellaVideoDecoder::blitFrameLocked()
{
#warning implement
}

void AcinerellaVideoDecoder::pullThreadEntryPoint()
{
	D(dprintf("\033[36m[VD]%s: %p\033[0m\n", __func__, this));

	while (!m_terminating)
	{
		m_pullEvent.waitFor(5_s);

		if ((!m_didShowFirstFrame || m_playing) && !m_terminating)
		{
			// D(dprintf("\033[36m[VD]%s: %p nf\033[0m\n", __func__, this));
			bool dropFrame = false;
			bool didShowFrame = false;

			while (m_playing && !m_terminating)
			{
				Seconds sleepFor = 0_s;
				double pts;

				// Grab time point (disregarding time it takes to swap vlayer buffers)
				auto timeDisplayed = MonotonicTime::now();

				{
					auto lock = Locker(m_lock);

					// Show previous frame
					if (dropFrame)
					{
						if (m_decodedFrames.size())
						{
							pts = m_decodedFrames.front().pts();
							m_position = pts;
							m_decodedFrames.pop();
							m_bufferedSeconds -= m_frameDuration;
							dropFrame = false;
							m_frameCount++;
							m_droppedFrameCount++;
						}
						else
						{
							break;
						}
					}
					else
					{
						if (m_decodedFrames.size())
						{
						    dispatch([this] {
							    m_client->onDecoderWantsToRender(Ref(*this));
						    });
							// Store current frame's pts
							pts = m_decodedFrames.front().pts();
							m_position = pts;
							
							// Blit the frame into overlay backbuffer
							blitFrameLocked();

							m_currentFrame = WTF::move(m_decodedFrames.front());
							m_paintQueueFront = false;
							m_decodedFrames.pop();
							m_bufferedSeconds -= m_frameDuration;

							m_frameCount++;
							didShowFrame = true;
						}
						else
						{
							break;
						}
					}
				}

				bool changePosition = 0 == (m_frameCount % int(m_fps));

				dispatch([this, changePosition, didShowFrame]() {
                    if (changePosition)
                    {
                        onPositionChanged();
                    }

					if (didShowFrame && !m_didShowFirstFrame)
					{
						m_didShowFirstFrame = true;
						onReadyToPlay();
					}

					decodeUntilBufferFull();
				});

				double audioAt = -1;
				bool canDropFrames = false;
				
				while (m_playing && !m_terminating)
				{
					{
						auto lock = Locker(m_lock);

						// Get next presentation time
						if (m_decodedFrames.size())
						{
							double nextPts = m_decodedFrames.front().pts();

							if (nextPts <= pts)
							{
								sleepFor = Seconds(m_frameDuration);
								sleepFor -= MonotonicTime::now() - timeDisplayed;
							}
							else
							{
								if (getAudioPresentationTime(audioAt))
								{
									sleepFor = Seconds((pts - audioAt) + (nextPts - pts));
									if (audioAt > 1.0)
										canDropFrames = true;
								}
								else
								{
									// Sleep for the duration of last frame - time we've already spent
									sleepFor = Seconds(nextPts - pts);
									sleepFor -= MonotonicTime::now() - timeDisplayed;
								}
							}

							DSYNC(dprintf("\033[36m[VD]%s%p: pts %f npts %f next frame in %f diff pts %f audio at %f\033[0m\n", "PTE", this, float(pts), float(nextPts), float(sleepFor.value()), float(nextPts - pts),
								float(audioAt)));
								
							break;
						}
					}

					m_pullEvent.waitFor(5_s);
				}

				if (sleepFor.value() > 0.0)
				{
					if (sleepFor.value() > 1.0)
					{
						DSYNC(dprintf("\033[36m[VD]%s: long sleep %f to catch to %f\033[0m\n", __func__, float(sleepFor.value()), float(audioAt)));
					}

					m_frameEvent.waitFor(sleepFor);
				}
				else if (m_canDropKeyFrames && canDropFrames && sleepFor.value() < -2.5)
				{
					DSYNC(dprintf("\033[36m[VD]%s: dropping video frames until %f\033[0m\n", __func__, float(audioAt) + 1.0));
					
					{
						auto lock = Locker(m_lock);
						while (m_decodedFrames.size())
						{
							auto pts = m_decodedFrames.front().pts();
							if (pts >= audioAt + 1.0)
								break;
							DSYNC(dprintf("\033[36m[VD]%s: droppped frame at %f\033[0m\n", __func__, float(pts)));
							m_decodedFrames.pop();
							m_bufferedSeconds -= m_frameDuration;
						}
					}
						
					dropUntilPTS(audioAt + 1.0);
				}
				else if (sleepFor.value() < -(m_frameDuration * 0.1))
				{
					dropFrame = true;
				}
			}
		}

	}
}

}
}

#endif
