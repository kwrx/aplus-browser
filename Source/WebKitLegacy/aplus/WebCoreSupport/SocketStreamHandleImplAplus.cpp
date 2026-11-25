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
#include "SocketStreamHandleImpl.h"

#include "SocketStreamHandleClient.h"

#include <WebCore/CertificateInfo.h>
#include <WebCore/CurlContext.h>
#include <WebCore/CurlStreamScheduler.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/SocketStreamError.h>
#include <WebCore/StorageSessionProvider.h>
#include <wtf/MainThread.h>
#include <wtf/UniqueArray.h>

namespace WebCore {

SocketStreamHandleImpl::SocketStreamHandleImpl(const URL& url, SocketStreamHandleClient& client, const StorageSessionProvider* provider, bool shouldAcceptInsecureCertificates)
    : SocketStreamHandle(url, client)
    , m_storageSessionProvider(provider)
{
    ASSERT(isMainThread());

    auto trust = shouldAcceptInsecureCertificates ? CurlStream::ServerTrustEvaluation::Disable : CurlStream::ServerTrustEvaluation::Enable;
    m_streamID = CurlContext::singleton().streamScheduler().createStream(url, *this, trust, CurlStream::LocalhostAlias::Disable);
}

SocketStreamHandleImpl::~SocketStreamHandleImpl()
{
    destroyStream();
}

void SocketStreamHandleImpl::destroyStream()
{
    if (m_streamID == invalidCurlStreamID) {
        return;
    }

    CurlContext::singleton().streamScheduler().destroyStream(m_streamID);
    m_streamID = invalidCurlStreamID;
}

std::optional<size_t> SocketStreamHandleImpl::platformSendInternal(std::span<const uint8_t> data)
{
    if (m_streamID == invalidCurlStreamID) {
        return std::nullopt;
    }

    if (data.empty()) {
        return 0;
    }

    auto buffer = makeUniqueArray<uint8_t>(data.size());
    memcpy(buffer.get(), data.data(), data.size());
    m_inflight += data.size();
    CurlContext::singleton().streamScheduler().send(m_streamID, WTF::move(buffer), data.size());
    return data.size();
}

void SocketStreamHandleImpl::platformClose()
{
    destroyStream();

    if (m_state == Closed) {
        return;
    }

    m_state = Closed;
    m_inflight = 0;
    m_client.didCloseSocketStream(*this);
}

void SocketStreamHandleImpl::didOpen(CurlStreamID)
{
    if (m_state != Connecting) {
        return;
    }

    m_state = Open;
    m_client.didOpenSocketStream(*this);
    sendPendingData();
}

void SocketStreamHandleImpl::didSendData(CurlStreamID, size_t length)
{
    m_inflight = length > m_inflight ? 0 : m_inflight - length;

    if (m_state == Closing && !bufferedAmount()) {
        disconnect();
        return;
    }

    m_client.didUpdateBufferedAmount(*this, bufferedAmount());
}

void SocketStreamHandleImpl::didReceiveData(CurlStreamID, const SharedBuffer& buffer)
{
    if (m_state != Open && m_state != Closing) {
        return;
    }

    if (!buffer.size()) {
        m_client.didFailToReceiveSocketStreamData(*this);
        return;
    }

    m_client.didReceiveSocketStreamData(*this, buffer.span());
}

void SocketStreamHandleImpl::didFail(CurlStreamID, CURLcode errorCode, CertificateInfo&&)
{
    if (m_state == Closed) {
        return;
    }

    Ref protectedThis { *this };
    destroyStream();
    m_client.didFailSocketStream(*this, SocketStreamError(static_cast<int>(errorCode), m_url.string(), String::fromLatin1(curl_easy_strerror(errorCode))));
    platformClose();
}

}
