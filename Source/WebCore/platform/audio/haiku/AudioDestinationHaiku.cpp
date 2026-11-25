/*
 *  Copyright (C) 2014 Haiku, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioDestinationHaiku.h"

#include "NotImplemented.h"

#include "Logging.h"

namespace WebCore {

WTF::Ref<WebCore::AudioDestination> AudioDestination::create(const CreationOptions& options)
{
    return adoptRef(*new AudioDestinationHaiku(options));
}

float AudioDestination::hardwareSampleRate()
{
    // FIXME get the actual sample rate from the media kit
    notImplemented();
    return 44100;
}

unsigned long AudioDestination::maxChannelCount()
{
    // FIXME: query the default audio hardware device to return the actual number
    // of channels of the device. Also see corresponding FIXME in create().
    notImplemented();
    return 0;
}


unsigned AudioDestinationHaiku::framesPerBuffer() const
{
    notImplemented();
    return 0;
}


AudioDestinationHaiku::AudioDestinationHaiku(const CreationOptions& options)
    : AudioDestination(options)
{
    // FIXME implement
}


AudioDestinationHaiku::~AudioDestinationHaiku()
{
}


void AudioDestinationHaiku::start(Function<void(Function<void()>&&)>&& dispatchToRenderThread, CompletionHandler<void(bool)>&& completionHandler)
{
    notImplemented();
}


void AudioDestinationHaiku::stop(CompletionHandler<void(bool)>&& completionHandler)
{
    notImplemented();
}


} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
