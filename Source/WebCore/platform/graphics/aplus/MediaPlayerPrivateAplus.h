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

#if ENABLE(VIDEO)

#include "AcinerellaClient.h"
#include "MediaPlayerAplus.h"
#include "MediaPlayerPrivate.h"
#include "MediaPromiseTypes.h"
#include <wtf/NativePromise.h>
#include "PlatformTimeRanges.h"
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

namespace Acinerella {
class Acinerella;
}

class AudioTrackPrivateAplus;
class MediaSourcePrivateAplus;
class VideoTrackPrivateAplus;

class MediaPlayerPrivateAplus final : public MediaPlayerPrivateInterface, public Acinerella::AcinerellaClient, public CanMakeWeakPtr<MediaPlayerPrivateAplus>, public RefCounted<MediaPlayerPrivateAplus> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(MediaPlayerPrivateAplus);

public:
    static Ref<MediaPlayerPrivateAplus> create(MediaPlayer& player) { return adoptRef(*new MediaPlayerPrivateAplus(player)); }
    ~MediaPlayerPrivateAplus();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    static void registerMediaEngine(MediaEngineRegistrar);
    static void getSupportedTypes(HashSet<String>&);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    constexpr MediaPlayerType mediaPlayerType() const final { return MediaPlayerType::Aplus; }

    void load(const String&) final;
#if ENABLE(MEDIA_SOURCE)
    void load(const URL&, const LoadOptions&, MediaSourcePrivateClient&) final;
    void readyStateFromMediaSourceChanged() final;
    void characteristicsFromMediaSourceChanged() final;
    void mediaSourceHasRetrievedAllData() final;
    void notifyActiveSourceBuffersChanged() final;
    void mediaSourceFrameAvailable();
#endif
    RefPtr<MediaPlayer> mediaPlayer() const { return m_player.get(); }
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) final;
#endif
    void cancelLoad() final;
    void prepareToPlay() final;
    void play() final;
    void pause() final;

    bool supportsFullscreen() const final { return true; }
    bool canSaveMediaData() const final;

    FloatSize naturalSize() const final;
    bool hasVideo() const final;
    bool hasAudio() const final;
    void setPageIsVisible(bool) final;

    MediaTime duration() const final;
    MediaTime currentTime() const final;
    Ref<MediaTimePromise> seekToTarget(const SeekTarget&) final;
    bool paused() const final;
    bool ended() const final;

    void setVolume(float) final;
    void setMuted(bool) final;

    MediaPlayer::NetworkState networkState() const final { return m_networkState; }
    MediaPlayer::ReadyState readyState() const final;
    MediaPlayer::MovieLoadType movieLoadType() const final;

    MediaTime maxTimeSeekable() const final;
    const PlatformTimeRanges& buffered() const final;
    bool didLoadingProgress() const final;

    void paint(GraphicsContext&, const FloatRect&) final;
    DestinationColorSpace colorSpace() final;

    std::optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() final;

    const MediaPlayerAplusStreamSettings& streamSettings() final { return m_streamSettings; }
    void accInitialized(MediaPlayerAplusInfo) final;
    void accUpdated(MediaPlayerAplusInfo) final;
    void accSetNetworkState(MediaPlayerEnums::NetworkState) final;
    void accSetReadyState(MediaPlayerEnums::ReadyState) final;
    void accSetBufferLength(double) final;
    void accSetPosition(double) final;
    void accSetDuration(double) final;
    void accSetVideoSize(int width, int height) final;
    void accSetFrameCounts(unsigned decoded, unsigned dropped) final;
    void accEnded() final;
    void accFailed() final;
    void accNextFrameReady() final;
    void accNoFramesReady() final;
    void accFrameUpdateNeeded() final { }
    bool accCodecSupported(const String&) final;
    bool accIsURLValid(const String&) final;
    RefPtr<PlatformMediaResourceLoader> accCreateResourceLoader() final;
    String accReferrer() final;

private:
    explicit MediaPlayerPrivateAplus(MediaPlayer&);

    void setNetworkState(MediaPlayer::NetworkState);
    void setReadyState(MediaPlayer::ReadyState);
    void loadFailed();
    void resolveSeek();

    ThreadSafeWeakPtr<MediaPlayer> m_player;
    RefPtr<Acinerella::Acinerella> m_acinerella;
#if ENABLE(MEDIA_SOURCE)
    RefPtr<MediaSourcePrivateAplus> m_mediaSource;
#endif
    RefPtr<AudioTrackPrivateAplus> m_audioTrack;
    RefPtr<VideoTrackPrivateAplus> m_videoTrack;
    std::optional<MediaTimePromise::AutoRejectProducer> m_seekPromise;
    unsigned m_seekGeneration { 0 };
    MediaPlayer::NetworkState m_networkState { MediaPlayer::NetworkState::Empty };
    MediaPlayer::ReadyState m_readyState { MediaPlayer::ReadyState::HaveNothing };
    MediaPlayerAplusStreamSettings m_streamSettings;
    mutable PlatformTimeRanges m_buffered;
    HashSet<String> m_failedHLSStreamURIs;
    double m_duration { 0 };
    double m_currentTime { 0 };
    int m_width { 0 };
    int m_height { 0 };
    unsigned m_decodedFrameCount { 0 };
    unsigned m_droppedFrameCount { 0 };
    float m_volume { 1 };
    bool m_muted { false };
    bool m_prepareToPlay { false };
    bool m_acInitialized { false };
    bool m_visible { true };
    bool m_didDrawFrame { false };
    mutable bool m_didLoadingProgress { false };
};

}

#endif
