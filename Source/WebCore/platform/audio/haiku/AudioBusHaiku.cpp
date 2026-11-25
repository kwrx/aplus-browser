/*
    Copyright (C) 2014 Haiku, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License 2.1 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(WEB_AUDIO)
#include "AudioBus.h"

#include "AudioFileReader.h"

#include <Resources.h>
#include <TypeConstants.h>

namespace WebCore {

RefPtr<AudioBus> AudioBus::loadPlatformResource(const char* name, float sampleRate)
{
    BResources resources;
    // TODO initialize resource to webkit library? Or to the main app?
    size_t size = 0;
    const uint8_t* data = (const uint8_t*)resources.LoadResource(B_ANY_TYPE, name, &size);

    return createBusFromInMemoryAudioFile(std::span<const uint8_t>(data, size), false, sampleRate);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)

