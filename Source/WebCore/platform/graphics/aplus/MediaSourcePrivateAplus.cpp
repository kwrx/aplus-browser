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
#include "MediaSourcePrivateAplus.h"

#if ENABLE(MEDIA_SOURCE)

#include "ContentType.h"
#include "MediaPlayer.h"
#include "MediaPlayerPrivateAplus.h"
#include "MediaSourcePrivateClient.h"
#include "SourceBufferPrivateAplus.h"
#include <wtf/MainThread.h>

#include <wtf/MonotonicTime.h>

namespace WebCore {

Ref<MediaSourcePrivateAplus> MediaSourcePrivateAplus::create(MediaPlayerPrivateAplus& player, MediaSourcePrivateClient& client)
{
    Ref mediaSource = adoptRef(*new MediaSourcePrivateAplus(player, client));
    client.setPrivateAndOpen(mediaSource.copyRef());
    return mediaSource;
}

MediaSourcePrivateAplus::MediaSourcePrivateAplus(MediaPlayerPrivateAplus& player, MediaSourcePrivateClient& client)
    : MediaSourcePrivate(client)
    , m_player(player)
    , m_clock(MediaSourceClockAplus::create())
    , m_watchdog(RunLoop::mainSingleton(), "MediaSourcePrivateAplus::watchdog"_s, [this] { watchdogFired(); })
{
}

MediaSourcePrivateAplus::~MediaSourcePrivateAplus()
{
    terminate();
}

RefPtr<MediaPlayerPrivateInterface> MediaSourcePrivateAplus::player() const
{
    return m_player.get();
}

void MediaSourcePrivateAplus::setPlayer(MediaPlayerPrivateInterface* player)
{
    if (!player) {
        pause();
        m_player = nullptr;
        return;
    }
    m_player = static_cast<MediaPlayerPrivateAplus*>(player);
}

Ref<MediaSourceClockAplus> MediaSourcePrivateAplus::clock() const
{
    return m_clock;
}

MediaSourcePrivate::AddStatus MediaSourcePrivateAplus::addSourceBuffer(const ContentType& contentType, const MediaSourceConfiguration&, RefPtr<SourceBufferPrivate>& outPrivate)
{
    MediaEngineSupportParameters parameters;
    parameters.platformType = PlatformMediaDecodingType::MediaSource;
    parameters.type = contentType;
    if (MediaPlayerPrivateAplus::supportsType(parameters) == MediaPlayer::SupportsType::IsNotSupported) {
        return AddStatus::NotSupported;
    }

    Ref sourceBuffer = SourceBufferPrivateAplus::create(*this);
    {
        Locker locker { m_lock };
        m_sourceBuffers.append(sourceBuffer.copyRef());
    }
    outPrivate = sourceBuffer.copyRef();
    outPrivate->setMediaSourceDuration(duration());
    return AddStatus::Ok;
}

void MediaSourcePrivateAplus::removeSourceBuffer(SourceBufferPrivate& sourceBuffer)
{
    if (RefPtr aplus = dynamicDowncast<SourceBufferPrivateAplus>(sourceBuffer)) {
        aplus->terminateDecoders();
    }
    MediaSourcePrivate::removeSourceBuffer(sourceBuffer);
    sourceBufferDecodersChanged();
}

Vector<Ref<SourceBufferPrivateAplus>> MediaSourcePrivateAplus::aplusSourceBuffers() const
{
    Vector<Ref<SourceBufferPrivateAplus>> result;
    for (auto& sourceBuffer : sourceBuffers()) {
        if (RefPtr aplus = dynamicDowncast<SourceBufferPrivateAplus>(sourceBuffer.get())) {
            result.append(aplus.releaseNonNull());
        }
    }
    return result;
}

void MediaSourcePrivateAplus::notifyActiveSourceBuffersChanged()
{
    ensureOnMainThread([weakThis = ThreadSafeWeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            return;
        }
        if (RefPtr player = protectedThis->m_player.get()) {
            if (RefPtr mediaPlayer = player->mediaPlayer()) {
                mediaPlayer->activeSourceBuffersChanged();
            }
        }
    });
}

