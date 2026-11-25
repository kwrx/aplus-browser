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
#include "MediaPlayerPrivateAplus.h"

#if ENABLE(VIDEO)

#include "AcinerellaContainer.h"
#include "AudioTrackPrivateAplus.h"
#include "ContentType.h"
#include "GraphicsContext.h"
#include "MediaPlayer.h"
#include "MediaSourcePrivateAplus.h"
#include "MediaSourcePrivateClient.h"
#include "PlatformMediaResourceLoader.h"
#include "SourceBufferPrivateAplus.h"
#include "VideoTrackPrivateAplus.h"
#include <wtf/text/MakeString.h>

namespace WebCore {

MediaPlayerAplusSettings& MediaPlayerAplusSettings::settings()
{
    static NeverDestroyed<MediaPlayerAplusSettings> settings;
    return settings;
}

class MediaPlayerFactoryAplus final : public MediaPlayerFactory {
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::Aplus; }

    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer& player) const final
    {
        return MediaPlayerPrivateAplus::create(player);
    }

    void getSupportedTypes(HashSet<String>& types) const final
    {
        MediaPlayerPrivateAplus::getSupportedTypes(types);
    }

    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        return MediaPlayerPrivateAplus::supportsType(parameters);
    }
};

void MediaPlayerPrivateAplus::registerMediaEngine(MediaEngineRegistrar registrar)
{
    registrar(makeUnique<MediaPlayerFactoryAplus>());
}

void MediaPlayerPrivateAplus::getSupportedTypes(HashSet<String>& types)
{
    static constexpr ASCIILiteral supported[] = {
        "audio/aac"_s, "audio/basic"_s, "audio/mp3"_s, "audio/mp4"_s, "audio/flac"_s, "audio/mpeg"_s,
        "audio/vnd.wave"_s, "audio/wav"_s, "audio/wave"_s, "audio/x-aiff"_s, "audio/x-flac"_s, "audio/x-m4a"_s,
        "audio/x-pn-wav"_s, "audio/x-wav"_s, "audio/ogg"_s, "audio/webm"_s, "audio/x-scpls"_s, "audio/mpa"_s,
        "audio/mpa-robust"_s, "audio/x-matroska"_s, "audio/opus"_s,
        "video/avi"_s, "video/flv"_s, "video/mp4"_s, "video/3gpp"_s, "video/vnd.objectvideo"_s, "video/x-flv"_s,
        "video/ogg"_s, "video/x-theora+ogg"_s, "video/webm"_s, "video/x-matroska"_s, "video/quicktime"_s, "video/x-m4v"_s,
        "audio/x-mpegurl"_s, "application/x-mpegurl"_s, "application/vnd.apple.mpegurl"_s,
    };

    for (auto type : supported) {
        types.add(type);
    }
}

MediaPlayer::SupportsType MediaPlayerPrivateAplus::supportsType(const MediaEngineSupportParameters& parameters)
{
    bool isMediaSource = parameters.platformType == PlatformMediaDecodingType::MediaSource;
    if (parameters.platformType != PlatformMediaDecodingType::FileOrHLS && !isMediaSource) {
        return MediaPlayer::SupportsType::IsNotSupported;
    }

    auto containerType = parameters.type.containerType();

    if (isMediaSource) {
        static constexpr ASCIILiteral mediaSourceTypes[] = { "video/mp4"_s, "audio/mp4"_s, "video/webm"_s, "audio/webm"_s };
        bool known = false;
        for (auto type : mediaSourceTypes) {
            if (equalIgnoringASCIICase(containerType, type)) {
                known = true;
            }
        }
        if (!known) {
            return MediaPlayer::SupportsType::IsNotSupported;
        }
    } else {
        auto& url = parameters.url.string();
        if (startsWithLettersIgnoringASCIICase(url, "data:"_s) || startsWithLettersIgnoringASCIICase(url, "blob:"_s)) {
            return MediaPlayer::SupportsType::IsNotSupported;
        }
        if (containerType.isEmpty()) {
            return MediaPlayer::SupportsType::MayBeSupported;
        }
    }

    if (startsWithLettersIgnoringASCIICase(containerType, "image/"_s)) {
        return MediaPlayer::SupportsType::IsNotSupported;
    }

    HashSet<String> types;
    getSupportedTypes(types);
    if (!types.contains(containerType.convertToASCIILowercase())) {
        return MediaPlayer::SupportsType::IsNotSupported;
    }

    auto codecs = parameters.type.codecs();
    if (codecs.isEmpty()) {
        return MediaPlayer::SupportsType::MayBeSupported;
    }

    for (auto& codec : codecs) {
        if (startsWithLettersIgnoringASCIICase(codec, "av01"_s) || startsWithLettersIgnoringASCIICase(codec, "av1"_s)) {
            return MediaPlayer::SupportsType::IsNotSupported;
        }
    }

    return MediaPlayer::SupportsType::IsSupported;
}

