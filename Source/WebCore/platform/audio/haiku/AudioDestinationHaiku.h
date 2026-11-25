/*
 *  Copyright (C) 2014 Haiku, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef AudioDestinationHaiku_h
#define AudioDestinationHaiku_h

#include "AudioDestination.h"

#include "AudioBus.h"
#include <wtf/ThreadSafeWeakPtr.h>

namespace WebCore {

class AudioDestinationHaiku : public AudioDestination, public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<AudioDestinationHaiku, WTF::DestructionThread::Main> {
public:
    AudioDestinationHaiku(const CreationOptions& options);
    ~AudioDestinationHaiku();

    void ref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref(); }
    void deref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref(); }

    WEBCORE_EXPORT void start(Function<void(Function<void()>&&)>&& dispatchToRenderThread, CompletionHandler<void(bool)>&&) final;
    WEBCORE_EXPORT void stop(CompletionHandler<void(bool)>&&) final;

    bool isPlaying() override { return m_isPlaying; }
    unsigned framesPerBuffer() const final;

private:
    RefPtr<AudioBus> m_renderBus;

    float m_sampleRate;
    bool m_isPlaying;
};

} // namespace WebCore

#endif // AudioDestinationHaiku_h
