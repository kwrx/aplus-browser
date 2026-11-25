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
#include "UserAgent.h"

#include <wtf/NeverDestroyed.h>
#include <wtf/URL.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

static constexpr ASCIILiteral versionForUAString()
{
    return "605.1.15"_s;
}

static const String& standardUserAgentStatic()
{
    static NeverDestroyed<String> userAgent(makeString("Mozilla/5.0 (X11; aplus x86_64) AppleWebKit/"_s, versionForUAString(), " (KHTML, like Gecko) Version/26.0 Safari/"_s, versionForUAString()));
    return userAgent;
}

String standardUserAgent(const String& applicationName, const String& applicationVersion)
{
    if (applicationName.isEmpty()) {
        return standardUserAgentStatic();
    }

    return makeString(standardUserAgentStatic(), ' ', applicationName, '/', applicationVersion.isEmpty() ? String(versionForUAString()) : applicationVersion);
}

String standardUserAgentForURL(const URL&)
{
    return standardUserAgentStatic();
}

}
