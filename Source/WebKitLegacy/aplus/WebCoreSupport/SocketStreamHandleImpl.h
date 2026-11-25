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

#include "SocketStreamHandle.h"

#include <WebCore/CurlStream.h>
#include <pal/SessionID.h>
#include <wtf/Function.h>
#include <wtf/RefPtr.h>
#include <wtf/StreamBuffer.h>

namespace WebCore {

class StorageSessionProvider;
class SocketStreamHandleClient;

class SocketStreamHandleImpl : public SocketStreamHandle, public CurlStream::Client {
public:
    static Ref<SocketStreamHandleImpl> create(const URL& url, SocketStreamHandleClient& client, PAL::SessionID, const String&, SourceApplicationAuditToken&&, const StorageSessionProvider* provider, bool shouldAcceptInsecureCertificates)
    {
        return adoptRef(*new SocketStreamHandleImpl(url, client, provider, shouldAcceptInsecureCertificates));
    }

    virtual ~SocketStreamHandleImpl();

    void platformSend(std::span<const uint8_t> data, Function<void(bool)>&&) final;
    void platformSendHandshake(std::span<const uint8_t> data, const std::optional<CookieRequestHeaderFieldProxy>&, Function<void(bool, bool)>&&) final;
    void platformClose() final;

private:
    SocketStreamHandleImpl(const URL&, SocketStreamHandleClient&, const StorageSessionProvider*, bool shouldAcceptInsecureCertificates);

    size_t bufferedAmount() final;
    std::optional<size_t> platformSendInternal(std::span<const uint8_t>);
    bool sendPendingData();

    void didOpen(CurlStreamID) final;
    void didSendData(CurlStreamID, size_t) final;
    void didReceiveData(CurlStreamID, const SharedBuffer&) final;
    void didFail(CurlStreamID, CURLcode, CertificateInfo&&) final;

    void destroyStream();

    RefPtr<const StorageSessionProvider> m_storageSessionProvider;
    CurlStreamID m_streamID { invalidCurlStreamID };
    size_t m_inflight { 0 };
    StreamBuffer<uint8_t, 1024 * 1024> m_buffer;
    static const unsigned maxBufferSize = 100 * 1024 * 1024;
};

}
