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
#pragma once

#include "AudioBus.h"
#include "AudioDestination.h"
#include "AudioDestinationOutputAplus.h"

#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class AudioDestinationAplus final : public AudioDestination, public AudioDestinationRenderer, public ThreadSafeRefCounted<AudioDestinationAplus, WTF::DestructionThread::Main> {
public:
    static Ref<AudioDestinationAplus> create(const CreationOptions& options) { return adoptRef(*new AudioDestinationAplus(options)); }
    ~AudioDestinationAplus();

    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::deref(); }

    void start(Function<void(Function<void()>&&)>&& dispatchToRenderThread, CompletionHandler<void(bool)>&&) final;
    void stop(CompletionHandler<void(bool)>&&) final;

    bool isPlaying() final { return m_isPlaying; }
    unsigned framesPerBuffer() const final;

private:
    explicit AudioDestinationAplus(const CreationOptions&);

    void render(int16_t* samplesStereo, size_t count) final;
    void renderQuanta(std::span<int16_t> samplesStereo, size_t count);

    RefPtr<AudioBus> m_renderBus;
    AudioDestinationOutputAplus m_output;

    Lock m_dispatchToRenderThreadLock;
    Function<void(Function<void()>&&)> m_dispatchToRenderThread WTF_GUARDED_BY_LOCK(m_dispatchToRenderThreadLock);

    Vector<int16_t> m_dispatchBuffer;
    double m_sampleTime { 0 };
    std::atomic<bool> m_isPlaying { false };
};

}
