/*
 * Copyright (C) 2014-2016 Haiku, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "MediaPlayerPrivateHaiku.h"

#if ENABLE(VIDEO)

#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "wtf/text/CString.h"
#include "wtf/NeverDestroyed.h"

#include <support/Locker.h>
#include <Bitmap.h>
#include <DataIO.h>
#include <HttpRequest.h>
#include <MediaDefs.h>
#include <MediaFile.h>
#include <MediaTrack.h>
#include <SoundPlayer.h>
#include <View.h>
#include <OS.h>
#include <cmath>


namespace WebCore {

class MediaPlayerFactoryHaiku final : public MediaPlayerFactory {
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::Haiku; };

    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer& player) const final
    {
        return adoptRef(* new MediaPlayerPrivate(player));
    }

    void getSupportedTypes(HashSet<String>& types) const final
    {
        return MediaPlayerPrivate::getSupportedTypes(types);
    }

    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        return MediaPlayerPrivate::supportsType(parameters);
    }

    bool supportsKeySystem(const String& keySystem, const String& mimeType) const final
    {
        return false;
        //return MediaPlayerPrivate::supportsKeySystem(keySystem, mimeType);
    }
};

void MediaPlayerPrivate::registerMediaEngine(MediaEngineRegistrar registrar)
{
    registrar(makeUnique<MediaPlayerFactoryHaiku>());
}

MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer& player)
    : m_didReceiveData(false)
    , m_mediaFile(nullptr)
    , m_audioTrack(nullptr)
    , m_videoTrack(nullptr)
    , m_soundPlayer(nullptr)
    , m_frameBuffer(nullptr)
    , m_player(player)
    , m_networkState(MediaPlayer::NetworkState::Empty)
    , m_readyState(MediaPlayer::ReadyState::HaveNothing)
    , m_volume(1.0)
    , m_currentTime(0.f)
    , m_paused(true)
    , m_abort(false)
    , m_loadingThread(-1)
    , m_videoThread(-1)
{
}


MediaPlayerPrivate::~MediaPlayerPrivate()
{
    m_mediaLock.Lock();

    cancelLoad();
    delete m_frameBuffer;
    m_mediaLock.Unlock();
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivate::load(const URL& url, const LoadOptions&, WebCore::MediaSourcePrivateClient&)
{
    load(url.string());
}
#endif

void MediaPlayerPrivate::load(const String& url)
{
    m_mediaLock.Lock();
    cancelLoad();
    m_abort = false;
    m_url = url;

    m_loadingThread = spawn_thread(loadingThread, "media loader", B_NORMAL_PRIORITY, this);
    if (m_loadingThread >= 0)
        resume_thread(m_loadingThread);

    m_mediaLock.Unlock();
}


void MediaPlayerPrivate::cancelLoad()
{
    m_abort = true;

    // Do not hold the lock while waiting for threads, as they need it to finish
    if (m_loadingThread >= 0) {
        status_t status;
        wait_for_thread(m_loadingThread, &status);
        m_loadingThread = -1;
    }

    if (m_videoThread >= 0) {
        status_t status;
        wait_for_thread(m_videoThread, &status);
        m_videoThread = -1;
    }

    m_mediaLock.Lock();
    if (m_soundPlayer)
        m_soundPlayer->Stop();
    delete m_soundPlayer;
    m_soundPlayer = nullptr;

    delete m_mediaFile;
    m_mediaFile = nullptr;
    m_audioTrack = nullptr;
    m_videoTrack = nullptr;
    m_mediaLock.Unlock();
}


void MediaPlayerPrivate::prepareToPlay()
{
    // TODO should we seek the tracks to 0? reset m_currentTime? other things?
}

void MediaPlayerPrivate::playCallback(void* cookie, void* buffer,
    size_t /*size*/, const media_raw_audio_format& /*format*/)
{
    MediaPlayerPrivate* player = (MediaPlayerPrivate*)cookie;

    if (!player->m_mediaLock.Lock())
        return;

    if (player->m_audioTrack) {
        player->m_currentTime = player->m_audioTrack->CurrentTime() / 1000000.f;

        int64 size64;
        if (player->m_audioTrack->ReadFrames(buffer, &size64) != B_OK)
        {
            player->m_currentTime = player->m_audioTrack->Duration() / 1000000.f;
            player->m_soundPlayer->Stop(false);

            callOnMainThread([p = WeakPtr(player)] {
                if (p)
                    p->m_player.timeChanged();
            });

            player->m_audioTrack = nullptr;
        }
    }
    player->m_mediaLock.Unlock();
}


