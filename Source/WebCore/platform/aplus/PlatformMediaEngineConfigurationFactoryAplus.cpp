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
#include "PlatformMediaEngineConfigurationFactoryAplus.h"

#if ENABLE(VIDEO)

#include "ContentType.h"
#include "MediaPlayer.h"
#include "PlatformMediaCapabilitiesDecodingInfo.h"
#include "PlatformMediaDecodingConfiguration.h"
#include <wtf/Function.h>

namespace WebCore {

static bool isTypeSupported(const String& contentType, PlatformMediaDecodingType type)
{
    MediaEngineSupportParameters parameters;
    parameters.type = ContentType(contentType);
    parameters.platformType = type;
    return MediaPlayer::supportsType(parameters) != MediaPlayer::SupportsType::IsNotSupported;
}

void createMediaPlayerDecodingConfigurationAplus(PlatformMediaDecodingConfiguration&& configuration, Function<void(PlatformMediaCapabilitiesDecodingInfo&&)>&& callback)
{
    bool supported = configuration.type != PlatformMediaDecodingType::MediaStream;
    bool smooth = true;

    if (supported && configuration.video) {
        auto& video = *configuration.video;
        supported = isTypeSupported(video.contentType, configuration.type);
        smooth = static_cast<double>(video.width) * video.height * video.framerate <= 1920.0 * 1080.0 * 30.0;
    }
    if (supported && configuration.audio) {
        supported = isTypeSupported(configuration.audio->contentType, configuration.type);
    }

    PlatformMediaCapabilitiesDecodingInfo info;
    info.supported = supported;
    info.smooth = supported && smooth;
    info.powerEfficient = false;
    info.configuration = WTF::move(configuration);
    callback(WTF::move(info));
}

}

#endif
