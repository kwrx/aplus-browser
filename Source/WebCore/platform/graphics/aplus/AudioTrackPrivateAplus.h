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

#include "AudioTrackPrivate.h"

namespace WebCore {

class AudioTrackPrivateAplus final : public AudioTrackPrivate {
public:
    static Ref<AudioTrackPrivateAplus> create(TrackID id) { return adoptRef(*new AudioTrackPrivateAplus(id)); }

    TrackID id() const final { return m_id; }
    Kind kind() const final { return Kind::Main; }
    std::optional<bool> defaultEnabled() const final { return true; }

private:
    explicit AudioTrackPrivateAplus(TrackID id)
        : m_id(id)
    {
    }

    TrackID m_id;
};

}

#endif