MediaPlayerPrivateAplus::MediaPlayerPrivateAplus(MediaPlayer& player)
    : m_player(player)
{
}

MediaPlayerPrivateAplus::~MediaPlayerPrivateAplus()
{
    if (m_acinerella) {
        m_acinerella->terminate();
    }
#if ENABLE(MEDIA_SOURCE)
    if (RefPtr mediaSource = std::exchange(m_mediaSource, nullptr)) {
        mediaSource->setPlayer(nullptr);
    }
#endif
}

void MediaPlayerPrivateAplus::setNetworkState(MediaPlayer::NetworkState state)
{
    if (m_networkState == state) {
        return;
    }
    m_networkState = state;
    if (RefPtr player = m_player.get()) {
        player->networkStateChanged();
    }
}

void MediaPlayerPrivateAplus::setReadyState(MediaPlayer::ReadyState state)
{
    if (m_readyState == state) {
        return;
    }
    m_readyState = state;
    if (RefPtr player = m_player.get()) {
        player->readyStateChanged();
    }
}

void MediaPlayerPrivateAplus::loadFailed()
{
    m_networkState = MediaPlayer::NetworkState::FormatError;
    m_readyState = MediaPlayer::ReadyState::HaveNothing;
    if (RefPtr player = m_player.get()) {
        player->networkStateChanged();
        player->readyStateChanged();
    }
}

void MediaPlayerPrivateAplus::load(const String& url)
{
    cancelLoad();

    if (startsWithLettersIgnoringASCIICase(url, "about:"_s)) {
        return;
    }

    m_networkState = MediaPlayer::NetworkState::Loading;
    m_readyState = MediaPlayer::ReadyState::HaveNothing;
    if (RefPtr player = m_player.get()) {
        player->networkStateChanged();
        player->readyStateChanged();
    }

    m_acinerella = Acinerella::Acinerella::create(this, url);
    if (!m_acinerella) {
        loadFailed();
    }
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateAplus::load(const URL&, const LoadOptions&, MediaSourcePrivateClient& client)
{
    cancelLoad();

    m_networkState = MediaPlayer::NetworkState::Loading;
    m_readyState = MediaPlayer::ReadyState::HaveNothing;
    if (RefPtr player = m_player.get()) {
        player->networkStateChanged();
        player->readyStateChanged();
    }

    if (RefPtr mediaSource = dynamicDowncast<MediaSourcePrivateAplus>(client.mediaSourcePrivate())) {
        mediaSource->setPlayer(this);
        m_mediaSource = WTF::move(mediaSource);
        client.reOpen();
    } else {
        m_mediaSource = MediaSourcePrivateAplus::create(*this, client);
    }
    m_mediaSource->setVolume(m_volume);
    m_mediaSource->setMuted(m_muted);
}

void MediaPlayerPrivateAplus::readyStateFromMediaSourceChanged()
{
    if (RefPtr player = m_player.get()) {
        player->readyStateChanged();
    }
}

void MediaPlayerPrivateAplus::characteristicsFromMediaSourceChanged()
{
    if (RefPtr player = m_player.get()) {
        player->characteristicChanged();
    }
}

void MediaPlayerPrivateAplus::mediaSourceHasRetrievedAllData()
{
    setNetworkState(MediaPlayer::NetworkState::Loaded);
}

void MediaPlayerPrivateAplus::notifyActiveSourceBuffersChanged()
{
    if (RefPtr player = m_player.get()) {
        player->activeSourceBuffersChanged();
    }
}

void MediaPlayerPrivateAplus::mediaSourceFrameAvailable()
{
    m_didLoadingProgress = true;
    accNextFrameReady();
}
#endif

MediaPlayer::ReadyState MediaPlayerPrivateAplus::readyState() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        return m_mediaSource->mediaPlayerReadyState();
    }
#endif
    return m_readyState;
}

#if ENABLE(MEDIA_STREAM)
void MediaPlayerPrivateAplus::load(MediaStreamPrivate&)
{
    loadFailed();
}
#endif

