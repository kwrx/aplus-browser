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

#include "AcinerellaDecoder.h"
#include "AcinerellaMuxer.h"
#include "AcinerellaPointer.h"
#include "AcinerellaVideoDecoder.h"
#include "FloatSize.h"
#include "MediaPromiseTypes.h"
#include "SourceBufferPrivate.h"
#include <atomic>
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

namespace WebCore {

class GraphicsContext;
class FloatRect;
class MediaSampleAplus;
class MediaSourcePrivateAplus;
class SourceBufferPrivateAplus;

class SourceBufferParserAplus final : public ThreadSafeRefCounted<SourceBufferParserAplus> {
public:
    struct Stream {
        int index { -1 };
        ac_stream_info info { };
        String codec;
    };

    struct Result {
        bool failed { false };
        bool hasInit { false };
        Stream audio;
        Stream video;
        double duration { 0 };
        RefPtr<Acinerella::AcinerellaPointer> pointer;
        Vector<Ref<MediaSampleAplus>> samples;
    };

    static Ref<SourceBufferParserAplus> create(Function<void()>&& drained) { return adoptRef(*new SourceBufferParserAplus(WTF::move(drained))); }
    ~SourceBufferParserAplus();

    void start();
    void append(std::span<const uint8_t>);
    void terminate();
    Result take();

private:
    explicit SourceBufferParserAplus(Function<void()>&&);

    void run();
    void finish();
    int read(uint8_t*, int);
    void notifyDrained() WTF_REQUIRES_LOCK(m_lock);

    static int readCallback(void*, uint8_t*, int);

    Function<void()> m_drained;
    RefPtr<Thread> m_thread;
    Lock m_lock;
    Condition m_condition;
    Vector<uint8_t> m_data WTF_GUARDED_BY_LOCK(m_lock);
    size_t m_readPosition WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    bool m_terminated WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_drainPending WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_finished WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_initTaken WTF_GUARDED_BY_LOCK(m_lock) { false };
    Result m_result WTF_GUARDED_BY_LOCK(m_lock);
};

class MediaSourceClockAplus final : public ThreadSafeRefCounted<MediaSourceClockAplus> {
public:
    static Ref<MediaSourceClockAplus> create() { return adoptRef(*new MediaSourceClockAplus); }

    void setVideoDecoder(RefPtr<Acinerella::AcinerellaVideoDecoder>&&);
    void clearVideoDecoder(Acinerella::AcinerellaDecoder*);
    void setHasAudio(bool hasAudio) { m_hasAudio = hasAudio; }
    bool hasAudio() const { return m_hasAudio; }
    void audioPosition(double);
    void videoPosition(double);
    void setPosition(double position) { m_position = position; }
    double position() const { return m_position; }
    void videoFrameCounts(unsigned& decoded, unsigned& dropped);

private:
    MediaSourceClockAplus() = default;

    Lock m_lock;
    RefPtr<Acinerella::AcinerellaVideoDecoder> m_videoDecoder WTF_GUARDED_BY_LOCK(m_lock);
    std::atomic<bool> m_hasAudio { false };
    std::atomic<double> m_position { 0 };
};

class SourceBufferSampleRequestAplus final : public ThreadSafeRefCounted<SourceBufferSampleRequestAplus> {
public:
    static Ref<SourceBufferSampleRequestAplus> create() { return adoptRef(*new SourceBufferSampleRequestAplus); }

    ThreadSafeWeakPtr<SourceBufferPrivateAplus> owner;
    std::atomic<int> audioIndex { -1 };
    std::atomic<int> videoIndex { -1 };
    std::atomic<bool> wantsAudio { false };
    std::atomic<bool> wantsVideo { false };

private:
    SourceBufferSampleRequestAplus() = default;
};

class SourceBufferPrivateAplus final : public SourceBufferPrivate, public Acinerella::AcinerellaDecoderClient {
public:
    static constexpr TrackID audioTrackID = 1;
    static constexpr TrackID videoTrackID = 2;

    static Ref<SourceBufferPrivateAplus> create(MediaSourcePrivateAplus&);
    ~SourceBufferPrivateAplus();

    constexpr MediaPlatformType platformType() const final { return MediaPlatformType::Aplus; }

