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
#include "SourceBufferPrivateAplus.h"

#if ENABLE(MEDIA_SOURCE)

#include "AcinerellaAudioDecoder.h"
#include "AcinerellaVideoDecoder.h"
#include "AudioTrackPrivateAplus.h"
#include "MediaDescriptionAplus.h"
#include "MediaPlayerAplus.h"
#include "MediaSampleAplus.h"
#include "MediaSourcePrivateAplus.h"
#include "SharedBuffer.h"
#include "VideoTrackPrivateAplus.h"
#include <limits>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

#include <wtf/MonotonicTime.h>

namespace WebCore {

using namespace Acinerella;

static constexpr int sampleRequestThreshold = 64;
static constexpr int maximumQueuedPackages = 192;

static String codecString(const char* name)
{
    static constexpr std::pair<const char*, ASCIILiteral> codecs[] = {
        { "h264", "avc1"_s },
        { "hevc", "hev1"_s },
        { "vp8", "vp08"_s },
        { "vp9", "vp09"_s },
        { "av1", "av01"_s },
        { "aac", "mp4a"_s },
        { "opus", "opus"_s },
        { "vorbis", "vorb"_s },
        { "flac", "fLaC"_s },
        { "mp3", ".mp3"_s },
    };

    if (!name) {
        return emptyString();
    }
    for (auto& [ffmpegName, codec] : codecs) {
        if (!strcmp(name, ffmpegName)) {
            return codec;
        }
    }
    return String::fromLatin1(name);
}

static uint32_t readBE32(std::span<const uint8_t> data, size_t offset)
{
    return (uint32_t(data[offset]) << 24) | (uint32_t(data[offset + 1]) << 16) | (uint32_t(data[offset + 2]) << 8) | uint32_t(data[offset + 3]);
}

static bool readEBMLVint(std::span<const uint8_t> data, size_t& offset, uint64_t& value, bool keepMarker, bool& unknown)
{
    if (offset >= data.size()) {
        return false;
    }
    uint8_t first = data[offset];
    int length = 1;
    while (length <= 8 && !(first & (0x80 >> (length - 1)))) {
        length++;
    }
    if (length > 8 || offset + length > data.size()) {
        return false;
    }
    uint64_t result = keepMarker ? first : (first & (0xff >> length));
    bool allOnes = (first & (0xff >> length)) == (0xff >> length);
    for (int i = 1; i < length; i++) {
        result = (result << 8) | data[offset + i];
        allOnes = allOnes && data[offset + i] == 0xff;
    }
    offset += length;
    value = result;
    unknown = !keepMarker && allOnes;
    return true;
}

static std::optional<size_t> webmInitializationSegmentLength(std::span<const uint8_t> data)
{
    size_t offset = 0;
    uint64_t id = 0;
    uint64_t size = 0;
    bool unknown = false;

    if (!readEBMLVint(data, offset, id, true, unknown) || !readEBMLVint(data, offset, size, false, unknown) || unknown) {
        return data.size();
    }
    offset += size;
    if (!readEBMLVint(data, offset, id, true, unknown) || id != 0x18538067 || !readEBMLVint(data, offset, size, false, unknown)) {
        return data.size();
    }
    while (offset < data.size()) {
        size_t elementStart = offset;
        if (!readEBMLVint(data, offset, id, true, unknown)) {
            return data.size();
        }
        if (id == 0x1F43B675) {
            return elementStart;
        }
        if (!readEBMLVint(data, offset, size, false, unknown) || unknown) {
            return data.size();
        }
        if (size > data.size() - offset) {
            return data.size();
        }
        offset += size;
    }
    return data.size();
}

static std::optional<size_t> mp4InitializationSegmentLength(std::span<const uint8_t> data)
{
    size_t offset = 0;
    while (offset + 8 <= data.size()) {
        uint64_t size = readBE32(data, offset);
        auto type = data.subspan(offset + 4, 4);
        auto isType = [&](const char* name) {
            return !memcmp(type.data(), name, 4);
        };
        if (isType("moof") || isType("styp") || isType("sidx") || isType("emsg") || isType("mdat") || isType("prft")) {
            return offset;
        }
        if (size == 1) {
            if (offset + 16 > data.size()) {
                return data.size();
            }
            size = (uint64_t(readBE32(data, offset + 8)) << 32) | readBE32(data, offset + 12);
        } else if (!size) {
            return data.size();
        }
        if (size < 8 || size > data.size() - offset) {
            return data.size();
        }
        offset += size;
    }
    return data.size();
}

static std::optional<size_t> initializationSegmentLength(std::span<const uint8_t> data)
{
    if (data.size() >= 4 && data[0] == 0x1A && data[1] == 0x45 && data[2] == 0xDF && data[3] == 0xA3) {
        return webmInitializationSegmentLength(data);
    }
    if (data.size() >= 8 && (!memcmp(data.data() + 4, "ftyp", 4) || !memcmp(data.data() + 4, "moov", 4))) {
        return mp4InitializationSegmentLength(data);
    }
    return std::nullopt;
}

SourceBufferParserAplus::SourceBufferParserAplus(Function<void()>&& drained)
    : m_drained(WTF::move(drained))
{
}

SourceBufferParserAplus::~SourceBufferParserAplus() = default;

void SourceBufferParserAplus::start()
{
    m_thread = Thread::create("Aplus MSE Parser"_s, [protectedThis = Ref(*this)] {
        protectedThis->run();
    });
}

void SourceBufferParserAplus::append(std::span<const uint8_t> data)
{
    Locker locker { m_lock };
    m_data.append(data);
    m_drainPending = true;
    if (m_finished) {
        notifyDrained();
        return;
    }
    m_condition.notifyAll();
}

void SourceBufferParserAplus::terminate()
{
    {
        Locker locker { m_lock };
        m_terminated = true;
        m_condition.notifyAll();
    }
    if (RefPtr thread = std::exchange(m_thread, nullptr)) {
        thread->waitForCompletion();
    }
}

SourceBufferParserAplus::Result SourceBufferParserAplus::take()
{
    Locker locker { m_lock };
    Result result;
    result.failed = m_result.failed;
    result.samples = std::exchange(m_result.samples, { });
    if (m_result.hasInit && !m_initTaken) {
        m_initTaken = true;
        result.hasInit = true;
        result.audio = m_result.audio;
        result.video = m_result.video;
        result.duration = m_result.duration;
        result.pointer = m_result.pointer;
    }
    return result;
}

void SourceBufferParserAplus::notifyDrained()
{
    m_drainPending = false;
    callOnMainThread([protectedThis = Ref(*this)] {
        {
            Locker locker { protectedThis->m_lock };
            if (protectedThis->m_terminated) {
                return;
            }
        }
        if (protectedThis->m_drained) {
            protectedThis->m_drained();
        }
    });
}

int SourceBufferParserAplus::readCallback(void* sender, uint8_t* buffer, int size)
{
    return static_cast<SourceBufferParserAplus*>(sender)->read(buffer, size);
}

int SourceBufferParserAplus::read(uint8_t* buffer, int size)
{
    Locker locker { m_lock };
    while (!m_terminated && m_readPosition >= m_data.size()) {
        if (m_drainPending) {
            notifyDrained();
        }
        m_condition.wait(m_lock);
    }
    if (m_terminated || size <= 0) {
        return 0;
    }
    size_t count = std::min<size_t>(size, m_data.size() - m_readPosition);
    memcpy(buffer, m_data.span().subspan(m_readPosition, count).data(), count);
    m_readPosition += count;
    if (m_readPosition >= m_data.size()) {
        m_data.clear();
        m_readPosition = 0;
    }
    return static_cast<int>(count);
}

void SourceBufferParserAplus::finish()
{
    Locker locker { m_lock };
    m_finished = true;
    if (!m_terminated) {
        m_result.failed = true;
        notifyDrained();
    }
}

void SourceBufferParserAplus::run()
{
    RefPtr<AcinerellaPointer> pointer = AcinerellaPointer::create();
    if (!pointer || !pointer->instance() || ac_open(pointer->instance(), this, nullptr, readCallback, nullptr, nullptr, nullptr) < 0) {
        finish();
        return;
    }

    auto* instance = pointer->instance();
    Stream audio;
    Stream video;
    for (int i = 0; i < std::min(instance->stream_count, AcinerellaMuxedBuffer::maxDecoders); i++) {
        ac_stream_info info;
        ac_get_stream_info(instance, i, &info);
        Stream* stream = nullptr;
        if (info.stream_type == AC_STREAM_TYPE_AUDIO && audio.index < 0) {
            stream = &audio;
        } else if (info.stream_type == AC_STREAM_TYPE_VIDEO && video.index < 0) {
            stream = &video;
        }
        if (!stream) {
            continue;
        }
        pointer->setDecoder(i, ac_create_decoder(instance, i));
        if (!pointer->decoder(i)) {
            continue;
        }
        ac_decoder_fake_seek(pointer->decoder(i));
        stream->index = i;
        stream->info = info;
        stream->codec = codecString(ac_codec_name(instance, i));
    }

    if (audio.index < 0 && video.index < 0) {
        finish();
        return;
    }

    {
        Locker locker { m_lock };
        m_result.hasInit = true;
        m_result.audio = audio;
        m_result.video = video;
        m_result.duration = instance->info.duration > 0 ? instance->info.duration / 1000.0 : 0;
        m_result.pointer = pointer;
    }

    FloatSize presentationSize;
    if (video.index >= 0) {
        presentationSize = FloatSize(std::max(1, video.info.additional_info.video_info.frame_width), std::max(1, video.info.additional_info.video_info.frame_height));
    }

    MediaTime lastDecodeTime[2] = { MediaTime::invalidTime(), MediaTime::invalidTime() };
    MediaTime lastDuration[2] = { MediaTime(1, 50), MediaTime(1, 30) };

    while (auto* package = ac_read_package(instance)) {
        bool isAudio = package->stream_index == audio.index;
        bool isVideo = package->stream_index == video.index;
        if (!isAudio && !isVideo) {
            ac_free_package(package);
            continue;
        }

        int64_t pts = 0;
        int64_t dts = 0;
        int64_t duration = 0;
        int num = 1;
        int den = 1;
        ac_get_package_timing(instance, package, &pts, &dts, &duration, &num, &den);
        if (den <= 0 || num <= 0) {
            num = 1;
            den = 1;
        }

        int slot = isAudio ? 0 : 1;
        MediaTime presentationTime(pts * num, den);
        MediaTime decodeTime(dts * num, den);
        MediaTime sampleDuration = MediaTime(duration * num, den);
        if (duration <= 0) {
            if (lastDecodeTime[slot].isValid() && decodeTime > lastDecodeTime[slot]) {
                sampleDuration = decodeTime - lastDecodeTime[slot];
            } else {
                sampleDuration = lastDuration[slot];
            }
        }
        lastDecodeTime[slot] = decodeTime;
        lastDuration[slot] = sampleDuration;

        bool isSync = isAudio || ac_get_package_keyframe(package);
        auto sample = MediaSampleAplus::create(AcinerellaPackage::create(pointer, package), isAudio ? SourceBufferPrivateAplus::audioTrackID : SourceBufferPrivateAplus::videoTrackID, presentationTime, decodeTime, sampleDuration, isSync, isVideo ? presentationSize : FloatSize());

        Locker locker { m_lock };
        if (m_terminated) {
            break;
        }
        m_result.samples.append(WTF::move(sample));
    }

    finish();
}

void MediaSourceClockAplus::setVideoDecoder(RefPtr<AcinerellaVideoDecoder>&& decoder)
{
    Locker locker { m_lock };
    m_videoDecoder = WTF::move(decoder);
}

void MediaSourceClockAplus::clearVideoDecoder(AcinerellaDecoder* decoder)
{
    RefPtr<AcinerellaVideoDecoder> released;
    {
        Locker locker { m_lock };
        if (m_videoDecoder.get() == decoder) {
            released = std::exchange(m_videoDecoder, nullptr);
        }
    }
}

void MediaSourceClockAplus::audioPosition(double position)
{
    m_position = position;
    Locker locker { m_lock };
    if (m_videoDecoder) {
        m_videoDecoder->setAudioPresentationTime(position);
    }
}

void MediaSourceClockAplus::videoFrameCounts(unsigned& decoded, unsigned& dropped)
{
    Locker locker { m_lock };
    if (m_videoDecoder) {
        decoded = m_videoDecoder->decodedFrameCount();
        dropped = m_videoDecoder->droppedFrameCount();
    }
}

void MediaSourceClockAplus::videoPosition(double position)
{
    if (!m_hasAudio) {
        m_position = position;
    }
}

static const MediaPlayerAplusStreamSettings& mediaSourceStreamSettings()
{
    static NeverDestroyed<MediaPlayerAplusStreamSettings> settings;
    return settings.get();
}

Ref<SourceBufferPrivateAplus> SourceBufferPrivateAplus::create(MediaSourcePrivateAplus& parent)
{
    Ref sourceBuffer = adoptRef(*new SourceBufferPrivateAplus(parent));
    sourceBuffer->m_request->owner = sourceBuffer.get();
    return sourceBuffer;
}

SourceBufferPrivateAplus::SourceBufferPrivateAplus(MediaSourcePrivateAplus& parent)
    : SourceBufferPrivate(parent)
    , m_muxer(AcinerellaMuxedBuffer::create())
    , m_clock(parent.clock())
    , m_request(SourceBufferSampleRequestAplus::create())
    , m_volume(parent.volume())
{
    m_muxer->setSinkFunction([request = m_request](int index, int packagesLeft, uint32_t) -> bool {
        if (packagesLeft >= sampleRequestThreshold) {
            return false;
        }
        TrackID trackID = 0;
        if (index == request->audioIndex && request->wantsAudio.exchange(false)) {
            trackID = audioTrackID;
        } else if (index == request->videoIndex && request->wantsVideo.exchange(false)) {
            trackID = videoTrackID;
        }
        if (trackID) {
            callOnMainThread([request, trackID] {
                if (RefPtr sourceBuffer = request->owner.get()) {
                    sourceBuffer->provideMediaData(trackID);
                }
            });
        }
        return false;
    });
}

SourceBufferPrivateAplus::~SourceBufferPrivateAplus()
{
    terminateDecoders();
}

RefPtr<MediaSourcePrivateAplus> SourceBufferPrivateAplus::mediaSource() const
{
    RefPtr mediaSource = m_mediaSource.get();
    return static_cast<MediaSourcePrivateAplus*>(mediaSource.get());
}

void SourceBufferPrivateAplus::postTask(Function<void(SourceBufferPrivateAplus&)>&& task)
{
    callOnMainThread([request = m_request, task = WTF::move(task)]() mutable {
        if (RefPtr sourceBuffer = request->owner.get()) {
            task(*sourceBuffer);
        }
    });
}

Ref<MediaPromise> SourceBufferPrivateAplus::appendInternal(Ref<SharedBuffer>&& buffer)
{
    auto data = buffer->span();
    auto media = data;
    bool restart = false;

    if (auto length = initializationSegmentLength(data)) {
        auto initialization = data.first(*length);
        media = data.subspan(*length);
        bool sameInitialization = initialization.size() == m_initSegment.size() && !memcmp(initialization.data(), m_initSegment.span().data(), initialization.size());
        if (!m_parser || !sameInitialization) {
            if (!sameInitialization) {
                m_initSegment = Vector<uint8_t>(initialization);
                m_deliverInit = true;
            }
            restart = true;
        }
    } else if (!m_parser) {
        if (m_initSegment.isEmpty()) {
            return MediaPromise::createAndReject(PlatformMediaError::ParsingError);
        }
        restart = true;
    }

    if (restart) {
        Vector<uint8_t> initial;
        initial.append(m_initSegment.span());
        initial.append(media);
        startParser(WTF::move(initial));
    } else if (media.empty()) {
        return MediaPromise::createAndResolve();
    } else {
        m_parser->append(media);
    }

    settleAppend(true);
    m_appendProducer.emplace();
    return m_appendProducer->promise();
}

void SourceBufferPrivateAplus::startParser(Vector<uint8_t>&& data)
{
    stopParser();
    unsigned generation = ++m_parserGeneration;
    m_parser = SourceBufferParserAplus::create([weakThis = ThreadSafeWeakPtr { *this }, generation] {
        if (RefPtr protectedThis = weakThis.get()) {
            protectedThis->parserDrained(generation);
        }
    });
    m_parser->append(data.span());
    m_parser->start();
}

void SourceBufferPrivateAplus::stopParser()
{
    ++m_parserGeneration;
    if (RefPtr parser = std::exchange(m_parser, nullptr)) {
        parser->terminate();
    }
    settleAppend(true);
}

void SourceBufferPrivateAplus::settleAppend(bool success)
{
    auto producer = std::exchange(m_appendProducer, std::nullopt);
    if (!producer) {
        return;
    }
    if (success) {
        producer->resolve();
    } else {
        producer->reject(PlatformMediaError::ParsingError);
    }
}

void SourceBufferPrivateAplus::parserDrained(unsigned generation)
{
    if (generation != m_parserGeneration || !m_parser) {
        return;
    }

    auto result = m_parser->take();

    if (result.hasInit) {
        ensureDecoders(result);
        if (std::exchange(m_deliverInit, false)) {
            InitializationSegment segment;
            segment.duration = result.duration > 0 ? MediaTime::createWithDouble(result.duration) : MediaTime::invalidTime();
            if (result.audio.index >= 0) {
                segment.audioTracks.append({ MediaDescriptionAplus::create(MediaDescriptionAplus::Kind::Audio, String(result.audio.codec)), AudioTrackPrivateAplus::create(audioTrackID) });
            }
            if (result.video.index >= 0) {
                segment.videoTracks.append({ MediaDescriptionAplus::create(MediaDescriptionAplus::Kind::Video, String(result.video.codec)), VideoTrackPrivateAplus::create(videoTrackID) });
            }
            didReceiveInitializationSegment(WTF::move(segment));
        }
    }

    for (auto& sample : result.samples) {
        didReceiveSample(WTF::move(sample));
    }

    if (result.failed) {
        auto producer = std::exchange(m_appendProducer, std::nullopt);
        stopParser();
        if (producer) {
            producer->reject(PlatformMediaError::ParsingError);
        }
        return;
    }

    settleAppend(true);
}

void SourceBufferPrivateAplus::ensureDecoders(const SourceBufferParserAplus::Result& result)
{
    RefPtr<AcinerellaPointer> pointer = result.pointer;
    bool changed = false;

    if (result.audio.index >= 0 && (!m_audioDecoder || m_audioDecoder->index() != result.audio.index)) {
        if (RefPtr old = std::exchange(m_audioDecoder, nullptr)) {
            old->terminate();
        }
        auto decoder = AcinerellaAudioDecoder::create(this, pointer, m_muxer, result.audio.index, result.audio.info, false, false);
        decoder->setDuration(std::numeric_limits<double>::infinity());
        m_audioDecoder = decoder;
        m_request->audioIndex = result.audio.index;
        m_audioEnded = false;
        changed = true;
    }

    if (result.video.index >= 0 && (!m_videoDecoder || m_videoDecoder->index() != result.video.index)) {
        if (RefPtr old = std::exchange(m_videoDecoder, nullptr)) {
            m_clock->clearVideoDecoder(old.get());
            old->terminate();
        }
        auto decoder = AcinerellaVideoDecoder::create(this, pointer, m_muxer, result.video.index, result.video.info, false, false);
        decoder->setDuration(std::numeric_limits<double>::infinity());
        decoder->setCanDropKeyFrames(true);
        m_videoDecoder = decoder;
        m_request->videoIndex = result.video.index;
        m_videoEnded = false;
        m_clock->setVideoDecoder(WTF::move(decoder));
        changed = true;
    }

    if (result.video.index >= 0) {
        auto& info = result.video.info.additional_info.video_info;
        m_presentationSize = FloatSize(info.frame_width, info.frame_height);
    }

    if (!changed) {
        return;
    }

    updateMuxer();

    if (m_audioDecoder) {
        m_audioDecoder->warmUp();
        m_audioDecoder->setVolume(m_volume);
    }
    if (m_videoDecoder) {
        m_videoDecoder->warmUp();
    }

    if (RefPtr mediaSource = this->mediaSource()) {
        mediaSource->sourceBufferDecodersChanged();
    }
}

void SourceBufferPrivateAplus::updateMuxer()
{
    uint32_t mask = 0;
    uint32_t audioMask = 0;
    if (m_audioDecoder) {
        mask |= 1u << m_audioDecoder->index();
        audioMask |= 1u << m_audioDecoder->index();
    }
    if (m_videoDecoder) {
        mask |= 1u << m_videoDecoder->index();
    }
    m_muxer->setDecoderMask(mask, audioMask);
}

RefPtr<AcinerellaDecoder> SourceBufferPrivateAplus::decoderForTrack(TrackID trackID) const
{
    if (trackID == audioTrackID) {
        return m_audioDecoder;
    }
    if (trackID == videoTrackID) {
        return m_videoDecoder;
    }
    return nullptr;
}

void SourceBufferPrivateAplus::resetParserStateInternal()
{
    stopParser();
}

void SourceBufferPrivateAplus::removedFromMediaSource()
{
    terminateDecoders();
    SourceBufferPrivate::removedFromMediaSource();
}

void SourceBufferPrivateAplus::terminateDecoders()
{
    stopParser();
    if (RefPtr decoder = std::exchange(m_audioDecoder, nullptr)) {
        decoder->terminate();
    }
    if (RefPtr decoder = std::exchange(m_videoDecoder, nullptr)) {
        m_clock->clearVideoDecoder(decoder.get());
        decoder->terminate();
    }
    m_request->audioIndex = -1;
    m_request->videoIndex = -1;
    m_lastAudioPts = std::nullopt;
    m_audioDedupUntil = std::nullopt;
    m_muxer->terminate();
}

void SourceBufferPrivateAplus::flush(TrackID trackID)
{
    RefPtr decoder = decoderForTrack(trackID);
    if (!decoder) {
        return;
    }

    if (!m_seekTarget) {
        postTask([](SourceBufferPrivateAplus& sourceBuffer) {
            sourceBuffer.reenqueueMediaIfNeeded(sourceBuffer.currentTime());
        });
        if (trackID == audioTrackID && m_lastAudioPts) {
            m_audioDedupUntil = m_lastAudioPts;
            return;
        }
    }

    m_audioDedupUntil = std::nullopt;
    if (trackID == audioTrackID) {
        m_lastAudioPts = std::nullopt;
    }
    m_muxer->flush(decoder->index());
    auto& pointer = trackID == audioTrackID ? m_lastAudioPointer : m_lastVideoPointer;
    if (pointer) {
        RefPtr<AcinerellaPackage> package = AcinerellaPackage::create(pointer, ac_flush_packet());
        m_muxer->push(package, decoder->index());
    }
    decoder->skipUntilPTS(m_seekTarget.value_or(currentTime().toDouble()));

    m_eosTracks &= ~(1u << trackID);
    if (trackID == audioTrackID) {
        m_audioEnded = false;
    } else {
        m_videoEnded = false;
    }
}

void SourceBufferPrivateAplus::enqueueSample(Ref<MediaSample>&& sample, TrackID trackID)
{
    RefPtr decoder = decoderForTrack(trackID);
    if (!decoder) {
        return;
    }

    if (trackID == audioTrackID) {
        double pts = sample->presentationTime().toDouble();
        if (m_audioDedupUntil) {
            if (pts <= *m_audioDedupUntil + 0.0001) {
                return;
            }
            m_audioDedupUntil = std::nullopt;
        }
        m_lastAudioPts = pts;
    }
    auto& aplusSample = static_cast<MediaSampleAplus&>(sample.get());
    RefPtr<AcinerellaPackage> package = &aplusSample.package();
    if (trackID == audioTrackID) {
        m_lastAudioPointer = package->acinerella();
    } else {
        m_lastVideoPointer = package->acinerella();
    }
    m_muxer->push(package, decoder->index());
    kickDecoders();
}

void SourceBufferPrivateAplus::allSamplesInTrackEnqueued(TrackID trackID)
{
    m_eosTracks |= 1u << trackID;
    uint32_t needed = (m_audioDecoder ? 1u << audioTrackID : 0) | (m_videoDecoder ? 1u << videoTrackID : 0);
    if (needed && (m_eosTracks & needed) == needed) {
        RefPtr<AcinerellaPackage> endOfStream;
        m_muxer->push(endOfStream);
        kickDecoders();
    }
}

bool SourceBufferPrivateAplus::isReadyForMoreSamples(TrackID trackID)
{
    RefPtr decoder = decoderForTrack(trackID);
    return decoder && m_muxer->packagesForDecoder(decoder->index()) < maximumQueuedPackages;
}

void SourceBufferPrivateAplus::notifyClientWhenReadyForMoreSamples(TrackID trackID)
{
    RefPtr decoder = decoderForTrack(trackID);
    if (!decoder) {
        return;
    }
    if (m_muxer->packagesForDecoder(decoder->index()) < sampleRequestThreshold) {
        callOnMainThread([request = m_request, trackID] {
            if (RefPtr sourceBuffer = request->owner.get()) {
                sourceBuffer->provideMediaData(trackID);
            }
        });
        return;
    }
    if (trackID == audioTrackID) {
        m_request->wantsAudio = true;
    } else {
        m_request->wantsVideo = true;
    }
}

void SourceBufferPrivateAplus::kickDecoders()
{
    if (m_kickPending) {
        return;
    }
    m_kickPending = true;
    callOnMainThread([request = m_request] {
        RefPtr sourceBuffer = request->owner.get();
        if (!sourceBuffer) {
            return;
        }
        sourceBuffer->m_kickPending = false;
        RefPtr mediaSource = sourceBuffer->mediaSource();
        bool readying = mediaSource && mediaSource->isWaitingForReady();
        for (auto& decoder : { sourceBuffer->m_audioDecoder, sourceBuffer->m_videoDecoder }) {
            if (!decoder) {
                continue;
            }
            if (readying) {
                decoder->prePlay();
            } else {
                decoder->warmUp();
            }
        }
    });
}

void SourceBufferPrivateAplus::prePlay()
{
    if (m_audioDecoder) {
        m_audioDecoder->prePlay();
    }
    if (m_videoDecoder) {
        m_videoDecoder->prePlay();
    }
}

void SourceBufferPrivateAplus::play()
{
    m_seekTarget = std::nullopt;
    if (m_audioDecoder) {
        m_audioDecoder->play();
    }
    if (m_videoDecoder) {
        m_videoDecoder->play();
    }
}

void SourceBufferPrivateAplus::pause()
{
    if (m_audioDecoder) {
        m_audioDecoder->pause();
    }
    if (m_videoDecoder) {
        m_videoDecoder->pause();
    }
}

void SourceBufferPrivateAplus::willSeek(double target)
{
    m_seekTarget = target;
    m_audioEnded = false;
    m_videoEnded = false;
    if (m_audioDecoder) {
        m_audioDecoder->pause(true);
    }
    if (m_videoDecoder) {
        m_videoDecoder->pause(true);
    }
}

void SourceBufferPrivateAplus::setVolume(float volume)
{
    m_volume = volume;
    if (m_audioDecoder) {
        m_audioDecoder->setVolume(volume);
    }
}

void SourceBufferPrivateAplus::setAudioPresentationTime(double position)
{
    if (m_videoDecoder) {
        static_cast<AcinerellaVideoDecoder*>(m_videoDecoder.get())->setAudioPresentationTime(position);
    }
}

void SourceBufferPrivateAplus::paint(GraphicsContext& context, const FloatRect& rect)
{
    if (m_videoDecoder) {
        m_videoDecoder->paint(context, rect);
    }
}

bool SourceBufferPrivateAplus::areDecodersReadyToPlay() const
{
    if (!m_audioDecoder && !m_videoDecoder) {
        return false;
    }
    return (!m_audioDecoder || m_audioDecoder->isReadyToPlay()) && (!m_videoDecoder || m_videoDecoder->isReadyToPlay());
}

bool SourceBufferPrivateAplus::areDecodersEnded() const
{
    if (m_audioDecoder) {
        return m_audioEnded;
    }
    return m_videoEnded;
}

FloatSize SourceBufferPrivateAplus::videoSize() const
{
    if (m_videoDecoder) {
        auto* video = static_cast<AcinerellaVideoDecoder*>(m_videoDecoder.get());
        if (video->frameWidth() > 0 && video->frameHeight() > 0) {
            return FloatSize(video->frameWidth(), video->frameHeight());
        }
    }
    return m_presentationSize;
}

const MediaPlayerAplusStreamSettings& SourceBufferPrivateAplus::streamSettings()
{
    return mediaSourceStreamSettings();
}

void SourceBufferPrivateAplus::onDecoderReadyToPlay(RefPtr<AcinerellaDecoder>)
{
    postTask([](SourceBufferPrivateAplus& sourceBuffer) {
        if (RefPtr mediaSource = sourceBuffer.mediaSource()) {
            mediaSource->sourceBufferReadyToPlay();
        }
    });
}

void SourceBufferPrivateAplus::onDecoderEnded(RefPtr<AcinerellaDecoder> decoder)
{
    if (decoder->isAudio()) {
        m_audioEnded = true;
    } else {
        m_videoEnded = true;
    }
    postTask([](SourceBufferPrivateAplus& sourceBuffer) {
        if (RefPtr mediaSource = sourceBuffer.mediaSource()) {
            mediaSource->sourceBufferEnded();
        }
    });
}

void SourceBufferPrivateAplus::onDecoderUpdatedPosition(RefPtr<AcinerellaDecoder> decoder, double position)
{
    if (decoder->isAudio()) {
        m_clock->audioPosition(position);
    } else {
        m_clock->videoPosition(position);
    }
}

void SourceBufferPrivateAplus::onDecoderWantsToRender(RefPtr<AcinerellaDecoder>)
{
    postTask([](SourceBufferPrivateAplus& sourceBuffer) {
        if (RefPtr mediaSource = sourceBuffer.mediaSource()) {
            mediaSource->videoFrameAvailable();
        }
    });
}

void SourceBufferPrivateAplus::onDecoderNotReadyToRender(RefPtr<AcinerellaDecoder>)
{
}

}

#endif
