/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com> All rights reserved.
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com> All rights reserved.
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


#pragma once

#include <WebCore/ChromeClient.h>
#include <WebCore/FloatRect.h>
#include <wtf/URL.h>

class AplusWebView;

namespace WebCore {

class ChromeClientAplus final : public ChromeClient {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(ChromeClientAplus);

public:
    explicit ChromeClientAplus(AplusWebView*);
    ~ChromeClientAplus();

    void chromeDestroyed() override;

    void setWindowRect(const FloatRect&) override;
    FloatRect windowRect() const override;
    FloatRect pageRect() const override;

    void focus() override;
    void unfocus() override;
    bool canTakeFocus(FocusDirection) const override;
    void takeFocus(FocusDirection) override;
    void focusedElementChanged(Element*, LocalFrame*, FocusOptions, BroadcastFocusedElement) override;
    void focusedFrameChanged(Frame*) override;

    RefPtr<Page> createWindow(LocalFrame&, const String&, const WindowFeatures&, const NavigationAction&) override;
    void closeWindow() override;
    void show() override;
    bool canRunModal() const override;
    void runModal() override;
    bool isPopup() const override;
    void setResizable(bool) override;

    void addMessageToConsole(JSC::MessageSource, JSC::MessageLevel, const String& message, unsigned lineNumber, unsigned columnNumber, const String& sourceID) override;

    bool canRunBeforeUnloadConfirmPanel() override;
    bool runBeforeUnloadConfirmPanel(String&& message, LocalFrame&) override;

    void rootFrameAdded(const LocalFrame&) override { }
    void rootFrameRemoved(const LocalFrame&) override { }

    void runJavaScriptAlert(LocalFrame&, const String&) override;
    bool runJavaScriptConfirm(LocalFrame&, const String&) override;
    bool runJavaScriptPrompt(LocalFrame&, const String& message, const String& defaultValue, String& result) override;
    KeyboardUIMode keyboardUIMode() override;

    bool hasAccessoryMousePointingDevice() const override { return false; }
    bool hoverSupportedByPrimaryPointingDevice() const override { return true; }
    bool hoverSupportedByAnyAvailablePointingDevice() const override { return true; }
    std::optional<PointerCharacteristics> pointerCharacteristicsOfPrimaryPointingDevice() const override { return PointerCharacteristics::Fine; }
    OptionSet<PointerCharacteristics> pointerCharacteristicsOfAllAvailablePointingDevices() const override { return PointerCharacteristics::Fine; }

    void invalidateRootView(const IntRect&) override;
    void invalidateContentsAndRootView(const IntRect&) override;
    void invalidateContentsForSlowScroll(const IntRect&) override;
    void scroll(const IntSize&, const IntRect&, const IntRect&) override;

    IntPoint screenToRootView(const IntPoint&) const override;
    IntPoint rootViewToScreen(const IntPoint&) const override;
    IntRect rootViewToScreen(const IntRect&) const override;
    IntPoint accessibilityScreenToRootView(const IntPoint&) const override;
    IntRect rootViewToAccessibilityScreen(const IntRect&) const override;

    void didFinishLoadingImageForElement(HTMLImageElement&) override { }

    PlatformPageClient platformPageClient() const override;
    void setCursor(const Cursor&) override;
    void setCursorHiddenUntilMouseMoves(bool) override { }

    void contentsSizeChanged(LocalFrame&, const IntSize&) const override;
    void intrinsicContentsSizeChanged(const IntSize&) const override;
    void scrollContainingScrollViewsToRevealRect(const IntRect&) const override;

    void mouseDidMoveOverElement(const HitTestResult&, OptionSet<PlatformEventModifier>, const String&, TextDirection) override;
    void print(LocalFrame&, const StringWithDirection&) override;
    void exceededDatabaseQuota(LocalFrame&, const String&, DatabaseDetails) override;

    RefPtr<ColorChooser> createColorChooser(ColorChooserClient&, const Color&) override;
    RefPtr<DataListSuggestionPicker> createDataListSuggestionPicker(DataListSuggestionsClient&) override;
    bool canShowDataListSuggestionLabels() const override;
    RefPtr<DateTimeChooser> createDateTimeChooser(DateTimeChooserClient&) override;

    void setTextIndicator(RefPtr<TextIndicator>&&) const override { }
    void updateTextIndicator(RefPtr<TextIndicator>&&) const override { }

    void runOpenPanel(LocalFrame&, FileChooser&) override;
    void loadIconForFiles(const Vector<String>&, FileIconLoader&) override;
    RefPtr<Icon> createIconForFiles(const Vector<String>&) override;

    void postAccessibilityNotification(AccessibilityObject&, AXNotification) override { }
    void postAccessibilityNodeTextChangeNotification(AccessibilityObject*, AXTextChange, unsigned, const String&) override { }
    void postAccessibilityFrameLoadingEventNotification(AccessibilityObject*, AXLoadingEvent) override { }

    RefPtr<PopupMenu> createPopupMenu(PopupMenuClient&) const override;
    RefPtr<SearchPopupMenu> createSearchPopupMenu(PopupMenuClient&) const override;

    void wheelEventHandlersChanged(bool) override { }

    void attachRootGraphicsLayer(LocalFrame&, GraphicsLayer*) override;
    void attachViewOverlayGraphicsLayer(GraphicsLayer*) override;
    void setNeedsOneShotDrawingSynchronization() override;
    void triggerRenderingUpdate() override;
    CompositingTriggerFlags allowedCompositingTriggers() const override { return static_cast<CompositingTriggerFlags>(0); }

#if ENABLE(FULLSCREEN_API)
    bool supportsFullScreenForElement(const Element&, bool) override;
    void enterFullScreenForElement(Element&, HTMLMediaElementEnums::VideoFullscreenMode, CompletionHandler<void(ExceptionOr<void>)>&&, CompletionHandler<bool(bool)>&&) override;
    void exitFullScreenForElement(Element*, CompletionHandler<void()>&&) override;
#endif

private:
    AplusWebView* m_view;
    URL m_hoverURL;
    String m_hoverTitle;
    String m_toolTip;
};

}
