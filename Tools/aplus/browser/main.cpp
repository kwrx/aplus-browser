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

#include "AplusWebView.h"
#include "App.h"
#include "BrowserWindow.h"
#include "Tab.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <string>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include <aplus/ui-keymap.h>
#include <aplus/ui-widgets.h>
#include <aplus/ui.h>

extern char** environ;

static constexpr uint16_t keyLeftMeta = 125;
static constexpr uint16_t keyRightMeta = 126;

static ui_connection_t* g_conn;
static ui_keymap_t* g_keymap;
static std::vector<BrowserWindow*> g_windows;
static std::vector<ui_event_t> g_pending;
static BrowserWindow* g_active;
static bool g_meta;
static bool g_pumpQueued;
static bool g_quitting;
static int g_status;
static unsigned g_timerGeneration;
static double g_timerDeadline = -1;
static double g_lastPump;

struct Modal {
    BrowserWindow* window;
    const int* result;
};

static std::vector<Modal> g_modals;
static const std::string g_userAgent = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.0 Safari/605.1.15";

static BrowserWindow* windowById(uint32_t id)
{
    for (auto* window : g_windows) {
        if (window->id() == id) {
            return window;
        }
    }
    return nullptr;
}

static void dispatchEvent(const ui_event_t& event)
{
    if (event.type == UI_EVENT_THEME) {
        ui_theme_update(g_conn);
        for (auto* window : g_windows) {
            window->handleEvent(event);
        }
        return;
    }

    if (BrowserWindow* window = windowById(event.window_id)) {
        window->handleEvent(event);
    }
}

class Clipboard final : public AplusClipboard {
public:
    void writeText(const char* data, size_t length) override
    {
        std::string text(data, length);
        ui_clipboard_set_text(g_conn, text.c_str());
    }

    char* readText() override
    {
        uint32_t serial = 0;
        if (ui_clipboard_request(g_conn, &serial) < 0) {
            return nullptr;
        }

        double deadline = App::now() + 0.5;

        while (App::now() < deadline) {
            ui_event_t event;
            int r = ui_next_event(g_conn, &event, 50);

            if (r < 0) {
                return nullptr;
            }
            if (r == 0) {
                continue;
            }

            if (event.type == UI_EVENT_CLIPBOARD && event.clipboard.serial == serial) {
                size_t size = 0;
                const char* mime = nullptr;
                const void* data = ui_clipboard_data(g_conn, &size, &mime);

                if (!data || !size) {
                    return nullptr;
                }

                char* out = (char*)malloc(size + 1);
                if (!out) {
                    return nullptr;
                }

                memcpy(out, data, size);
                out[size] = '\0';
                return out;
            }

            g_pending.push_back(event);
        }

        return nullptr;
    }
};

static Clipboard g_clipboard;

namespace App {

ui_connection_t* connection()
{
    return g_conn;
}

double now()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

uint8_t modifiers()
{
    uint8_t mods = g_keymap ? ui_keymap_modifiers(g_keymap) : 0;
    uint8_t out = 0;

    if (mods & ((1 << 0) | (1 << 4) | (1 << 5))) {
        out |= APLUS_MOD_SHIFT;
    }
    if (mods & ((1 << 2) | (1 << 6) | (1 << 7))) {
        out |= APLUS_MOD_CTRL;
    }
    if (mods & (1 << 3)) {
        out |= APLUS_MOD_ALT;
    }
    if (g_meta) {
        out |= APLUS_MOD_META;
    }

    return out;
}

void noteKey(uint16_t vkey, bool down)
{
    if (vkey == keyLeftMeta || vkey == keyRightMeta) {
        g_meta = down;
    }
}

size_t translate(uint16_t vkey, bool down, char* out, size_t size)
{
    if (!g_keymap || !size) {
        return 0;
    }

    size_t length = ui_keymap_translate(g_keymap, vkey, down, out, size);
    out[std::min(length, size - 1)] = '\0';
    return length;
}

const std::string& userAgent()
{
    return g_userAgent;
}

std::string clipboard()
{
    char* text = g_clipboard.readText();
    std::string out = text ? text : "";
    free(text);
    return out;
}

void setClipboard(const std::string& text)
{
    ui_clipboard_set_text(g_conn, text.c_str());
}

BrowserWindow* openWindow(const char* url)
{
    BrowserWindow* window = BrowserWindow::create(url);

    if (window) {
        g_windows.push_back(window);
        g_active = window;
    }

    return window;
}

BrowserWindow* activeWindow()
{
    return g_active;
}

void setActiveWindow(BrowserWindow* window)
{
    g_active = window;
}

void openExternal(const char* url)
{
    aplusOpenExternal(url);
}

std::string downloadsDirectory()
{
    const char* home = getenv("HOME");
    std::string dir = std::string(home && home[0] ? home : "/tmp") + "/Downloads";
    mkdir(dir.c_str(), 0755);
    return dir;
}

int runModal(BrowserWindow* modal, const int* result)
{
    g_modals.push_back({ modal, result });
    requestUpdate();

    while (*result < 0 && !modal->closed() && !g_quitting) {
        AplusWebView::run();
    }

    g_modals.pop_back();
    requestUpdate();
    return *result;
}

}