void MediaSourcePrivateAplus::durationChanged(const MediaTime& duration)
{
    MediaSourcePrivate::durationChanged(duration);
    ensureOnMainThread([weakThis = ThreadSafeWeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            return;
        }
        if (RefPtr player = protectedThis->m_player.get()) {
            if (RefPtr mediaPlayer = player->mediaPlayer()) {
                mediaPlayer->durationChanged();
            }
        }
    });
}

void MediaSourcePrivateAplus::markEndOfStream(EndOfStreamStatus status)
{
    MediaSourcePrivate::markEndOfStream(status);
    ensureOnMainThread([weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get()) {
            protectedThis->checkEnded();
        }
    });
}

void MediaSourcePrivateAplus::unmarkEndOfStream()
{
    MediaSourcePrivate::unmarkEndOfStream();
    m_ended = false;
}

MediaTime MediaSourcePrivateAplus::currentTime() const
{
    return MediaTime::createWithDouble(m_seeking ? m_seekTime.load() : m_clock->position());
}

bool MediaSourcePrivateAplus::timeIsProgressing() const
{
    return !m_paused && !m_seeking && !m_waitReady && hasFutureTime(currentTime());
}

bool MediaSourcePrivateAplus::hasAudioDecoder() const
{
    for (auto& sourceBuffer : aplusSourceBuffers()) {
        if (sourceBuffer->hasAudioDecoder()) {
            return true;
        }
    }
    return false;
}

bool MediaSourcePrivateAplus::hasVideoDecoder() const
{
    for (auto& sourceBuffer : aplusSourceBuffers()) {
        if (sourceBuffer->hasVideoDecoder()) {
            return true;
        }
    }
    return false;
}

FloatSize MediaSourcePrivateAplus::naturalSize() const
{
    for (auto& sourceBuffer : aplusSourceBuffers()) {
        if (sourceBuffer->hasVideoDecoder()) {
            auto size = sourceBuffer->videoSize();
            if (!size.isEmpty()) {
                return size;
            }
        }
    }
    return m_naturalSize;
}

bool MediaSourcePrivateAplus::areSourceBuffersReadyToPlay() const
{
    auto sourceBuffers = aplusSourceBuffers();
    if (sourceBuffers.isEmpty()) {
        return false;
    }
    for (auto& sourceBuffer : sourceBuffers) {
        if (!sourceBuffer->areDecodersReadyToPlay()) {
            return false;
        }
    }
    return true;
}

bool MediaSourcePrivateAplus::areSourceBuffersEnded() const
{
    bool anyAudio = false;
    for (auto& sourceBuffer : aplusSourceBuffers()) {
        if (sourceBuffer->hasAudioDecoder()) {
            anyAudio = true;
            if (!sourceBuffer->areDecodersEnded()) {
                return false;
            }
        }
    }
    return anyAudio;
}

void MediaSourcePrivateAplus::sourceBufferDecodersChanged()
{
    m_clock->setHasAudio(hasAudioDecoder());
    applyVolume();
    if (RefPtr player = m_player.get()) {
        if (RefPtr mediaPlayer = player->mediaPlayer()) {
            mediaPlayer->characteristicChanged();
            mediaPlayer->sizeChanged();
        }
    }
    if (!m_paused && !m_seeking) {
        startPlaying();
    }
}

void MediaSourcePrivateAplus::sourceBufferReadyToPlay()
{
    if (!m_waitReady || m_paused || m_seeking) {
        return;
    }
    if (areSourceBuffersReadyToPlay()) {
        startPlaying();
    }
}

void MediaSourcePrivateAplus::sourceBufferEnded()
{
    checkEnded();
}

void MediaSourcePrivateAplus::videoFrameAvailable()
{
    RefPtr player = m_player.get();
    if (!player) {
        return;
    }
    auto size = naturalSize();
    if (size != m_naturalSize) {
        m_naturalSize = size;
        if (RefPtr mediaPlayer = player->mediaPlayer()) {
            mediaPlayer->sizeChanged();
        }
    }
    player->mediaSourceFrameAvailable();
}