    void prePlay();
    void play();
    void pause();
    void willSeek(double);
    void setVolume(float);
    void setAudioPresentationTime(double);
    void paint(GraphicsContext&, const FloatRect&);
    void terminateDecoders();

    bool hasAudioDecoder() const { return !!m_audioDecoder; }
    bool hasVideoDecoder() const { return !!m_videoDecoder; }
    bool areDecodersReadyToPlay() const;
    bool areDecodersEnded() const;
    FloatSize videoSize() const;

    const MediaPlayerAplusStreamSettings& streamSettings() final;
    void onDecoderWarmedUp(RefPtr<Acinerella::AcinerellaDecoder>) final { }
    void onDecoderReadyToPlay(RefPtr<Acinerella::AcinerellaDecoder>) final;
    void onDecoderPlaying(RefPtr<Acinerella::AcinerellaDecoder>, bool) final { }
    void onDecoderEnded(RefPtr<Acinerella::AcinerellaDecoder>) final;
    void onDecoderUpdatedBufferLength(RefPtr<Acinerella::AcinerellaDecoder>, double) final { }
    void onDecoderUpdatedPosition(RefPtr<Acinerella::AcinerellaDecoder>, double) final;
    void onDecoderUpdatedDuration(RefPtr<Acinerella::AcinerellaDecoder>, double) final { }
    void onDecoderWantsToRender(RefPtr<Acinerella::AcinerellaDecoder>) final;
    void onDecoderRenderUpdate(RefPtr<Acinerella::AcinerellaDecoder>) final { }
    void onDecoderNotReadyToRender(RefPtr<Acinerella::AcinerellaDecoder>) final;

private:
    explicit SourceBufferPrivateAplus(MediaSourcePrivateAplus&);

    Ref<MediaPromise> appendInternal(Ref<SharedBuffer>&&) final;
    void resetParserStateInternal() final;
    void removedFromMediaSource() final;
    void flush(TrackID) final;
    void enqueueSample(Ref<MediaSample>&&, TrackID) final;
    void allSamplesInTrackEnqueued(TrackID) final;
    bool isReadyForMoreSamples(TrackID) final;
    void notifyClientWhenReadyForMoreSamples(TrackID) final;

    RefPtr<MediaSourcePrivateAplus> mediaSource() const;
    void startParser(Vector<uint8_t>&&);
    void stopParser();
    void parserDrained(unsigned generation);
    void ensureDecoders(const SourceBufferParserAplus::Result&);
    void updateMuxer();
    void kickDecoders();
    RefPtr<Acinerella::AcinerellaDecoder> decoderForTrack(TrackID) const;
    void settleAppend(bool success);
    void postTask(Function<void(SourceBufferPrivateAplus&)>&&);

    RefPtr<SourceBufferParserAplus> m_parser;
    std::optional<MediaPromise::Producer> m_appendProducer;
    Vector<uint8_t> m_initSegment;
    bool m_deliverInit { false };
    unsigned m_parserGeneration { 0 };

    RefPtr<Acinerella::AcinerellaMuxedBuffer> m_muxer;
    RefPtr<Acinerella::AcinerellaPointer> m_lastAudioPointer;
    RefPtr<Acinerella::AcinerellaPointer> m_lastVideoPointer;
    RefPtr<Acinerella::AcinerellaDecoder> m_audioDecoder;
    RefPtr<Acinerella::AcinerellaDecoder> m_videoDecoder;
    const Ref<MediaSourceClockAplus> m_clock;
    const Ref<SourceBufferSampleRequestAplus> m_request;
    std::atomic<bool> m_audioEnded { false };
    std::atomic<bool> m_videoEnded { false };
    FloatSize m_presentationSize;
    std::optional<double> m_seekTarget;
    std::optional<double> m_lastAudioPts;
    std::optional<double> m_audioDedupUntil;
    uint32_t m_eosTracks { 0 };
    bool m_kickPending { false };
    float m_volume { 1 };
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SourceBufferPrivateAplus)
static bool isType(const WebCore::SourceBufferPrivate& sourceBuffer) { return sourceBuffer.platformType() == WebCore::MediaPlatformType::Aplus; }
SPECIALIZE_TYPE_TRAITS_END()

#endif
