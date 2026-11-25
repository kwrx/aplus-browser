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

#include "AplusBrowserHost.h"
#include "AplusEvents.h"

#include <cairo.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace WebCore {
class LocalFrame;
class Page;
class PopupMenuAplus;
}

class AplusWebView;

struct AplusHitTest {
    std::string linkURL;
    std::string linkTitle;
    std::string imageURL;
    std::string mediaURL;
    std::string selectedText;
    bool mediaIsVideo { false };
    bool editable { false };
    bool selected { false };
};

struct AplusPopupItem {
    std::string text;
    std::string toolTip;
    bool enabled { true };
    bool separator { false };
    bool label { false };
    bool selected { false };
};

struct AplusConsoleMessage {
    std::string source;
    std::string message;
    unsigned line { 0 };
    unsigned column { 0 };
};

enum class AplusEditCommand {
    Cut,
    Copy,
    Paste,
    Undo,
    Redo,
    SelectAll,
    Unselect,
};

enum AplusFindOptions {
    APLUS_FIND_BACKWARDS = 1 << 0,
    APLUS_FIND_CASE_SENSITIVE = 1 << 1,
    APLUS_FIND_WRAP = 1 << 2,
    APLUS_FIND_START_IN_SELECTION = 1 << 3,
};

class AplusWebViewClient : public AplusBrowserHost {
public:
    void setCursor(int) override { }

    virtual void invalidate(int x, int y, int width, int height) { }
    virtual void scrollRect(int dx, int dy, int x, int y, int width, int height) { invalidate(x, y, width, height); }
    virtual void scheduleRenderingUpdate() { }
    virtual void setToolTip(const char*) { }
    virtual void setHoveredLink(const char* url, const char* title) { }
    virtual void setStatusText(const char*) { }
    virtual void titleChanged(const char*) { }
    virtual void loadStarted(const char* url) { }
    virtual void loadCommitted(const char* url) { }
    virtual void loadFinished() { }
    virtual void loadFailed(const char* url, const char* description, bool cancelled) { }
    virtual void urlChanged(const char* url) { }
    virtual void progressChanged(double) { }
    virtual void navigationStateChanged() { }
    virtual void historyVisited(const char* url, const char* title) { }
    virtual AplusWebView* createView(const char* url, bool background) { return nullptr; }
    virtual void closeView() { }
    virtual void focusView() { }
    virtual bool isActive() { return true; }
    virtual void openExternalURL(const char* url) { aplusOpenExternal(url); }
    virtual const char* userAgent() { return nullptr; }
    virtual bool privateBrowsing() { return false; }
    virtual void consoleMessage(const AplusConsoleMessage&) { }
    virtual void jsAlert(const char* message) { }
    virtual bool jsConfirm(const char* message) { return false; }
    virtual bool jsPrompt(const char* message, const char* defaultValue, std::string& result) { return false; }
    virtual bool jsBeforeUnload(const char* message) { return true; }
    virtual bool authenticate(const char* host, const char* realm, bool retry, std::string& user, std::string& password) { return false; }
    virtual bool acceptCertificate(const char* url, const char* reason) { return false; }
    virtual void downloadRequested(const char* url, const char* suggestedFilename) { }
    virtual void showPopupMenu(const std::vector<AplusPopupItem>&, int x, int y, int width, int height, int selected) { }
    virtual void hidePopupMenu() { }
    virtual bool chooseFiles(bool multiple, const std::vector<std::string>& acceptMIMETypes, std::vector<std::string>& files) { return false; }
    virtual void setFullScreen(bool) { }
    virtual void showContextMenu(const AplusHitTest&, int x, int y) { }
};

class AplusDownload;

class AplusDownloadClient {
public:
    virtual ~AplusDownloadClient() = default;

    virtual void downloadDidReceiveResponse(AplusDownload*, long long expectedLength, const char* suggestedFilename, const char* mimeType) { }
    virtual void downloadDidReceiveData(AplusDownload*, unsigned length) { }
    virtual void downloadDidFinish(AplusDownload*) { }
    virtual void downloadDidFail(AplusDownload*, bool cancelled) { }
};

class AplusDownload {
public:
    static AplusDownload* create(const char* url, AplusDownloadClient*);

    virtual ~AplusDownload() = default;

    virtual const std::string& url() const = 0;
    virtual void start(const char* path) = 0;
    virtual void setDestination(const char* path) = 0;
    virtual void cancel() = 0;
};

