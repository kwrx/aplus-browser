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

#include "MediaDescription.h"

namespace WebCore {

class MediaDescriptionAplus final : public MediaDescription {
public:
    enum class Kind : uint8_t { Audio, Video, Text };

    static Ref<MediaDescriptionAplus> create(Kind kind, String&& codec) { return adoptRef(*new MediaDescriptionAplus(kind, WTF::move(codec))); }

    bool isVideo() const final { return m_kind == Kind::Video; }
    bool isAudio() const final { return m_kind == Kind::Audio; }
    bool isText() const final { return m_kind == Kind::Text; }

private:
    MediaDescriptionAplus(Kind kind, String&& codec)
        : MediaDescription(WTF::move(codec))
        , m_kind(kind)
    {
    }

    Kind m_kind;
};

}

#endif