status_t MediaPlayerPrivate::loadingThread(void* cookie)
{
    MediaPlayerPrivate* player = (MediaPlayerPrivate*)cookie;
    player->IdentifyTracks(player->m_url);

    if (player->m_abort)
        return B_OK;

    callOnMainThread([player = WeakPtr(player)] {
        if (!player)
            return;

        if (player->m_mediaFile && player->m_mediaFile->InitCheck() == B_OK) {
            player->m_player.characteristicChanged();
            player->m_player.durationChanged();
            player->m_player.sizeChanged();
            player->m_player.firstVideoFrameAvailable();

            player->m_readyState = MediaPlayer::ReadyState::HaveEnoughData;
            player->m_networkState = MediaPlayer::NetworkState::Loaded;
        } else {
            player->m_readyState = MediaPlayer::ReadyState::HaveMetadata;
            player->m_networkState = MediaPlayer::NetworkState::FormatError;
        }
        player->m_player.networkStateChanged();
        player->m_player.readyStateChanged();
    });

    return B_OK;
}

status_t MediaPlayerPrivate::videoThread(void* cookie)
{
    MediaPlayerPrivate* player = (MediaPlayerPrivate*)cookie;
    bigtime_t startTime = system_time();
    bigtime_t startMediaTime = 0;
    float lastObservedTime = 0;

    while (!player->m_abort) {
        if (player->m_paused) {
            snooze(10000);
            startTime = system_time();
            startMediaTime = (bigtime_t)(player->m_currentTime * 1000000);
            lastObservedTime = player->m_currentTime;
            continue;
        }

        if (!player->m_mediaLock.Lock())
            continue;

        if (player->m_videoTrack) {
            // Detect seek or external time update
            if (std::abs(player->m_currentTime - lastObservedTime) > 0.5f) {
                startTime = system_time();
                startMediaTime = (bigtime_t)(player->m_currentTime * 1000000);
            }

            bigtime_t videoTime = player->m_videoTrack->CurrentTime();
            bigtime_t audioTime;

            if (player->m_audioTrack) {
                audioTime = player->m_audioTrack->CurrentTime();
            } else {
                audioTime = startMediaTime + (system_time() - startTime);
            }

            player->m_currentTime = audioTime / 1000000.f;
            lastObservedTime = player->m_currentTime;

            if (videoTime < audioTime) {
                int64 count;
                if (player->m_videoTrack->ReadFrames(player->m_frameBuffer->Bits(), &count) == B_OK) {
                    callOnMainThread([player = WeakPtr(player)] {
                        if (player)
                            player->m_player.repaint();
                    });
                } else {
                    player->m_videoTrack = nullptr;
                }
            } else {
                snooze(5000);
            }
        }
        player->m_mediaLock.Unlock();
        snooze(1000);
    }

    return B_OK;
}




void MediaPlayerPrivate::play()
{
    if (m_soundPlayer)
        m_soundPlayer->Start();
    m_paused = false;
}

void MediaPlayerPrivate::pause()
{
    if (m_soundPlayer)
        m_soundPlayer->Stop(false);
    m_paused = true;
}

FloatSize MediaPlayerPrivate::naturalSize() const
{
    if (!m_frameBuffer)
        return FloatSize(0,0);

    BRect r(m_frameBuffer->Bounds());
    return FloatSize(r.Width() + 1, r.Height() + 1);
}

bool MediaPlayerPrivate::hasAudio() const
{
    return m_audioTrack;
}

bool MediaPlayerPrivate::hasVideo() const
{
    return m_videoTrack;
}

void MediaPlayerPrivate::setPageIsVisible(bool)
{
    notImplemented();
}

WTF::MediaTime MediaPlayerPrivate::duration() const
{
    // TODO handle the case where there is a video, but no audio track.
    if (!m_audioTrack)
        return WTF::MediaTime();
    return WTF::MediaTime::createWithDouble(m_audioTrack->Duration() / 1000000.f);
}

WTF::MediaTime MediaPlayerPrivate::currentTime() const
{
    return WTF::MediaTime::createWithDouble(m_currentTime);
}

Ref<MediaTimePromise> MediaPlayerPrivate::seekToTarget(const SeekTarget& time)
{
    m_mediaLock.Lock();
    bigtime_t newTime = (bigtime_t)(time.time.toDouble() * 1000000);
    if (m_videoTrack)
        m_videoTrack->SeekToTime(&newTime);
    if (m_audioTrack)
        m_audioTrack->SeekToTime(&newTime);

    m_currentTime = newTime / 1000000.f;

    if (m_paused && m_videoTrack && m_frameBuffer) {
        int64 count;
        if (m_videoTrack->ReadFrames(m_frameBuffer->Bits(), &count) == B_OK) {
            callOnMainThread([p = WeakPtr(this)] {
                if (p)
                    p->m_player.repaint();
            });
        }
    }
    m_player.timeChanged();
    m_mediaLock.Unlock();
}


