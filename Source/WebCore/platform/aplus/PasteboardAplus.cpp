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
#include "Pasteboard.h"

#include "AplusBrowserHost.h"
#include "Color.h"
#include "DragData.h"
#include "NotImplemented.h"
#include "PasteboardCustomData.h"
#include "SharedBuffer.h"
#include <stdlib.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>

namespace WebCore {

struct AplusPasteboardState {
    String text;
    String markup;
    String url;
};

static AplusPasteboardState& pasteboardState()
{
    static NeverDestroyed<AplusPasteboardState> state;
    return state;
}

static void writeClipboardText(const String& text)
{
    pasteboardState().text = text;

    if (auto* clipboard = aplusClipboard()) {
        auto utf8 = text.utf8();
        clipboard->writeText(utf8.data(), utf8.length());
    }
}

static String readClipboardText()
{
    auto* clipboard = aplusClipboard();

    if (!clipboard) {
        return pasteboardState().text;
    }

    char* data = clipboard->readText();

    if (!data) {
        return { };
    }

    String text = String::fromUTF8(data);
    free(data);

    if (text != pasteboardState().text) {
        pasteboardState().text = text;
        pasteboardState().markup = { };
        pasteboardState().url = { };
    }

    return text;
}

static bool isTextType(const String& type)
{
    return type == "text/plain"_s || type == "text"_s || type.startsWith("text/plain;"_s);
}

static bool isURLType(const String& type)
{
    return type == "text/uri-list"_s || type == "url"_s;
}

static bool isMarkupType(const String& type)
{
    return type == "text/html"_s;
}

std::unique_ptr<Pasteboard> Pasteboard::createForCopyAndPaste(std::unique_ptr<PasteboardContext>&& context)
{
    return makeUnique<Pasteboard>(WTF::move(context));
}

#if ENABLE(DRAG_SUPPORT)
std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop(std::unique_ptr<PasteboardContext>&& context)
{
    return makeUnique<Pasteboard>(WTF::move(context));
}

std::unique_ptr<Pasteboard> Pasteboard::create(const DragData&)
{
    return makeUnique<Pasteboard>(nullptr);
}

void Pasteboard::setDragImage(DragImage, const IntPoint&)
{
}
#endif

Pasteboard::Pasteboard(std::unique_ptr<PasteboardContext>&& context)
    : m_context(WTF::move(context))
{
}

bool Pasteboard::hasData()
{
    return !readClipboardText().isEmpty();
}

Vector<String> Pasteboard::typesSafeForBindings(const String&)
{
    return typesForLegacyUnsafeBindings();
}

Vector<String> Pasteboard::typesForLegacyUnsafeBindings()
{
    Vector<String> types;

    if (readClipboardText().isEmpty()) {
        return types;
    }

    types.append("text/plain"_s);

    if (!pasteboardState().markup.isEmpty()) {
        types.append("text/html"_s);
    }

    if (!pasteboardState().url.isEmpty()) {
        types.append("text/uri-list"_s);
    }

    return types;
}

String Pasteboard::readOrigin()
{
    return { };
}

String Pasteboard::readString(const String& type)
{
    String text = readClipboardText();

    if (isTextType(type)) {
        return text;
    }

    if (isMarkupType(type)) {
        return pasteboardState().markup;
    }

    if (isURLType(type)) {
        return pasteboardState().url;
    }

    return { };
}

String Pasteboard::readStringInCustomData(const String&)
{
    return { };
}

void Pasteboard::writeString(const String& type, const String& data)
{
    if (isTextType(type)) {
        writeClipboardText(data);
        pasteboardState().markup = { };
        pasteboardState().url = { };
    } else if (isMarkupType(type)) {
        pasteboardState().markup = data;
    } else if (isURLType(type)) {
        pasteboardState().url = data;

        if (pasteboardState().text.isEmpty()) {
            writeClipboardText(data);
        }
    }
}

void Pasteboard::clear()
{
    pasteboardState().markup = { };
    pasteboardState().url = { };
    writeClipboardText(emptyString());
}

void Pasteboard::clear(const String& type)
{
    if (isTextType(type)) {
        writeClipboardText(emptyString());
    } else if (isMarkupType(type)) {
        pasteboardState().markup = { };
    } else if (isURLType(type)) {
        pasteboardState().url = { };
    }
}

void Pasteboard::read(PasteboardPlainText& text, PlainTextURLReadingPolicy, std::optional<size_t>)
{
    text.text = readClipboardText();
}

void Pasteboard::read(PasteboardWebContentReader& reader, WebContentReadingPolicy policy, std::optional<size_t>)
{
    String text = readClipboardText();

    if (text.isEmpty()) {
        return;
    }

    if (policy == WebContentReadingPolicy::AnyType && !pasteboardState().markup.isEmpty()) {
        if (reader.readHTML(pasteboardState().markup)) {
            return;
        }
    }

    reader.readPlainText(text);
}

void Pasteboard::read(PasteboardFileReader&, std::optional<size_t>)
{
}

void Pasteboard::write(const Color&)
{
}

void Pasteboard::write(const PasteboardURL& url)
{
    writeClipboardText(url.url.string());
    pasteboardState().markup = { };
    pasteboardState().url = url.url.string();
}

void Pasteboard::writeTrustworthyWebURLsPboardType(const PasteboardURL&)
{
    notImplemented();
}

void Pasteboard::write(const PasteboardImage&)
{
    notImplemented();
}

void Pasteboard::write(const PasteboardBuffer&)
{
    notImplemented();
}

void Pasteboard::write(const PasteboardWebContent& content)
{
    writeClipboardText(content.text);
    pasteboardState().markup = content.markup;
    pasteboardState().url = { };
}

void Pasteboard::writeCustomData(const Vector<PasteboardCustomData>& data)
{
    if (data.isEmpty()) {
        return;
    }

    const auto& item = data.first();

    if (item.hasSameOriginCustomData()) {
        return;
    }

    for (auto& entry : item.data()) {
        if (std::holds_alternative<String>(entry.platformData)) {
            writeString(entry.type, std::get<String>(entry.platformData));
        }
    }
}

Pasteboard::FileContentState Pasteboard::fileContentState()
{
    return FileContentState::NoFileOrImageData;
}

bool Pasteboard::canSmartReplace()
{
    return false;
}

void Pasteboard::writeMarkup(const String& markup)
{
    pasteboardState().markup = markup;
}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption)
{
    writeClipboardText(text);
    pasteboardState().markup = { };
    pasteboardState().url = { };
}

}
