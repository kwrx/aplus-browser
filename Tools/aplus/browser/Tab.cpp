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

#include "Tab.h"

#include "App.h"
#include "BrowserWindow.h"
#include "Delegates.h"
#include "Draw.h"
#include "Pages.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static std::string normalize(const std::string& text)
{
    std::string url = text;

    while (!url.empty() && isspace((unsigned char)url.front())) {
        url.erase(url.begin());
    }
    while (!url.empty() && isspace((unsigned char)url.back())) {
        url.pop_back();
    }

    if (url.empty()) {
        return std::string();
    }

    if (url.find("://") != std::string::npos || url.rfind("about:", 0) == 0 || url.rfind("data:", 0) == 0 || url.rfind("javascript:", 0) == 0) {
        return url;
    }

    if (url[0] == '/') {
        return "file://" + url;
    }

    bool hostLike = url.find(' ') == std::string::npos && (url.find('.') != std::string::npos || url.rfind("localhost", 0) == 0);

    if (hostLike) {
        return "https://" + url;
    }

    std::string query;
    for (unsigned char c : url) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            query += (char)c;
        } else if (c == ' ') {
            query += '+';
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", c);
            query += hex;
        }
    }

    return "https://duckduckgo.com/html/?q=" + query;
}

static std::string hostOf(const std::string& url)
{
    size_t sep = url.find("://");
    if (sep == std::string::npos) {
        return std::string();
    }

    std::string host = url.substr(sep + 3);
    size_t end = host.find_first_of("/?#");
    if (end != std::string::npos) {
        host = host.substr(0, end);
    }
    size_t at = host.rfind('@');
    if (at != std::string::npos) {
        host = host.substr(at + 1);
    }
    return host;
}

Tab::Tab(BrowserWindow* window)
    : m_window(window)
{
}

Tab::~Tab()
{
    m_window->tabDestroyed(this);

    delete m_view;

    if (m_cr) {
        cairo_destroy(m_cr);
    }
    if (m_surface) {
        cairo_surface_destroy(m_surface);
    }
}

bool Tab::init(int width, int height)
{
    m_view = new AplusWebView(this);
    m_view->setUserAgent(App::userAgent().c_str());
    m_view->setPlaceholderColor(Draw::theme().background.r, Draw::theme().background.g, Draw::theme().background.b);

    m_width = 0;
    m_height = 0;
    resize(width, height);
    return m_surface != nullptr;
}

void Tab::resize(int width, int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);

    if (m_surface && width == m_width && height == m_height) {
        return;
    }

    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return;
    }

    cairo_t* cr = cairo_create(surface);
    const ui_color_t& fill = Draw::theme().background;
    cairo_set_source_rgb(cr, fill.r, fill.g, fill.b);
    cairo_paint(cr);

    if (m_cr) {
        cairo_destroy(m_cr);
    }
    if (m_surface) {
        cairo_surface_destroy(m_surface);
    }

    m_surface = surface;
    m_cr = cr;
    m_width = width;
    m_height = height;

    m_view->resize(width, height);
    addPending({ 0, 0, width, height });
}

void Tab::addPending(const ui_rect_t& rect)
{
    if (rect.width <= 0 || rect.height <= 0) {
        return;
    }

    if (m_hasPending) {
        m_pending = ui_rect_union(&m_pending, &rect);
    } else {
        m_pending = rect;
        m_hasPending = true;
    }

    m_window->tabDamaged(this);
}

void Tab::exposeIfNeeded()
{
    if (!m_view || !m_cr) {
        return;
    }

    if (m_needsUpdate) {
        m_needsUpdate = false;
        m_view->layoutIfNeeded();
    }

    if (!m_hasPending) {
        return;
    }

    ui_rect_t rect = ui_rect_clip(&m_pending, m_width, m_height);
    m_hasPending = false;

    if (ui_rect_empty(&rect)) {
        return;
    }

    cairo_save(m_cr);
    cairo_rectangle(m_cr, rect.x, rect.y, rect.width, rect.height);
    cairo_clip(m_cr);
    const ui_color_t& fill = Draw::theme().background;
    cairo_set_source_rgb(m_cr, fill.r, fill.g, fill.b);
    cairo_paint(m_cr);
    cairo_restore(m_cr);

    m_view->paint(m_cr, rect.x, rect.y, rect.width, rect.height);
    cairo_surface_flush(m_surface);

    if (m_hasDamage) {
        m_damage = ui_rect_union(&m_damage, &rect);
    } else {
        m_damage = rect;
        m_hasDamage = true;
    }
}