bool MediaPlayerPrivate::paused() const
{
    return m_paused;
}

void MediaPlayerPrivate::setVolume(float volume)
{
    m_volume = volume;
    if (m_soundPlayer)
        m_soundPlayer->SetVolume(volume);
}

MediaPlayer::NetworkState MediaPlayerPrivate::networkState() const
{
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivate::readyState() const
{
    return m_readyState;
}

PlatformTimeRanges& MediaPlayerPrivate::buffered() const
{
    notImplemented();
    static PlatformTimeRanges ranges;
    return ranges;
}

bool MediaPlayerPrivate::didLoadingProgress() const
{
    bool progress = m_didReceiveData;
    m_didReceiveData = false;
    return progress;
}

void MediaPlayerPrivate::paint(GraphicsContext& context, const FloatRect& r)
{
    if (context.paintingDisabled())
        return;

    if (m_frameBuffer) {
        BView* target = context.platformContext();
        target->SetDrawingMode(B_OP_COPY);
        target->DrawBitmap(m_frameBuffer, r);
    }
}

// #pragma mark - private methods

void MediaPlayerPrivate::IdentifyTracks(const String& url)
{
    // TODO something here is blocking inside the Media Kit. We should rework
    // things so that this is run in a separate thread (Media Thread?). This
    // would avoid the annoying freeze whenever a media is being loaded.
#if B_HAIKU_VERSION <= B_HAIKU_VERSION_1_BETA_5
    m_mediaFile = new BMediaFile(BUrl(url.utf8().data()));
#else
    m_mediaFile = new BMediaFile(BUrl(url.utf8().data(), false));
#endif

    if (m_mediaFile->InitCheck() == B_OK) {
        for (int i = m_mediaFile->CountTracks() - 1; i >= 0; i--)
        {
            BMediaTrack* track = m_mediaFile->TrackAt(i);

            media_format format;
            track->DecodedFormat(&format);

            if (format.IsVideo()) {
                if (m_videoTrack)
                    continue;
                m_videoTrack = track;

                m_frameBuffer = new BBitmap(
                    BRect(0, 0, format.Width() - 1, format.Height() - 1),
                    B_RGB32);

                if (m_audioTrack)
                    break;
            }

            if (format.IsAudio()) {
                if (m_audioTrack)
                    continue;
                m_audioTrack = track;

                m_soundPlayer = new BSoundPlayer(&format.u.raw_audio,
                    "HTML5 Audio", playCallback, NULL, this);
                m_soundPlayer->SetVolume(m_volume);
                if (!m_paused)
                    m_soundPlayer->Start();

                if (m_videoTrack && m_audioTrack)
                    break;
            }

        }

        if (m_videoTrack) {
            m_videoThread = spawn_thread(videoThread, "video decoder", B_NORMAL_PRIORITY, this);
            if (m_videoThread >= 0)
                resume_thread(m_videoThread);
        }
    }
}



// #pragma mark - static methods

static HashSet<String> mimeTypeCache()
{
    static NeverDestroyed<HashSet<String>> cache;
    static bool typeListInitialized = false;

    if (typeListInitialized)
        return cache;

    int32 cookie = 0;
    media_file_format mfi;

    // Add the types the Haiku Media Kit add-ons advertise support for
    while(get_next_file_format(&cookie, &mfi) == B_OK) {
        cache.get().add(String::fromUTF8(mfi.mime_type));
    }

    typeListInitialized = true;
    return cache;
}

void MediaPlayerPrivate::getSupportedTypes(HashSet<String>& types)
{
    types = mimeTypeCache();
}

MediaPlayer::SupportsType MediaPlayerPrivate::supportsType(const MediaEngineSupportParameters& parameters)
{
    if (parameters.type.isEmpty())
        return MediaPlayer::SupportsType::IsNotSupported;

    // spec says we should not return "probably" if the codecs string is empty
    if (mimeTypeCache().contains(parameters.type.containerType())) {
        return parameters.type.codecs().isEmpty() ? MediaPlayer::SupportsType::MayBeSupported : MediaPlayer::SupportsType::IsSupported;
    }

    return MediaPlayer::SupportsType::IsNotSupported;
}

DestinationColorSpace MediaPlayerPrivate::colorSpace()
{
    return DestinationColorSpace::SRGB();
}

}

#endif
