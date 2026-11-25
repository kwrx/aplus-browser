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

#include "AplusWebView.h"

#include <aplus/ui.h>
#include <cairo.h>
#include <string>
#include <vector>

class BrowserWindow;

class Tab final : public AplusWebViewClient {
public:
    explicit Tab(BrowserWindow*);
    ~Tab() override;

    bool init(int width, int height);

    void screenSize(int& width, int& height) override;
    void setCursor(int cursor) override;
    void setHoveredLink(const char* url, const char* title) override;
    void setStatusText(const char* text) override;
    void titleChanged(const char* title) override;
    void invalidate(int x, int y, int width, int height) override;
    void scheduleRenderingUpdate() override;
    void loadStarted(const char* url) override;
    void loadCommitted(const char* url) override;
    void loadFinished() override;
    void loadFailed(const char* url, const char* description, bool cancelled) override;
    void urlChanged(const char* url) override;
    void progressChanged(double) override;
    void navigationStateChanged() override;
    AplusWebView* createView(const char* url, bool background) override;
    void closeView() override;
    bool isActive() override;
    void openExternalURL(const char* url) override;
    const char* userAgent() override;
    void consoleMessage(const AplusConsoleMessage&) override;
    void jsAlert(const char* message) override;
    bool jsConfirm(const char* message) override;
    bool jsPrompt(const char* message, const char* defaultValue, std::string& result) override;
    bool authenticate(const char* host, const char* realm, bool retry, std::string& user, std::string& password) override;
    void downloadRequested(const char* url, const char* suggestedFilename) override;
    void showPopupMenu(const std::vector<AplusPopupItem>&, int x, int y, int width, int height, int selected) override;
    void hidePopupMenu() override;
    void setFullScreen(bool) override;
    void showContextMenu(const AplusHitTest&, int x, int y) override;

    BrowserWindow* window() const { return m_window; }
    AplusWebView* view() const { return m_view; }
    cairo_surface_t* surface() const { return m_surface; }

    void load(const std::string& text);
    void loadURL(const std::string& url);
    void showStartPage();
    void showError(const std::string& url, const std::string& description);
    void reload();
    void stop();
    void goBack();
    void goForward();
    bool canGoBack() const;
    bool canGoForward() const;

    void resize(int width, int height);
    int width() const { return m_width; }
    int height() const { return m_height; }
    void exposeIfNeeded();
    bool needsExpose() const { return m_needsUpdate || m_hasPending; }
    bool takeDamage(ui_rect_t& rect);
    void damageAll();

    void setActive(bool active);
    void exitFullScreen();
    bool fullScreen() const { return m_fullScreen; }
    void zoomIn();
    void zoomOut();
    void resetZoom();
    int zoomPercent() const;

    const std::string& title() const { return m_title; }
    const std::string& url() const { return m_url; }
    const std::string& hoveredLink() const { return m_hoveredLink; }
    const std::string& statusText() const { return m_statusText; }
    bool loading() const { return m_loading; }
    double progress() const { return m_progress; }
    int cursor() const { return m_cursor; }
    bool closing() const { return m_closing; }
    bool isStartPage() const { return m_startPage; }

private:
    void setURL(const std::string&);
    void addPending(const ui_rect_t&);
    std::string dialogTitle() const;

    BrowserWindow* m_window { nullptr };
    AplusWebView* m_view { nullptr };
    cairo_surface_t* m_surface { nullptr };
    cairo_t* m_cr { nullptr };
    int m_width { 0 };
    int m_height { 0 };

    ui_rect_t m_pending { 0, 0, 0, 0 };
    bool m_hasPending { false };
    bool m_needsUpdate { false };
    ui_rect_t m_damage { 0, 0, 0, 0 };
    bool m_hasDamage { false };

    std::string m_title;
    std::string m_url;
    std::string m_hoveredLink;
    std::string m_statusText;
    std::string m_errorURL;
    bool m_loading { false };
    double m_progress { 0 };
    int m_cursor { 0 };
    bool m_closing { false };
    bool m_startPage { false };
    bool m_showingError { false };
    bool m_pendingStart { false };
    bool m_pendingError { false };
    bool m_fullScreen { false };
};