bool Tab::takeDamage(ui_rect_t& rect)
{
    if (!m_hasDamage) {
        return false;
    }

    rect = ui_rect_clip(&m_damage, m_width, m_height);
    m_hasDamage = false;
    return !ui_rect_empty(&rect);
}

void Tab::damageAll()
{
    addPending({ 0, 0, m_width, m_height });
}

void Tab::invalidate(int x, int y, int width, int height)
{
    addPending({ x, y, width, height });
}

void Tab::scheduleRenderingUpdate()
{
    m_needsUpdate = true;
    m_window->tabDamaged(this);
}

void Tab::screenSize(int& width, int& height)
{
    width = m_window->width();
    height = m_window->height();
}

void Tab::setCursor(int cursor)
{
    if (m_cursor != cursor) {
        m_cursor = cursor;
        m_window->tabCursorChanged(this);
    }
}

void Tab::setHoveredLink(const char* url, const char*)
{
    std::string link = url ? url : "";

    if (link != m_hoveredLink) {
        m_hoveredLink = link;
        m_window->tabStatusChanged(this);
    }
}

void Tab::setStatusText(const char* text)
{
    std::string status = text ? text : "";

    if (status != m_statusText) {
        m_statusText = status;
        m_window->tabStatusChanged(this);
    }
}

void Tab::titleChanged(const char* title)
{
    std::string text = title ? title : "";

    if (m_startPage || m_pendingStart) {
        return;
    }

    if (text != m_title) {
        m_title = text;
        m_window->tabChanged(this);
    }
}

void Tab::closeView()
{
    m_closing = true;
    m_window->tabChanged(this);
}

void Tab::openExternalURL(const char* url)
{
    App::openExternal(url);
}

bool Tab::isActive()
{
    return m_window->focused() && m_window->isActiveTab(this);
}

const char* Tab::userAgent()
{
    return App::userAgent().c_str();
}

void Tab::consoleMessage(const AplusConsoleMessage& message)
{
    static const bool enabled = getenv("APLUS_BROWSER_CONSOLE");

    if (enabled) {
        fprintf(stderr, "aplus-browser: console:%s:%u:%u: %s\n", message.source.c_str(), message.line, message.column, message.message.c_str());
    }
}

std::string Tab::dialogTitle() const
{
    std::string host = hostOf(m_view ? m_view->url() : std::string());
    return host.empty() ? std::string("This page says") : host + " says";
}

void Tab::jsAlert(const char* message)
{
    m_window->alert(this, dialogTitle(), message ? message : "");
}

bool Tab::jsConfirm(const char* message)
{
    return m_window->confirm(this, dialogTitle(), message ? message : "");
}

bool Tab::jsPrompt(const char* message, const char* defaultValue, std::string& result)
{
    return m_window->prompt(this, dialogTitle(), message ? message : "", defaultValue ? defaultValue : "", result);
}

bool Tab::authenticate(const char* host, const char* realm, bool, std::string& user, std::string& password)
{
    return m_window->login(this, host ? host : "", realm ? realm : "", user, password);
}

void Tab::downloadRequested(const char* url, const char* suggestedFilename)
{
    if (url && *url) {
        Delegates::download(url, suggestedFilename ? suggestedFilename : "");
    }
}

void Tab::showPopupMenu(const std::vector<AplusPopupItem>& items, int x, int y, int width, int height, int selected)
{
    m_window->showDropdown(this, items, x, y, width, height, selected);
}

void Tab::hidePopupMenu()
{
    m_window->hideDropdown(this);
}

void Tab::showContextMenu(const AplusHitTest& hit, int x, int y)
{
    m_window->showContextMenu(this, hit, x, y);
}

AplusWebView* Tab::createView(const char* url, bool background)
{
    Tab* tab = m_window->openTab(url, background);
    return tab ? tab->view() : nullptr;
}

void Tab::setURL(const std::string& url)
{
    std::string shown = url == "about:blank" ? std::string() : url;

    if (shown != m_url) {
        m_url = shown;
        m_hoveredLink.clear();
        m_window->tabChanged(this);
    }
}