static void reapWindows()
{
    for (size_t i = 0; i < g_windows.size();) {
        BrowserWindow* window = g_windows[i];

        if (!window->closed()) {
            i++;
            continue;
        }

        g_windows.erase(g_windows.begin() + i);

        if (g_active == window) {
            g_active = g_windows.empty() ? nullptr : g_windows.back();
        }

        delete window;
    }
}

static bool modalDone(const Modal& modal)
{
    return *modal.result >= 0 || modal.window->closed() || g_quitting;
}

static void routeEvent(const ui_event_t& event)
{
    if (!g_modals.empty() && event.window_id != g_modals.back().window->id() && event.type != UI_EVENT_THEME) {
        g_pending.push_back(event);
        return;
    }

    dispatchEvent(event);
}

static void pump();

static void armTimer(int timeout)
{
    if (timeout < 0) {
        return;
    }

    double deadline = App::now() + timeout / 1000.0;

    if (g_timerDeadline >= 0 && g_timerDeadline <= deadline + 0.001) {
        return;
    }

    unsigned generation = ++g_timerGeneration;
    g_timerDeadline = deadline;

    AplusWebView::dispatchAfter(timeout / 1000.0, [generation] {
        if (generation != g_timerGeneration) {
            return;
        }
        g_timerDeadline = -1;
        pump();
    });
}

static void pump()
{
    g_lastPump = App::now();

    ui_event_t event;
    int r;

    while ((r = ui_next_event(g_conn, &event, 0)) > 0) {
        routeEvent(event);
    }

    if (r < 0) {
        g_status = 1;
        g_quitting = true;
    }

    if (g_modals.empty() && !g_pending.empty()) {
        std::vector<ui_event_t> pending;
        pending.swap(g_pending);
        for (auto& queued : pending) {
            routeEvent(queued);
        }
    }

    if (!g_modals.empty()) {
        if (modalDone(g_modals.back())) {
            AplusWebView::quit();
            return;
        }
        g_modals.back().window->update();
    } else {
        for (size_t i = 0; i < g_windows.size(); i++) {
            g_windows[i]->update();
        }

        reapWindows();
    }

    if (g_windows.empty() || g_quitting) {
        g_quitting = true;
        AplusWebView::quit();
        return;
    }

    int timeout = -1;

    for (auto* window : g_windows) {
        if (window->present() < 0) {
            window->close();
            timeout = 0;
        }

        int t = window->timeout();
        if (t >= 0 && (timeout < 0 || t < timeout)) {
            timeout = t;
        }
    }

    if (!g_pending.empty() && g_modals.empty()) {
        timeout = 0;
    }

    armTimer(timeout);
}

namespace App {

void requestUpdate()
{
    if (g_pumpQueued) {
        return;
    }

    g_pumpQueued = true;
    double wait = std::max(0.0, 0.016 - (now() - g_lastPump));

    AplusWebView::dispatchAfter(wait, [] {
        g_pumpQueued = false;
        pump();
    });
}

}

// FIXME: temporary SIGSEGV dump to /dev/kmsg to chase the random crash, remove it once fixed.
extern "C" char __executable_start[];
extern "C" char etext[];

namespace {

int g_crashFd = -1;

struct CrashLine {
    char buf[256];
    size_t len { 0 };

    void put(const char* s)
    {
        while (*s && len < sizeof(buf) - 1) {
            buf[len++] = *s++;
        }
    }

    void hex(unsigned long v)
    {
        char tmp[19] = "0x";
        for (int i = 0; i < 16; i++) {
            tmp[2 + i] = "0123456789abcdef"[(v >> (60 - 4 * i)) & 0xf];
        }
        tmp[18] = '\0';
        put(tmp);
    }

