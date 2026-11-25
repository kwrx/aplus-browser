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
#include "WebResourceLoadScheduler.h"

#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>

using namespace WebCore;

static constexpr auto webKitErrorDomain = "WebKitErrorDomain"_s;
static constexpr auto urlErrorDomain = "NSURLErrorDomain"_s;

enum {
    errorCancelled = -999,
    errorFileDoesNotExist = -1100,
    errorCannotShowMIMEType = 100,
    errorCannotShowURL = 101,
    errorFrameLoadInterruptedByPolicyChange = 102,
    errorCannotUseRestrictedPort = 103,
    errorFrameLoadBlockedByContentBlocker = 104,
    errorFrameLoadBlockedByContentFilter = 105,
    errorHTTPSUpgradeRedirectLoop = 106,
    errorHTTPNavigationWithHTTPSOnly = 107,
    errorPlugInWillHandleLoad = 204,
};

ResourceError WebResourceLoadScheduler::pluginWillHandleLoadErrorFromResponse(const ResourceResponse& response)
{
    return ResourceError(webKitErrorDomain, errorPlugInWillHandleLoad, response.url(), "Plug-in handled load"_s);
}

ResourceError WebResourceLoadScheduler::pluginWillHandleLoadError(const ResourceResponse& response) const
{
    return pluginWillHandleLoadErrorFromResponse(response);
}

ResourceError WebResourceLoadScheduler::cancelledError(const ResourceRequest& request) const
{
    return ResourceError(urlErrorDomain, errorCancelled, request.url(), "Load cancelled"_s, ResourceError::Type::Cancellation);
}

ResourceError WebResourceLoadScheduler::blockedErrorFromRequest(const ResourceRequest& request)
{
    return ResourceError(webKitErrorDomain, errorCannotUseRestrictedPort, request.url(), "Not allowed to use restricted network port"_s);
}

ResourceError WebResourceLoadScheduler::blockedError(const ResourceRequest& request) const
{
    return blockedErrorFromRequest(request);
}

bool WebResourceLoadScheduler::isBlockedError(const ResourceError& error) const
{
    return error.domain() == webKitErrorDomain && error.errorCode() == errorCannotUseRestrictedPort;
}

ResourceError WebResourceLoadScheduler::blockedByContentBlockerError(const ResourceRequest& request) const
{
    return ResourceError(webKitErrorDomain, errorFrameLoadBlockedByContentBlocker, request.url(), "The URL was blocked by a content blocker"_s);
}

ResourceError WebResourceLoadScheduler::cannotShowURLError(const ResourceRequest& request) const
{
    return ResourceError(webKitErrorDomain, errorCannotShowURL, request.url(), "The URL can't be shown"_s);
}

ResourceError WebResourceLoadScheduler::interruptedForPolicyChangeError(const ResourceRequest& request) const
{
    return ResourceError(webKitErrorDomain, errorFrameLoadInterruptedByPolicyChange, request.url(), "Frame load was interrupted"_s, ResourceError::Type::Cancellation);
}

#if ENABLE(CONTENT_FILTERING)
ResourceError WebResourceLoadScheduler::blockedByContentFilterError(const ResourceRequest& request) const
{
    return ResourceError(webKitErrorDomain, errorFrameLoadBlockedByContentFilter, request.url(), "The URL was blocked by a content filter"_s);
}
#endif

ResourceError WebResourceLoadScheduler::cannotShowMIMETypeError(const ResourceResponse& response) const
{
    return ResourceError(webKitErrorDomain, errorCannotShowMIMEType, response.url(), "The content type can't be shown"_s);
}

ResourceError WebResourceLoadScheduler::fileDoesNotExistError(const ResourceResponse& response) const
{
    return ResourceError(urlErrorDomain, errorFileDoesNotExist, response.url(), "The file does not exist"_s);
}

ResourceError WebResourceLoadScheduler::httpsUpgradeRedirectLoopError(const ResourceRequest& request) const
{
    return ResourceError(webKitErrorDomain, errorHTTPSUpgradeRedirectLoop, request.url(), "HTTPS upgrade redirect loop detected"_s);
}

ResourceError WebResourceLoadScheduler::httpNavigationWithHTTPSOnlyError(const ResourceRequest& request) const
{
    return ResourceError(webKitErrorDomain, errorHTTPNavigationWithHTTPSOnly, request.url(), "Navigation failed because the request was for an HTTP URL with HTTPS-Only enabled"_s);
}

bool WebResourceLoadScheduler::isHttpNavigationWithHTTPSOnlyError(const ResourceError& error) const
{
    return error.domain() == webKitErrorDomain && error.errorCode() == errorHTTPNavigationWithHTTPSOnly;
}
