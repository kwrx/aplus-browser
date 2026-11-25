/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2026 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioDestinationAplus.h"

#include "AudioChannel.h"
#include <wtf/MainThread.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WebCore {

static constexpr unsigned framesToPull = 128;
static constexpr Seconds renderThreadTimeout = 250_ms;

class RenderSignal : public ThreadSafeRefCounted<RenderSignal> {
public:
    static Ref<RenderSignal> create() { return adoptRef(*new RenderSignal); }
    BinarySemaphore semaphore;
};

float AudioDestination::hardwareSampleRate()
{
    return 44100.f;
}

unsigned long AudioDestination::maxChannelCount()
{
    return 2;
}

Ref<AudioDestination> AudioDestination::create(const CreationOptions& options)
{
    return AudioDestinationAplus::create(options);
}

AudioDestinationAplus::AudioDestinationAplus(const CreationOptions& options)
    : AudioDestination(options)
    , m_renderBus(AudioBus::create(2, framesToPull, true))
    , m_output(*this, options.sampleRate)
{
    m_renderBus->setSampleRate(options.sampleRate);
}

AudioDestinationAplus::~AudioDestinationAplus() = default;

void AudioDestinationAplus::start(Function<void(Function<void()>&&)>&& dispatchToRenderThread, CompletionHandler<void(bool)>&& completionHandler)
{
    {
        Locker locker { m_dispatchToRenderThreadLock };
        m_dispatchToRenderThread = WTF::move(dispatchToRenderThread);
    }

    bool success = m_output.start();
    if (success) {
        m_isPlaying = true;
    }

    callOnMainThread([completionHandler = WTF::move(completionHandler), success]() mutable {
        completionHandler(success);
    });
}

void AudioDestinationAplus::stop(CompletionHandler<void(bool)>&& completionHandler)
{
    m_output.stop();
    m_isPlaying = false;

    {
        Locker locker { m_dispatchToRenderThreadLock };
        m_dispatchToRenderThread = nullptr;
    }

    callOnMainThread([completionHandler = WTF::move(completionHandler)]() mutable {
        completionHandler(true);
    });
}

unsigned AudioDestinationAplus::framesPerBuffer() const
{
    return m_renderBus->length();
}

void AudioDestinationAplus::render(int16_t* samplesStereo, size_t count)
{
    std::span<int16_t> output { samplesStereo, count * 2 };

    if (!m_isPlaying) {
        zeroSpan(output);
        return;
    }

    if (m_dispatchToRenderThreadLock.tryLock()) {
        Locker locker { AdoptLock, m_dispatchToRenderThreadLock };
        if (!m_dispatchToRenderThread) {
            renderQuanta(output, count);
            return;
        }

        if (m_dispatchBuffer.size() != output.size()) {
            m_dispatchBuffer.resize(output.size());
        }

        auto signal = RenderSignal::create();
        m_dispatchToRenderThread([protectedThis = Ref { *this }, count, signal] {
            protectedThis->renderQuanta(protectedThis->m_dispatchBuffer.mutableSpan(), count);
            signal->semaphore.signal();
        });

        if (signal->semaphore.waitFor(renderThreadTimeout)) {
            memcpySpan(output, m_dispatchBuffer.span());
        } else {
            zeroSpan(output);
        }
        return;
    }

    zeroSpan(output);
}

void AudioDestinationAplus::renderQuanta(std::span<int16_t> samplesStereo, size_t count)
{
    const size_t length = m_renderBus->length();
    size_t offset = 0;

    for (size_t frame = 0; frame + length <= count; frame += length) {
        AudioIOPosition position {
            Seconds { m_sampleTime / static_cast<double>(sampleRate()) },
            MonotonicTime::now()
        };
        m_sampleTime += length;

        callRenderCallback(*m_renderBus, length, position);

        auto* channelA = m_renderBus->channel(0);
        auto* channelB = m_renderBus->channel(1);
        auto out = samplesStereo.subspan(offset, length * 2);
        offset += length * 2;

        if (channelA->isSilent() && channelB->isSilent()) {
            zeroSpan(out);
            continue;
        }

        auto dataA = channelA->span();
        auto dataB = channelB->span();
        for (size_t sample = 0; sample < length; ++sample) {
            out[2 * sample] = static_cast<int16_t>(std::clamp(static_cast<int32_t>(dataA[sample] * 0x8000), -0x8000, 0x7FFF));
            out[2 * sample + 1] = static_cast<int16_t>(std::clamp(static_cast<int32_t>(dataB[sample] * 0x8000), -0x8000, 0x7FFF));
        }
    }

    if (offset < samplesStereo.size()) {
        zeroSpan(samplesStereo.subspan(offset));
    }
}

}

#endif
