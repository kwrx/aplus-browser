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

#if ENABLE(MEDIA_SOURCE)

#include "FloatSize.h"
#include "MediaSourcePrivate.h"
#include <wtf/RunLoop.h>
#include <atomic>
#include <wtf/WeakPtr.h>

namespace WebCore {

class FloatRect;
class GraphicsContext;
class MediaPlayerPrivateAplus;
class MediaSourceClockAplus;
class SourceBufferPrivateAplus;

class MediaSourcePrivateAplus final : public MediaSourcePrivate {
public:
    static Ref<MediaSourcePrivateAplus> create(MediaPlayerPrivateAplus&, MediaSourcePrivateClient&);
    ~MediaSourcePrivateAplus();

    RefPtr<MediaPlayerPrivateInterface> player() const final;
    void setPlayer(MediaPlayerPrivateInterface*) final;
    constexpr MediaPlatformType platformType() const final { return MediaPlatformType::Aplus; }
    AddStatus addSourceBuffer(const ContentType&, const MediaSourceConfiguration&, RefPtr<SourceBufferPrivate>&) final;
    void removeSourceBuffer(SourceBufferPrivate&) final;
    void notifyActiveSourceBuffersChanged() final;
    void durationChanged(const MediaTime&) final;
    void markEndOfStream(EndOfStreamStatus) final;
    void unmarkEndOfStream() final;
    MediaTime currentTime() const final;
    bool timeIsProgressing() const final;

    Ref<MediaSourceClockAplus> clock() const;
    float volume() const { return m_muted ? 0 : m_volume; }
    bool isWaitingForReady() const { return m_waitReady; }
    void sourceBufferDecodersChanged();
    void sourceBufferReadyToPlay();
    void sourceBufferEnded();
    void videoFrameAvailable();

    void play();
    void pause();
    void willSeek(const MediaTime&);
    void seekCompleted(const MediaTime&);
    void setVolume(float);
    void setMuted(bool);
    void paint(GraphicsContext&, const FloatRect&);
    void terminate();

    bool paused() const { return m_paused; }
    bool ended() const { return m_ended; }
    bool hasAudioDecoder() const;
    bool hasVideoDecoder() const;
    FloatSize naturalSize() const;

private:
    MediaSourcePrivateAplus(MediaPlayerPrivateAplus&, MediaSourcePrivateClient&);

    Vector<Ref<SourceBufferPrivateAplus>> aplusSourceBuffers() const;
    bool areSourceBuffersReadyToPlay() const;
    bool areSourceBuffersEnded() const;
    void startPlaying();
    void checkEnded();
    void watchdogFired();
    void applyVolume();

    WeakPtr<MediaPlayerPrivateAplus> m_player;
    const Ref<MediaSourceClockAplus> m_clock;
    RunLoop::Timer m_watchdog;
    std::atomic<bool> m_paused { true };
    bool m_ended { false };
    bool m_waitReady { false };
    std::atomic<bool> m_seeking { false };
    std::atomic<double> m_seekTime { 0 };
    bool m_muted { false };
    float m_volume { 1 };
    FloatSize m_naturalSize;
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MediaSourcePrivateAplus)
static bool isType(const WebCore::MediaSourcePrivate& mediaSource) { return mediaSource.platformType() == WebCore::MediaPlatformType::Aplus; }
SPECIALIZE_TYPE_TRAITS_END()

#endif