class AplusWebSettings {
public:
    bool javascriptEnabled { true };
    bool imagesEnabled { true };
    bool localStorageEnabled { true };
    bool webSecurityEnabled { true };
    bool mediaEnabled { true };
    bool smoothScrolling { true };
    bool javascriptCanOpenWindows { false };
    double defaultFontSize { 16 };
    double defaultFixedFontSize { 13 };
    double minimumFontSize { 0 };
    std::string standardFontFamily;
    std::string fixedFontFamily;
    std::string serifFontFamily;
    std::string sansSerifFontFamily;
    std::string defaultTextEncoding;
};

class AplusWebView {
public:
    static void initialize();
    static void setCacheModel(size_t memoryCapacity, size_t diskCapacity);
    static void releaseMemory(bool critical);
    static void shutdown();

    static void run();
    static void quit();
    static void dispatch(std::function<void()>);
    static void dispatchAfter(double seconds, std::function<void()>);
    static void watchFd(int fd, std::function<void()> handler);
    static void unwatchFd(int fd);

    explicit AplusWebView(AplusWebViewClient*, bool privateBrowsing = false);
    ~AplusWebView();

    AplusWebView(const AplusWebView&) = delete;
    AplusWebView& operator=(const AplusWebView&) = delete;

    AplusWebViewClient* client() const { return m_client; }

    void loadURL(const char* url);
    void loadHTML(const char* html, const char* baseURL);
    void reload(bool bypassCache = false);
    void stop();
    void goBack();
    void goForward();
    bool canGoBack() const;
    bool canGoForward() const;

    std::string url() const;
    std::string title() const;
    std::string mimeType() const;
    std::string responseURL() const;
    std::string suggestedFilename() const;
    std::string selectedText() const;
    std::string source() const;
    std::vector<uint8_t> documentData() const;
    std::vector<uint8_t> archive() const;
    double progress() const;
    bool loading() const;

    void resize(int width, int height);
    int width() const { return m_width; }
    int height() const { return m_height; }
    void paint(cairo_t*, int x, int y, int width, int height);
    void layoutIfNeeded();

    void setFocused(bool);
    void setActive(bool);
    void setVisible(bool);

    bool keyEvent(const AplusKeyEvent&);
    bool pointerEvent(const AplusPointerEvent&);
    bool scrollEvent(const AplusScrollEvent&);
    void pointerLeave();

    void scrollBy(int dx, int dy);
    void scrollTo(int x, int y);
    int scrollX() const;
    int scrollY() const;

    void setZoom(double factor, bool textOnly = false);
    double zoom() const { return m_zoom; }
    void zoomIn();
    void zoomOut();
    void resetZoom();

    bool find(const char* text, unsigned options);
    unsigned markAllMatches(const char* text, unsigned options, unsigned limit);
    void unmarkAllMatches();

    bool canEdit(AplusEditCommand) const;
    void edit(AplusEditCommand);

    AplusHitTest hitTest(int x, int y) const;
    void exitFullScreen();

    void setUserAgent(const char*);
    void applySettings(const AplusWebSettings&);
    static AplusWebSettings& defaultSettings();
    static std::string preferredExtensionForMIMEType(const char*);
    void setTransparent(bool);
    bool transparent() const { return m_transparent; }
    void setPlaceholderColor(double r, double g, double b);
    uint32_t placeholderColor() const { return m_placeholderColor; }

    void popupMenuDidSelect(int index);
    void popupMenuDidHide();

    WebCore::Page* page() const { return m_page; }
    WebCore::LocalFrame* mainFrame() const;
    WebCore::LocalFrame* focusedFrame() const;

    void setPopupMenu(WebCore::PopupMenuAplus*);
    WebCore::PopupMenuAplus* popupMenu() const { return m_popup; }

    bool privateBrowsing() const { return m_private; }
    const std::string& customUserAgent() const { return m_userAgent; }

    void didStartProgress();
    void didChangeProgress(double);
    void didFinishProgress();
    void didChangeTitle(const std::string&);
    void didStartLoad(const std::string&);
    void didCommitLoad(const std::string&);
    void didFinishLoad();
    void didFailLoad(const std::string& url, const std::string& description, bool cancelled);
    void didChangeURL(const std::string&);

private:
    bool handleKeyScroll(const AplusKeyEvent&);
    bool handleShortcut(const AplusKeyEvent&, bool beforePage);
    bool showContextMenu(const AplusPointerEvent&);

    AplusWebViewClient* m_client;
    WebCore::Page* m_page { nullptr };
    WebCore::PopupMenuAplus* m_popup { nullptr };
    std::string m_userAgent;
    std::string m_title;
    std::string m_url;
    int m_width { 0 };
    int m_height { 0 };
    double m_zoom { 1 };
    double m_progress { 0 };
    bool m_loading { false };
    bool m_private { false };
    bool m_textZoom { false };
    bool m_transparent { false };
    uint32_t m_placeholderColor { 0xFFFFFFFF };
    bool m_pressed { false };
};