    void dec(long v)
    {
        char tmp[24];
        int n = 0;
        unsigned long u = v < 0 ? -static_cast<unsigned long>(v) : v;
        do {
            tmp[n++] = '0' + u % 10;
            u /= 10;
        } while (u && n < 22);
        if (v < 0) {
            tmp[n++] = '-';
        }
        while (n && len < sizeof(buf) - 1) {
            buf[len++] = tmp[--n];
        }
    }

    void flush()
    {
        buf[len++] = '\n';
        if (write(g_crashFd, buf, len) < 0) { }
        len = 0;
    }
};

void crashHandler(int sig, siginfo_t* info, void* context)
{
    static const char* const names[23] = { "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rdi", "rsi", "rbp", "rbx", "rdx", "rax", "rcx", "rsp", "rip", "efl", "csgsfs", "err", "trapno", "oldmask", "cr2" };
    const greg_t* g = static_cast<ucontext_t*>(context)->uc_mcontext.gregs;
    CrashLine line;

    line.put("aplus-browser: crash signal ");
    line.dec(sig);
    line.put(" code ");
    line.dec(info->si_code);
    line.put(" addr ");
    line.hex(reinterpret_cast<unsigned long>(info->si_addr));
    line.put(" pid ");
    line.dec(getpid());
    line.put(" text ");
    line.hex(reinterpret_cast<unsigned long>(__executable_start));
    line.put("-");
    line.hex(reinterpret_cast<unsigned long>(etext));
    line.flush();

    for (int i = 0; i <= REG_RIP; i += 4) {
        line.put("aplus-browser:");
        for (int j = i; j < i + 4 && j <= REG_RIP; j++) {
            line.put(" ");
            line.put(names[j]);
            line.put("=");
            line.hex(g[j]);
        }
        line.flush();
    }

    auto* sp = reinterpret_cast<unsigned long*>(g[REG_RSP] & ~7L);
    for (int i = 0; i < 32; i += 4) {
        line.put("aplus-browser: stack +");
        line.dec(i * 8);
        line.put(":");
        for (int j = i; j < i + 4; j++) {
            line.put(" ");
            line.hex(sp[j]);
        }
        line.flush();
    }

    auto lo = reinterpret_cast<unsigned long>(__executable_start);
    auto hi = reinterpret_cast<unsigned long>(etext);
    int found = 0;
    line.put("aplus-browser: trace");
    for (int i = 0; i < 4096 && found < 48; i++) {
        if ((reinterpret_cast<unsigned long>(&sp[i]) & 4095) == 0 && found % 6) {
            line.flush();
            line.put("aplus-browser: trace");
            found += 6 - found % 6;
        }
        unsigned long v = sp[i];
        if (v < lo || v >= hi) {
            continue;
        }
        line.put(" ");
        line.hex(v);
        if (++found % 6 == 0) {
            line.flush();
            line.put("aplus-browser: trace");
        }
    }
    line.flush();
}

void installCrashHandler()
{
    g_crashFd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
    if (g_crashFd < 0) {
        g_crashFd = STDERR_FILENO;
    }

    static char altstack[65536];
    stack_t ss { };
    ss.ss_sp = altstack;
    ss.ss_size = sizeof(altstack);
    sigaltstack(&ss, nullptr);

    struct sigaction sa { };
    sa.sa_sigaction = crashHandler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, nullptr);
}

}

int main(int argc, char** argv)
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    installCrashHandler();

    g_conn = ui_connect(nullptr, 2000);
    if (!g_conn) {
        fprintf(stderr, "aplus-browser: cannot connect to the window manager: %s\n", strerror(errno));
        return 1;
    }

    g_keymap = ui_keymap_open(nullptr);

    AplusWebView::initialize();
    aplusSetClipboard(&g_clipboard);

    if (!App::openWindow(argc > 1 ? argv[1] : nullptr)) {
        fprintf(stderr, "aplus-browser: cannot create the browser window\n");
        return 1;
    }

    AplusWebView::watchFd(ui_connection_fd(g_conn), [] {
        pump();
    });

    pump();

    while (!g_quitting) {
        AplusWebView::run();
    }

    AplusWebView::unwatchFd(ui_connection_fd(g_conn));

    while (!g_windows.empty()) {
        delete g_windows.back();
        g_windows.pop_back();
    }

    if (g_keymap) {
        ui_keymap_close(g_keymap);
    }

    AplusWebView::shutdown();
    ui_disconnect(g_conn);
    return g_status;
}
