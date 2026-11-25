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
#include "MIMETypeRegistry.h"

#include <wtf/text/StringView.h>

namespace WebCore {

struct ExtensionMap {
    ASCIILiteral extension;
    ASCIILiteral mimeType;
};

static constexpr ExtensionMap extensionMap[] = {
    { "avif"_s, "image/avif"_s },
    { "bmp"_s, "image/bmp"_s },
    { "css"_s, "text/css"_s },
    { "gif"_s, "image/gif"_s },
    { "htm"_s, "text/html"_s },
    { "html"_s, "text/html"_s },
    { "ico"_s, "image/x-icon"_s },
    { "jpeg"_s, "image/jpeg"_s },
    { "jpg"_s, "image/jpeg"_s },
    { "js"_s, "text/javascript"_s },
    { "json"_s, "application/json"_s },
    { "mjs"_s, "text/javascript"_s },
    { "mht"_s, "multipart/related"_s },
    { "mhtml"_s, "multipart/related"_s },
    { "mp3"_s, "audio/mpeg"_s },
    { "mp4"_s, "video/mp4"_s },
    { "oga"_s, "audio/ogg"_s },
    { "ogg"_s, "audio/ogg"_s },
    { "ogv"_s, "video/ogg"_s },
    { "otf"_s, "font/otf"_s },
    { "pdf"_s, "application/pdf"_s },
    { "png"_s, "image/png"_s },
    { "rss"_s, "application/rss+xml"_s },
    { "svg"_s, "image/svg+xml"_s },
    { "text"_s, "text/plain"_s },
    { "ttf"_s, "font/ttf"_s },
    { "txt"_s, "text/plain"_s },
    { "wasm"_s, "application/wasm"_s },
    { "wav"_s, "audio/wav"_s },
    { "webm"_s, "video/webm"_s },
    { "webp"_s, "image/webp"_s },
    { "woff"_s, "font/woff"_s },
    { "woff2"_s, "font/woff2"_s },
    { "xbm"_s, "image/x-xbitmap"_s },
    { "xht"_s, "application/xhtml+xml"_s },
    { "xhtml"_s, "application/xhtml+xml"_s },
    { "xml"_s, "text/xml"_s },
    { "xsl"_s, "text/xsl"_s },
    { "zip"_s, "application/zip"_s },
};

String MIMETypeRegistry::mimeTypeForExtension(StringView extension)
{
    for (auto& entry : extensionMap) {
        if (equalIgnoringASCIICase(extension, entry.extension)) {
            return entry.mimeType;
        }
    }

    return String();
}

String MIMETypeRegistry::preferredExtensionForMIMEType(const String& type)
{
    for (auto& entry : extensionMap) {
        if (equalIgnoringASCIICase(type, entry.mimeType)) {
            return entry.extension;
        }
    }

    return String();
}

Vector<String> MIMETypeRegistry::extensionsForMIMEType(const String& type)
{
    Vector<String> result;

    for (auto& entry : extensionMap) {
        if (equalIgnoringASCIICase(type, entry.mimeType)) {
            result.append(entry.extension);
        }
    }

    return result;
}

bool MIMETypeRegistry::isApplicationPluginMIMEType(const String&)
{
    return false;
}

}
