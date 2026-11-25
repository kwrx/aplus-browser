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

#include "AcinerellaMuxer.h"
#include "FloatSize.h"
#include "MediaSample.h"

namespace WebCore {

class MediaSampleAplus final : public MediaSample {
public:
    static Ref<MediaSampleAplus> create(Ref<Acinerella::AcinerellaPackage>&& package, TrackID trackID, const MediaTime& pts, const MediaTime& dts, const MediaTime& duration, bool isSync, const FloatSize& presentationSize)
    {
        return adoptRef(*new MediaSampleAplus(WTF::move(package), trackID, pts, dts, duration, isSync ? IsSync : None, presentationSize));
    }

    MediaTime presentationTime() const final { return m_pts; }
    MediaTime decodeTime() const final { return m_dts; }
    MediaTime duration() const final { return m_duration; }
    TrackID trackID() const final { return m_trackID; }
    size_t sizeInBytes() const final { return m_size; }
    FloatSize presentationSize() const final { return m_presentationSize; }
    void offsetTimestampsBy(const MediaTime&) final;
    void setTimestamps(const MediaTime&, const MediaTime&) final;
    Ref<MediaSample> createNonDisplayingCopy() const final;
    Ref<MediaSample> createCopyWithAdjustedStartTime(const MediaTime&) const final;
    SampleFlags flags() const final { return m_flags; }
    PlatformSample platformSample() const final { return PlatformSample { static_cast<const MockSampleBox*>(nullptr) }; }
    Type type() const final { return Type::None; }

    Acinerella::AcinerellaPackage& package() const { return m_package.get(); }

private:
    MediaSampleAplus(Ref<Acinerella::AcinerellaPackage>&&, TrackID, const MediaTime& pts, const MediaTime& dts, const MediaTime& duration, SampleFlags, const FloatSize&);

    const Ref<Acinerella::AcinerellaPackage> m_package;
    TrackID m_trackID;
    MediaTime m_pts;
    MediaTime m_dts;
    MediaTime m_duration;
    size_t m_size { 0 };
    SampleFlags m_flags { None };
    FloatSize m_presentationSize;
};

}

#endif