void MediaSourcePrivateAplus::startPlaying()
{
    m_ended = false;
    auto sourceBuffers = aplusSourceBuffers();
    if (areSourceBuffersReadyToPlay()) {
        m_waitReady = false;
        for (auto& sourceBuffer : sourceBuffers) {
            sourceBuffer->play();
        }
    } else {
        m_waitReady = true;
        for (auto& sourceBuffer : sourceBuffers) {
            sourceBuffer->prePlay();
        }
    }
    if (!m_watchdog.isActive()) {
        m_watchdog.startRepeating(250_ms);
    }
}

void MediaSourcePrivateAplus::play()
{
    m_paused = false;
    if (!m_seeking) {
        startPlaying();
    }
}

void MediaSourcePrivateAplus::pause()
{
    m_paused = true;
    m_waitReady = false;
    m_watchdog.stop();
    for (auto& sourceBuffer : aplusSourceBuffers()) {
        sourceBuffer->pause();
    }
}

void MediaSourcePrivateAplus::willSeek(const MediaTime& time)
{
    m_seekTime = time.toDouble();
    m_seeking = true;
    m_ended = false;
    m_waitReady = false;
    for (auto& sourceBuffer : aplusSourceBuffers()) {
        sourceBuffer->willSeek(time.toDouble());
    }
    m_clock->setPosition(time.toDouble());
}

void MediaSourcePrivateAplus::seekCompleted(const MediaTime& time)
{
    m_clock->setPosition(time.toDouble());
    m_seeking = false;
    if (!m_paused) {
        startPlaying();
    }
}

void MediaSourcePrivateAplus::setVolume(float volume)
{
    m_volume = volume;
    applyVolume();
}

void MediaSourcePrivateAplus::setMuted(bool muted)
{
    m_muted = muted;
    applyVolume();
}

void MediaSourcePrivateAplus::applyVolume()
{
    for (auto& sourceBuffer : aplusSourceBuffers()) {
        sourceBuffer->setVolume(volume());
    }
}

void MediaSourcePrivateAplus::paint(GraphicsContext& context, const FloatRect& rect)
{
    for (auto& sourceBuffer : aplusSourceBuffers()) {
        if (sourceBuffer->hasVideoDecoder()) {
            sourceBuffer->paint(context, rect);
            return;
        }
    }
}

void MediaSourcePrivateAplus::terminate()
{
    m_paused = true;
    m_watchdog.stop();
    for (auto& sourceBuffer : aplusSourceBuffers()) {
        sourceBuffer->terminateDecoders();
    }
}

void MediaSourcePrivateAplus::checkEnded()
{
    if (m_ended || m_seeking || !isEnded()) {
        return;
    }
    auto duration = this->duration();
    bool atEnd = duration.isValid() && duration.isFinite() && m_clock->position() >= duration.toDouble() - 0.1;
    if (!atEnd && !areSourceBuffersEnded()) {
        return;
    }
    m_ended = true;
    m_waitReady = false;
    if (duration.isValid() && duration.isFinite()) {
        m_clock->setPosition(duration.toDouble());
    }
    if (RefPtr player = m_player.get()) {
        if (RefPtr mediaPlayer = player->mediaPlayer()) {
            mediaPlayer->timeChanged();
            mediaPlayer->characteristicChanged();
            mediaPlayer->playbackStateChanged();
        }
    }
}

void MediaSourcePrivateAplus::watchdogFired()
{
    if (m_paused) {
        m_watchdog.stop();
        return;
    }
    if (m_waitReady && !m_seeking && areSourceBuffersReadyToPlay()) {
        startPlaying();
    }
    if (RefPtr player = m_player.get()) {
        if (RefPtr mediaPlayer = player->mediaPlayer()) {
            mediaPlayer->timeChanged();
        }
    }
    checkEnded();
}

}

#endif