void MediaPlayerPrivateAplus::cancelLoad()
{
    m_prepareToPlay = false;
    m_acInitialized = false;
    m_didDrawFrame = false;

    if (auto seekPromise = std::exchange(m_seekPromise, std::nullopt)) {
        seekPromise->reject(PlatformMediaError::Cancelled);
    }

    if (RefPtr acinerella = std::exchange(m_acinerella, nullptr)) {
        acinerella->pause();
        acinerella->terminate();
    }

#if ENABLE(MEDIA_SOURCE)
    if (RefPtr mediaSource = std::exchange(m_mediaSource, nullptr)) {
        mediaSource->setPlayer(nullptr);
    }
#endif

    RefPtr player = m_player.get();
    if (m_audioTrack) {
        if (player) {
            player->removeAudioTrack(*m_audioTrack);
        }
        m_audioTrack = nullptr;
    }
    if (m_videoTrack) {
        if (player) {
            player->removeVideoTrack(*m_videoTrack);
        }
        m_videoTrack = nullptr;
    }

    m_duration = 0;
    m_currentTime = 0;
    m_width = 0;
    m_height = 0;
}

void MediaPlayerPrivateAplus::prepareToPlay()
{
    m_prepareToPlay = true;
    if (m_acinerella && m_acInitialized) {
        m_acinerella->warmUp();
    }
}

void MediaPlayerPrivateAplus::play()
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        m_mediaSource->play();
        if (RefPtr player = m_player.get()) {
            player->rateChanged();
            player->playbackStateChanged();
        }
        return;
    }
#endif
    if (!m_acinerella) {
        return;
    }

    m_acinerella->play();

    if (RefPtr player = m_player.get()) {
        player->rateChanged();
        player->playbackStateChanged();
    }
}

void MediaPlayerPrivateAplus::pause()
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        m_mediaSource->pause();
        return;
    }
#endif
    if (m_acinerella) {
        m_acinerella->pause();
    }
}

bool MediaPlayerPrivateAplus::canSaveMediaData() const
{
    return m_acinerella && !m_acinerella->isLive();
}

FloatSize MediaPlayerPrivateAplus::naturalSize() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        return m_mediaSource->naturalSize();
    }
#endif
    return { float(m_width), float(m_height) };
}

bool MediaPlayerPrivateAplus::hasVideo() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        return m_mediaSource->hasVideoDecoder() || m_mediaSource->hasVideo();
    }
#endif
    return m_acinerella && m_acinerella->hasVideo();
}

bool MediaPlayerPrivateAplus::hasAudio() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        return m_mediaSource->hasAudioDecoder() || m_mediaSource->hasAudio();
    }
#endif
    return m_acinerella && m_acinerella->hasAudio();
}

void MediaPlayerPrivateAplus::setPageIsVisible(bool visible)
{
    m_visible = visible;
}

MediaTime MediaPlayerPrivateAplus::duration() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        return m_mediaSource->duration();
    }
#endif
    if (m_acinerella && m_acinerella->isLive()) {
        return MediaTime::positiveInfiniteTime();
    }
    if (!m_duration) {
        return MediaTime::invalidTime();
    }
    return MediaTime::createWithDouble(m_duration);
}

MediaTime MediaPlayerPrivateAplus::currentTime() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        return m_mediaSource->currentTime();
    }
#endif
    return MediaTime::createWithDouble(m_currentTime);
}

Ref<MediaTimePromise> MediaPlayerPrivateAplus::seekToTarget(const SeekTarget& target)
{
    if (auto previous = std::exchange(m_seekPromise, std::nullopt)) {
        previous->reject(PlatformMediaError::Cancelled);
    }

#if ENABLE(MEDIA_SOURCE)
    if (RefPtr mediaSource = m_mediaSource) {
        m_seekPromise.emplace(PlatformMediaError::Cancelled);
        Ref promise = m_seekPromise->promise();
        unsigned generation = ++m_seekGeneration;
        mediaSource->willSeek(target.time);
        mediaSource->waitForTarget(target)->whenSettled(RunLoop::currentSingleton(), [weakThis = WeakPtr { *this }, mediaSource, generation](auto&& result) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || protectedThis->m_mediaSource != mediaSource || protectedThis->m_seekGeneration != generation) {
                return;
            }
            if (!result) {
                if (auto seekPromise = std::exchange(protectedThis->m_seekPromise, std::nullopt)) {
                    seekPromise->reject(result.error());
                }
                return;
            }
            auto seekTime = *result;
            mediaSource->willSeek(seekTime);
            mediaSource->reenqueueMediaForTime(seekTime)->whenSettled(RunLoop::currentSingleton(), [weakThis, mediaSource, seekTime, generation](auto&& result) {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis || protectedThis->m_mediaSource != mediaSource || protectedThis->m_seekGeneration != generation || !result) {
                    return;
                }
                mediaSource->seekCompleted(seekTime);
                if (auto seekPromise = std::exchange(protectedThis->m_seekPromise, std::nullopt)) {
                    seekPromise->resolve(seekTime);
                }
            });
        });
        return promise;
    }
