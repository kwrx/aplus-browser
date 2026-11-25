/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com> All rights reserved.
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com> All rights reserved.
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ChromeClientAplus.h"

#include "AplusWebView.h"
#include "PopupMenuAplus.h"
#include "SearchPopupMenuAplus.h"
#include <WebCore/ColorChooser.h>
#include <WebCore/Cursor.h>
#include <WebCore/DateTimeChooser.h>
#include <WebCore/DataListSuggestionPicker.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentFullscreen.h>
#include <WebCore/Element.h>
#include <WebCore/FileChooser.h>
#include <WebCore/FileIconLoader.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/Icon.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/WindowFeatures.h>
#include <WebCore/DocumentPage.h>
#include <WebCore/DocumentView.h>
#include <WebCore/NodeDocument.h>

namespace WebCore {

ChromeClientAplus::ChromeClientAplus(AplusWebView* view)
    : m_view(view)
{
}

ChromeClientAplus::~ChromeClientAplus() = default;

void ChromeClientAplus::chromeDestroyed()
{
}

void ChromeClientAplus::setWindowRect(const FloatRect&)
{
}

FloatRect ChromeClientAplus::windowRect() const
{
    return FloatRect(0, 0, m_view->width(), m_view->height());
}

FloatRect ChromeClientAplus::pageRect() const
{
    auto* frame = m_view->mainFrame();
    if (!frame || !frame->view()) {
        return windowRect();
    }
    IntSize size = frame->view()->contentsSize();
    return FloatRect(0, 0, size.width(), size.height());
}

void ChromeClientAplus::focus()
{
    m_view->client()->focusView();
}

void ChromeClientAplus::unfocus()
{
}

bool ChromeClientAplus::canTakeFocus(FocusDirection) const
{
    return true;
}

void ChromeClientAplus::takeFocus(FocusDirection)
{
}

void ChromeClientAplus::focusedElementChanged(Element*, LocalFrame*, FocusOptions, BroadcastFocusedElement)
{
}

void ChromeClientAplus::focusedFrameChanged(Frame*)
{
}

RefPtr<Page> ChromeClientAplus::createWindow(LocalFrame&, const String&, const WindowFeatures&, const NavigationAction&)
{
    AplusWebView* view = m_view->client()->createView(nullptr, false);
    if (!view) {
        return nullptr;
    }
    return view->page();
}

void ChromeClientAplus::closeWindow()
{
    m_view->page()->setGroupName(String());
    if (auto* frame = m_view->mainFrame()) {
        frame->loader().stopAllLoaders();
    }
    m_view->client()->closeView();
}

void ChromeClientAplus::show()
{
}

bool ChromeClientAplus::canRunModal() const
{
    return false;
}

void ChromeClientAplus::runModal()
{
}

bool ChromeClientAplus::isPopup() const
{
    return false;
}

void ChromeClientAplus::setResizable(bool)
{
}

void ChromeClientAplus::addMessageToConsole(JSC::MessageSource, JSC::MessageLevel, const String& message, unsigned lineNumber, unsigned columnNumber, const String& sourceID)
{
    AplusConsoleMessage msg;
    msg.source = sourceID.utf8().data();
    msg.message = message.utf8().data();
    msg.line = lineNumber;
    msg.column = columnNumber;
    m_view->client()->consoleMessage(msg);
}

bool ChromeClientAplus::canRunBeforeUnloadConfirmPanel()
{
    return true;
}

bool ChromeClientAplus::runBeforeUnloadConfirmPanel(String&& message, LocalFrame&)
{
    return m_view->client()->jsBeforeUnload(message.utf8().data());
}

void ChromeClientAplus::runJavaScriptAlert(LocalFrame&, const String& message)
{
    m_view->client()->jsAlert(message.utf8().data());
}

bool ChromeClientAplus::runJavaScriptConfirm(LocalFrame&, const String& message)
{
    return m_view->client()->jsConfirm(message.utf8().data());
}

bool ChromeClientAplus::runJavaScriptPrompt(LocalFrame&, const String& message, const String& defaultValue, String& result)
{
    std::string value;
    if (!m_view->client()->jsPrompt(message.utf8().data(), defaultValue.utf8().data(), value)) {
        return false;
    }
    result = String::fromUTF8(value.c_str());
    return true;
}

KeyboardUIMode ChromeClientAplus::keyboardUIMode()
{
    return KeyboardAccessFull;
}

void ChromeClientAplus::invalidateRootView(const IntRect& rect)
{
    m_view->client()->invalidate(rect.x(), rect.y(), rect.width(), rect.height());
}

void ChromeClientAplus::invalidateContentsAndRootView(const IntRect& rect)
{
    m_view->client()->invalidate(rect.x(), rect.y(), rect.width(), rect.height());
}

void ChromeClientAplus::invalidateContentsForSlowScroll(const IntRect& rect)
{
    m_view->client()->invalidate(rect.x(), rect.y(), rect.width(), rect.height());
}

void ChromeClientAplus::scroll(const IntSize& delta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    IntRect rect = intersection(rectToScroll, clipRect);
    m_view->client()->scrollRect(delta.width(), delta.height(), rect.x(), rect.y(), rect.width(), rect.height());
}

IntPoint ChromeClientAplus::screenToRootView(const IntPoint& point) const
{
    return point;
}

IntPoint ChromeClientAplus::rootViewToScreen(const IntPoint& point) const
{
    return point;
}

IntRect ChromeClientAplus::rootViewToScreen(const IntRect& rect) const
{
    return rect;
}

IntPoint ChromeClientAplus::accessibilityScreenToRootView(const IntPoint& point) const
{
    return point;
}

IntRect ChromeClientAplus::rootViewToAccessibilityScreen(const IntRect& rect) const
{
    return rect;
}

PlatformPageClient ChromeClientAplus::platformPageClient() const
{
    return m_view->client();
}

void ChromeClientAplus::setCursor(const Cursor& cursor)
{
    m_view->client()->setCursor(static_cast<int>(reinterpret_cast<intptr_t>(cursor.platformCursor())));
}

void ChromeClientAplus::contentsSizeChanged(LocalFrame&, const IntSize&) const
{
}

void ChromeClientAplus::intrinsicContentsSizeChanged(const IntSize&) const
{
}

void ChromeClientAplus::scrollContainingScrollViewsToRevealRect(const IntRect&) const
{
}

void ChromeClientAplus::mouseDidMoveOverElement(const HitTestResult& result, OptionSet<PlatformEventModifier>, const String& toolTip, TextDirection)
{
    TextDirection dir;
    URL url = result.absoluteLinkURL();
    String title = result.title(dir);
    if (url != m_hoverURL || title != m_hoverTitle) {
        m_hoverURL = url;
        m_hoverTitle = title;
        m_view->client()->setHoveredLink(url.isEmpty() ? nullptr : url.string().utf8().data(), title.utf8().data());
    }
    if (toolTip != m_toolTip) {
        m_toolTip = toolTip;
        m_view->client()->setToolTip(toolTip.isEmpty() ? nullptr : toolTip.utf8().data());
    }
}

void ChromeClientAplus::print(LocalFrame&, const StringWithDirection&)
{
    notImplemented();
}

void ChromeClientAplus::exceededDatabaseQuota(LocalFrame&, const String&, DatabaseDetails)
{
}

RefPtr<ColorChooser> ChromeClientAplus::createColorChooser(ColorChooserClient&, const Color&)
{
    return nullptr;
}

RefPtr<DataListSuggestionPicker> ChromeClientAplus::createDataListSuggestionPicker(DataListSuggestionsClient&)
{
    return nullptr;
}

bool ChromeClientAplus::canShowDataListSuggestionLabels() const
{
    return false;
}

RefPtr<DateTimeChooser> ChromeClientAplus::createDateTimeChooser(DateTimeChooserClient&)
{
    return nullptr;
}

void ChromeClientAplus::runOpenPanel(LocalFrame&, FileChooser& chooser)
{
    std::vector<std::string> accept;
    for (auto& type : chooser.settings().acceptMIMETypes) {
        accept.push_back(type.utf8().data());
    }
    std::vector<std::string> files;
    if (!m_view->client()->chooseFiles(chooser.settings().allowsMultipleFiles, accept, files) || files.empty()) {
        chooser.cancelFileChoosing();
        return;
    }
    Vector<String> paths;
    for (auto& file : files) {
        paths.append(String::fromUTF8(file.c_str()));
    }
    chooser.chooseFiles(paths);
}

RefPtr<Icon> ChromeClientAplus::createIconForFiles(const Vector<String>&)
{
    return nullptr;
}

void ChromeClientAplus::loadIconForFiles(const Vector<String>&, FileIconLoader&)
{
}

RefPtr<PopupMenu> ChromeClientAplus::createPopupMenu(PopupMenuClient& client) const
{
    return adoptRef(new PopupMenuAplus(&client, m_view));
}

RefPtr<SearchPopupMenu> ChromeClientAplus::createSearchPopupMenu(PopupMenuClient& client) const
{
    return adoptRef(new SearchPopupMenuAplus(&client, m_view));
}

void ChromeClientAplus::attachRootGraphicsLayer(LocalFrame&, GraphicsLayer*)
{
}

void ChromeClientAplus::attachViewOverlayGraphicsLayer(GraphicsLayer*)
{
}

void ChromeClientAplus::setNeedsOneShotDrawingSynchronization()
{
}

void ChromeClientAplus::triggerRenderingUpdate()
{
    m_view->client()->scheduleRenderingUpdate();
}

#if ENABLE(FULLSCREEN_API)
bool ChromeClientAplus::supportsFullScreenForElement(const Element&, bool)
{
    return true;
}

void ChromeClientAplus::enterFullScreenForElement(Element& element, HTMLMediaElementEnums::VideoFullscreenMode mode, CompletionHandler<void(ExceptionOr<void>)>&& willEnterFullscreen, CompletionHandler<bool(bool)>&& didEnterFullscreen)
{
    auto result = element.document().fullscreen().willEnterFullscreen(element, mode);
    bool failed = result.hasException();
    willEnterFullscreen(WTF::move(result));
    if (failed) {
        didEnterFullscreen(false);
        return;
    }
    m_view->client()->setFullScreen(true);
    didEnterFullscreen(true);
}

void ChromeClientAplus::exitFullScreenForElement(Element* element, CompletionHandler<void()>&& completionHandler)
{
    if (element) {
        element->document().fullscreen().willExitFullscreen();
    }
    m_view->client()->setFullScreen(false);
    completionHandler();
}
#endif

}