void Tab::load(const std::string& text)
{
    std::string url = normalize(text);

    if (url.empty()) {
        showStartPage();
        return;
    }

    loadURL(url);
}

void Tab::loadURL(const std::string& url)
{
    m_pendingStart = false;
    m_pendingError = false;
    setURL(url);
    m_view->loadURL(url.c_str());
}

void Tab::showStartPage()
{
    m_pendingStart = true;
    m_pendingError = false;
    m_title = "New Tab";
    setURL(std::string());
    m_view->setPlaceholderColor(Draw::theme().background.r, Draw::theme().background.g, Draw::theme().background.b);
    m_view->loadHTML(Pages::startPage().c_str(), "about:blank");
    m_window->tabChanged(this);
}

void Tab::showError(const std::string& url, const std::string& description)
{
    m_pendingError = true;
    m_pendingStart = false;
    m_errorURL = url;
    m_view->loadHTML(Pages::errorPage(url, description).c_str(), url.c_str());
}

void Tab::reload()
{
    if (!m_errorURL.empty()) {
        loadURL(m_errorURL);
        return;
    }

    if (m_startPage) {
        showStartPage();
        return;
    }

    m_view->reload();
}

void Tab::stop()
{
    m_view->stop();

    if (m_loading) {
        m_loading = false;
        m_progress = 1.0;
        m_window->tabChanged(this);
    }
}

void Tab::goBack()
{
    m_view->goBack();
}

void Tab::goForward()
{
    m_view->goForward();
}

bool Tab::canGoBack() const
{
    return m_view && m_view->canGoBack();
}

bool Tab::canGoForward() const
{
    return m_view && m_view->canGoForward();
}

void Tab::setActive(bool active)
{
    if (!m_view) {
        return;
    }

    m_view->setFocused(active);
    m_view->setActive(active && m_window->focused());
    m_view->setVisible(active);
}

void Tab::zoomIn()
{
    m_view->zoomIn();
    m_window->tabChanged(this);
}

void Tab::zoomOut()
{
    m_view->zoomOut();
    m_window->tabChanged(this);
}

void Tab::resetZoom()
{
    m_view->resetZoom();
    m_window->tabChanged(this);
}

int Tab::zoomPercent() const
{
    return m_view ? (int)std::lround(m_view->zoom() * 100.0) : 100;
}

void Tab::loadStarted(const char*)
{
    if (!m_pendingError) {
        m_showingError = false;
        m_errorURL.clear();
    }
    if (!m_pendingStart) {
        m_startPage = false;
    }

    m_loading = true;
    m_progress = 0.08;
    m_window->tabChanged(this);
}

void Tab::loadCommitted(const char* url)
{
    if (m_pendingError) {
        m_pendingError = false;
        m_showingError = true;
        setURL(m_errorURL);
        return;
    }

    if (m_pendingStart) {
        m_pendingStart = false;
        m_startPage = true;
        setURL(std::string());
        return;
    }

    setURL(url ? url : "");
}

void Tab::urlChanged(const char* url)
{
    if (m_startPage || m_showingError || m_pendingStart || m_pendingError) {
        return;
    }

    setURL(url ? url : "");
}

void Tab::loadFinished()
{
    m_loading = false;
    m_progress = 1.0;
    m_window->tabChanged(this);
}

void Tab::loadFailed(const char* url, const char* description, bool cancelled)
{
    std::string failing = url ? url : "";

    m_loading = false;
    m_progress = 1.0;

    if (!cancelled && !failing.empty() && !m_pendingError) {
        showError(failing, description ? description : "");
    } else {
        m_pendingError = false;
        m_pendingStart = false;
    }

    m_window->tabChanged(this);
}

void Tab::progressChanged(double value)
{
    if (!m_loading) {
        return;
    }

    double progress = std::max(0.08, std::min(1.0, value));

    if (progress > m_progress) {
        m_progress = progress;
        m_window->tabChanged(this);
    }
}

void Tab::navigationStateChanged()
{
    m_window->tabChanged(this);
}

void Tab::setFullScreen(bool on)
{
    if (m_fullScreen == on) {
        return;
    }

    m_fullScreen = on;
    m_window->tabFullScreenChanged(this);
}

void Tab::exitFullScreen()
{
    if (!m_fullScreen) {
        return;
    }

    m_view->exitFullScreen();

    if (m_fullScreen) {
        setFullScreen(false);
    }
}