#endif

    if (!m_acinerella || !m_acinerella->canSeek()) {
        return MediaTimePromise::createAndResolve(currentTime());
    }

    m_currentTime = target.time.toDouble();
    m_seekPromise.emplace(PlatformMediaError::Cancelled);
    Ref promise = m_seekPromise->promise();
    m_acinerella->seek(m_currentTime);
    return promise;
}

void MediaPlayerPrivateAplus::resolveSeek()
{
    if (!m_seekPromise || (m_acinerella && m_acinerella->isSeeking())) {
        return;
    }
    std::exchange(m_seekPromise, std::nullopt)->resolve(currentTime());
}

bool MediaPlayerPrivateAplus::paused() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        return m_mediaSource->paused();
    }
#endif
    return !m_acinerella || m_acinerella->paused();
}

bool MediaPlayerPrivateAplus::ended() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        return m_mediaSource->ended();
    }
#endif
    return !m_acinerella || m_acinerella->ended();
}

void MediaPlayerPrivateAplus::setVolume(float volume)
{
    m_volume = volume;
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        m_mediaSource->setVolume(volume);
    }
#endif
    if (m_acinerella) {
        m_acinerella->setVolume(volume);
    }
}

void MediaPlayerPrivateAplus::setMuted(bool muted)
{
    m_muted = muted;
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        m_mediaSource->setMuted(muted);
    }
#endif
    if (m_acinerella) {
        m_acinerella->setMuted(muted);
    }
}

MediaPlayer::MovieLoadType MediaPlayerPrivateAplus::movieLoadType() const
{
    if (m_acinerella && m_acinerella->isLive()) {
        return MediaPlayer::MovieLoadType::LiveStream;
    }
    return MediaPlayer::MovieLoadType::Download;
}

MediaTime MediaPlayerPrivateAplus::maxTimeSeekable() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        return m_mediaSource->duration();
    }
#endif
    if (m_acinerella && m_acinerella->canSeek()) {
        return MediaTime::createWithDouble(m_duration);
    }
    return MediaTime::zeroTime();
}

const PlatformTimeRanges& MediaPlayerPrivateAplus::buffered() const
{
    m_buffered.clear();
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        m_buffered = m_mediaSource->buffered();
        return m_buffered;
    }
#endif
    if (m_acinerella) {
        m_buffered.add(MediaTime::createWithDouble(std::max(0.0, m_currentTime - 1.0)), MediaTime::createWithDouble(std::min(m_duration ? m_duration : m_currentTime + 10.0, m_currentTime + 10.0)));
    }
    return m_buffered;
}

bool MediaPlayerPrivateAplus::didLoadingProgress() const
{
    return std::exchange(m_didLoadingProgress, false);
}

void MediaPlayerPrivateAplus::paint(GraphicsContext& context, const FloatRect& rect)
{
    if (context.paintingDisabled() || !m_visible) {
        return;
    }
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        m_mediaSource->paint(context, rect);
        return;
    }
#endif
    if (!m_acinerella) {
        return;
    }
    m_acinerella->paint(context, rect);
}

DestinationColorSpace MediaPlayerPrivateAplus::colorSpace()
{
    return DestinationColorSpace::SRGB();
}

std::optional<VideoPlaybackQualityMetrics> MediaPlayerPrivateAplus::videoPlaybackQualityMetrics()
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource) {
        m_mediaSource->clock()->videoFrameCounts(m_decodedFrameCount, m_droppedFrameCount);
    }
#endif
    VideoPlaybackQualityMetrics metrics;
    metrics.totalVideoFrames = m_decodedFrameCount + m_droppedFrameCount;
    metrics.droppedVideoFrames = m_droppedFrameCount;
    return metrics;
}

void MediaPlayerPrivateAplus::accInitialized(MediaPlayerAplusInfo info)
{
    RefPtr player = m_player.get();
    if (!player || !m_acinerella) {
        return;
    }

    if (info.m_width) {
        m_width = info.m_width;
        m_height = info.m_height;
    }

    m_acInitialized = true;

    if (info.m_width && !m_videoTrack) {
        m_videoTrack = VideoTrackPrivateAplus::create(1);
        player->addVideoTrack(*m_videoTrack);
    }

    if (info.m_channels && !m_audioTrack) {
        m_audioTrack = AudioTrackPrivateAplus::create(2);
        player->addAudioTrack(*m_audioTrack);
    }

    player->sizeChanged();
    player->characteristicChanged();
    setReadyState(MediaPlayer::ReadyState::HaveMetadata);

    if (m_prepareToPlay) {
        m_acinerella->warmUp();
    }
}

