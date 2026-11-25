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
#include "AplusWebView.h"

#include <WebCore/CurlDownload.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace {

class AplusDownloadImpl final : public AplusDownload, public WebCore::CurlDownloadListener {
public:
    AplusDownloadImpl(const char* url, AplusDownloadClient* client)
        : m_client(client)
        , m_url(url ? url : "")
        , m_download(adoptRef(*new WebCore::CurlDownload()))
    {
        m_download->init(*this, URL { String::fromUTF8(m_url.c_str()) });
    }

    ~AplusDownloadImpl()
    {
        m_download->setListener(nullptr);
    }

    const std::string& url() const override { return m_url; }

    void start(const char* path) override
    {
        if (m_started || !path) {
            return;
        }

        m_started = true;
        m_path = path;
        m_download->setDeletesFileUponFailure(true);
        m_download->start(String::fromUTF8(path));
    }

    void setDestination(const char* path) override
    {
        if (!path) {
            return;
        }

        if (m_finished) {
            if (m_path != path) {
                FileSystem::moveFile(String::fromUTF8(m_path.c_str()), String::fromUTF8(path));
                m_path = path;
            }
            return;
        }

        m_path = path;
        m_download->setDestination(String::fromUTF8(path));
    }

    void cancel() override
    {
        if (m_finished) {
            return;
        }

        m_finished = true;
        m_download->cancel();

        if (m_client) {
            m_client->downloadDidFail(this, true);
        }

        destroyLater();
    }

private:
    void didReceiveResponse(const WebCore::ResourceResponse& response) override
    {
        if (m_finished || !m_client) {
            return;
        }

        m_client->downloadDidReceiveResponse(this, response.expectedContentLength(), response.suggestedFilename().utf8().data(), response.mimeType().utf8().data());
    }

    void didReceiveDataOfLength(int length) override
    {
        if (m_finished || !m_client || length <= 0) {
            return;
        }

        m_client->downloadDidReceiveData(this, static_cast<unsigned>(length));
    }

    void didFinish() override
    {
        if (m_finished) {
            return;
        }

        m_finished = true;

        if (m_client) {
            m_client->downloadDidFinish(this);
        }

        destroyLater();
    }

    void didFail() override
    {
        if (m_finished) {
            return;
        }

        m_finished = true;

        if (m_client) {
            m_client->downloadDidFail(this, false);
        }

        destroyLater();
    }

    void destroyLater()
    {
        m_client = nullptr;
        m_download->setListener(nullptr);
        callOnMainThread([this] {
            delete this;
        });
    }

    AplusDownloadClient* m_client;
    std::string m_url;
    std::string m_path;
    Ref<WebCore::CurlDownload> m_download;
    bool m_started { false };
    bool m_finished { false };
};

}

AplusDownload* AplusDownload::create(const char* url, AplusDownloadClient* client)
{
    return new AplusDownloadImpl(url, client);
}
