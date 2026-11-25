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

#include "Delegates.h"

#include "AplusWebView.h"
#include "App.h"
#include "BrowserWindow.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <unistd.h>

namespace Delegates {

std::string formatSize(long long bytes)
{
    char buffer[32];

    if (bytes < 1024) {
        snprintf(buffer, sizeof(buffer), "%lld B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buffer, sizeof(buffer), "%.0f KB", bytes / 1024.0);
    } else if (bytes < 1024LL * 1024 * 1024) {
        snprintf(buffer, sizeof(buffer), "%.1f MB", bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buffer, sizeof(buffer), "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    }

    return buffer;
}

std::string downloadPath(const std::string& filename)
{
    std::string dir = App::downloadsDirectory();
    std::string name = filename.empty() ? std::string("download") : filename;
    std::replace(name.begin(), name.end(), '/', '_');

    std::string path = dir + "/" + name;
    size_t dot = name.rfind('.');
    std::string stem = dot == std::string::npos || dot == 0 ? name : name.substr(0, dot);
    std::string ext = dot == std::string::npos || dot == 0 ? std::string() : name.substr(dot);

    for (int i = 1; access(path.c_str(), F_OK) == 0; i++) {
        path = dir + "/" + stem + " (" + std::to_string(i) + ")" + ext;
    }

    return path;
}

}

using Delegates::formatSize;

static std::string nameFromURL(const std::string& url)
{
    std::string path = url;
    size_t sep = path.find("://");
    if (sep != std::string::npos) {
        path = path.substr(sep + 3);
    }
    size_t end = path.find_first_of("?#");
    if (end != std::string::npos) {
        path = path.substr(0, end);
    }
    size_t slash = path.rfind('/');
    if (slash == std::string::npos) {
        return std::string();
    }
    return path.substr(slash + 1);
}

class Download final : public AplusDownloadClient {
public:
    Download(const std::string& url, const std::string& suggested)
        : m_suggested(suggested)
    {
        m_download = AplusDownload::create(url.c_str(), this);
    }

    void start()
    {
        std::string name = m_suggested.empty() ? nameFromURL(m_download->url()) : m_suggested;
        m_path = Delegates::downloadPath(name.empty() ? std::string("download") : name);
        m_name = m_path.substr(m_path.rfind('/') + 1);
        m_download->start(m_path.c_str());
        report();
    }

    void downloadDidReceiveResponse(AplusDownload* download, long long expected, const char* suggested, const char*) override
    {
        m_expected = expected;

        if (m_suggested.empty() && suggested && *suggested && m_name != suggested) {
            m_path = Delegates::downloadPath(suggested);
            m_name = m_path.substr(m_path.rfind('/') + 1);
            download->setDestination(m_path.c_str());
        }

        report();
    }

    void downloadDidReceiveData(AplusDownload*, unsigned length) override
    {
        m_received += length;

        double now = App::now();
        if (now - m_reported >= 0.25) {
            m_reported = now;
            report();
        }
    }

    void downloadDidFinish(AplusDownload*) override
    {
        flash("Downloaded " + m_name + " · " + formatSize(m_received), 6.0);
        delete this;
    }

    void downloadDidFail(AplusDownload*, bool cancelled) override
    {
        flash((cancelled ? "Download cancelled: " : "Download failed: ") + m_name, 6.0);
        delete this;
    }

private:
    static void flash(const std::string& text, double seconds)
    {
        if (BrowserWindow* window = App::activeWindow()) {
            window->flash(text, seconds);
        }
    }

    void report()
    {
        std::string text = "Downloading " + m_name + " · " + formatSize(m_received);

        if (m_expected > 0) {
            int percent = (int)std::min<long long>(100, m_received * 100 / m_expected);
            text += " of " + formatSize(m_expected) + " (" + std::to_string(percent) + "%)";
        }

        flash(text, 3.0);
    }

    AplusDownload* m_download { nullptr };
    std::string m_suggested;
    std::string m_path;
    std::string m_name;
    long long m_received { 0 };
    long long m_expected { -1 };
    double m_reported { 0 };
};

namespace Delegates {

void download(const std::string& url, const std::string& suggestedFilename)
{
    (new Download(url, suggestedFilename))->start();
}

}
