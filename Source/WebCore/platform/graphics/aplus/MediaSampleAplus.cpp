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
#include "MediaSampleAplus.h"

#if ENABLE(MEDIA_SOURCE)

namespace WebCore {

MediaSampleAplus::MediaSampleAplus(Ref<Acinerella::AcinerellaPackage>&& package, TrackID trackID, const MediaTime& pts, const MediaTime& dts, const MediaTime& duration, SampleFlags flags, const FloatSize& presentationSize)
    : m_package(WTF::move(package))
    , m_trackID(trackID)
    , m_pts(pts)
    , m_dts(dts)
    , m_duration(duration)
    , m_flags(flags)
    , m_presentationSize(presentationSize)
{
    if (auto* raw = m_package->package()) {
        m_size = std::max(0, ac_get_package_size(raw));
    }
}

void MediaSampleAplus::offsetTimestampsBy(const MediaTime& offset)
{
    m_pts += offset;
    m_dts += offset;
}

void MediaSampleAplus::setTimestamps(const MediaTime& pts, const MediaTime& dts)
{
    m_pts = pts;
    m_dts = dts;
}

Ref<MediaSample> MediaSampleAplus::createNonDisplayingCopy() const
{
    return adoptRef(*new MediaSampleAplus(m_package.copyRef(), m_trackID, m_pts, m_dts, m_duration, static_cast<SampleFlags>(m_flags | IsNonDisplaying), m_presentationSize));
}

Ref<MediaSample> MediaSampleAplus::createCopyWithAdjustedStartTime(const MediaTime& offset) const
{
    auto shift = std::min(offset, m_duration);
    return adoptRef(*new MediaSampleAplus(m_package.copyRef(), m_trackID, m_pts + shift, m_dts + shift, m_duration - shift, m_flags, m_presentationSize));
}

}

#endif