void MediaPlayerPrivateAplus::accUpdated(MediaPlayerAplusInfo info)
{
    if (info.m_width) {
        accSetVideoSize(info.m_width, info.m_height);
    }
}

void MediaPlayerPrivateAplus::accSetNetworkState(MediaPlayerEnums::NetworkState state)
{
    setNetworkState(state);
}

void MediaPlayerPrivateAplus::accSetReadyState(MediaPlayerEnums::ReadyState state)
{
    if (!m_acInitialized && state > MediaPlayer::ReadyState::HaveMetadata) {
        return;
    }
    setReadyState(state);
    if (state >= MediaPlayer::ReadyState::HaveEnoughData) {
        setNetworkState(MediaPlayer::NetworkState::Idle);
    }
}

void MediaPlayerPrivateAplus::accSetBufferLength(double)
{
    m_didLoadingProgress = true;
    if (RefPtr player = m_player.get()) {
        player->bufferedTimeRangesChanged();
        player->seekableTimeRangesChanged();
    }
}

void MediaPlayerPrivateAplus::accSetPosition(double position)
{
    if (m_seekPromise && m_acinerella && m_acinerella->isSeeking()) {
        return;
    }
    m_currentTime = position;
    resolveSeek();
    if (RefPtr player = m_player.get()) {
        player->timeChanged();
    }
}

void MediaPlayerPrivateAplus::accSetDuration(double duration)
{
    if (std::abs(duration - m_duration) < 0.5) {
        return;
    }
    m_duration = duration;
    if (RefPtr player = m_player.get()) {
        player->durationChanged();
    }
}

void MediaPlayerPrivateAplus::accSetVideoSize(int width, int height)
{
    if (m_width == width && m_height == height) {
        return;
    }
    m_width = width;
    m_height = height;
    if (RefPtr player = m_player.get()) {
        player->sizeChanged();
    }
}

void MediaPlayerPrivateAplus::accSetFrameCounts(unsigned decoded, unsigned dropped)
{
    m_decodedFrameCount = decoded;
    m_droppedFrameCount = dropped;
}

void MediaPlayerPrivateAplus::accEnded()
{
    if (m_duration) {
        m_currentTime = m_duration;
    }
    resolveSeek();
    if (RefPtr player = m_player.get()) {
        player->timeChanged();
        player->characteristicChanged();
        player->playbackStateChanged();
    }
}

void MediaPlayerPrivateAplus::accFailed()
{
    if (m_acinerella && m_acinerella->isLive() && !m_acinerella->hlsStreamURL().isEmpty()) {
        auto streamURL = m_acinerella->hlsStreamURL();
        if (!m_failedHLSStreamURIs.contains(streamURL)) {
            m_failedHLSStreamURIs.add(streamURL);
            String url = m_acinerella->url();
            m_acinerella->terminate();
            m_acinerella = Acinerella::Acinerella::create(this, url);
            if (m_acinerella) {
                return;
            }
        }
    }

    if (auto seekPromise = std::exchange(m_seekPromise, std::nullopt)) {
        seekPromise->reject(PlatformMediaError::Cancelled);
    }
    loadFailed();
}

void MediaPlayerPrivateAplus::accNextFrameReady()
{
    RefPtr player = m_player.get();
    if (!player) {
        return;
    }
    if (!m_didDrawFrame) {
        m_didDrawFrame = true;
        player->firstVideoFrameAvailable();
    }
    player->repaint();
}

void MediaPlayerPrivateAplus::accNoFramesReady()
{
    m_didDrawFrame = false;
    if (RefPtr player = m_player.get()) {
        player->repaint();
    }
}

bool MediaPlayerPrivateAplus::accCodecSupported(const String& codec)
{
    MediaEngineSupportParameters parameters;
    parameters.type = ContentType { makeString("video/mp4; codecs=\""_s, codec, '"') };
    return supportsType(parameters) == MediaPlayer::SupportsType::IsSupported;
}

bool MediaPlayerPrivateAplus::accIsURLValid(const String& url)
{
    return !m_failedHLSStreamURIs.contains(url);
}

RefPtr<PlatformMediaResourceLoader> MediaPlayerPrivateAplus::accCreateResourceLoader()
{
    if (RefPtr player = m_player.get()) {
        return player->mediaResourceLoader();
    }
    return nullptr;
}

String MediaPlayerPrivateAplus::accReferrer()
{
    if (RefPtr player = m_player.get()) {
        return player->referrer();
    }
    return { };
}

}

#endif
