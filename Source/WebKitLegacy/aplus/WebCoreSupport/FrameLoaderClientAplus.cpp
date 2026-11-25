/*
 * Copyright (C) 2006 Don Gibson <dgibson77@gmail.com>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com> All rights reserved.
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com> All rights reserved.
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 * Copyright (C) 2010 Michael Lotz <mmlr@mlotz.ch>
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

#include "config.h"
#include "FrameLoaderClientAplus.h"

#include "AplusWebView.h"
#include "FrameNetworkingContextAplus.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/AuthenticationClient.h>
#include <WebCore/CachedFrame.h>
#include <WebCore/Credential.h>
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/FormState.h>
#include <WebCore/FrameInlines.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameTree.h>
#include <WebCore/HTMLFrameOwnerElement.h>
#include <WebCore/HistoryController.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/NavigationAction.h>
#include <WebCore/Page.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/UserAgent.h>
#include <WebCore/DocumentPage.h>
#include <WebCore/DocumentView.h>
#include <WebCore/NodeDocument.h>
#include <wtf/CompletionHandler.h>
#include <wtf/URLHelpers.h>

namespace WebCore {

enum {
    WebKitErrorFrameLoadInterruptedByPolicyChange = 102,
    WebKitErrorPlugInWillHandleLoad = 201,
};

static std::string toStd(const String& string)
{
    auto utf8 = string.utf8();
    return std::string(utf8.data(), utf8.length());
}

FrameLoaderClientAplus::FrameLoaderClientAplus(FrameLoader& loader, AplusWebView* view)
    : LocalFrameLoaderClient(loader)
    , m_frameLoader(loader)
    , m_view(view)
{
}

FrameLoaderClientAplus::~FrameLoaderClientAplus() = default;

LocalFrame& FrameLoaderClientAplus::coreFrame() const
{
    return m_frameLoader.frame();
}

bool FrameLoaderClientAplus::isMainFrame() const
{
    return coreFrame().isMainFrame();
}

bool FrameLoaderClientAplus::hasWebView() const
{
    return true;
}

void FrameLoaderClientAplus::makeRepresentation(DocumentLoader*)
{
}

void FrameLoaderClientAplus::forceLayoutForNonHTML()
{
}

void FrameLoaderClientAplus::setCopiesOnScroll()
{
}

void FrameLoaderClientAplus::detachedFromParent2()
{
}

void FrameLoaderClientAplus::detachedFromParent3()
{
}

bool FrameLoaderClientAplus::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int)
{
    return false;
}

void FrameLoaderClientAplus::assignIdentifierToInitialRequest(ResourceLoaderIdentifier, DocumentLoader*, const ResourceRequest&)
{
}

void FrameLoaderClientAplus::dispatchWillSendRequest(DocumentLoader*, ResourceLoaderIdentifier, ResourceRequest&, const ResourceResponse&)
{
}

bool FrameLoaderClientAplus::shouldUseCredentialStorage(DocumentLoader*, ResourceLoaderIdentifier)
{
    return !m_view->privateBrowsing();
}

void FrameLoaderClientAplus::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, ResourceLoaderIdentifier, const AuthenticationChallenge& challenge)
{
    const ProtectionSpace& space = challenge.protectionSpace();
    std::string user = toStd(challenge.proposedCredential().user());
    std::string password = toStd(challenge.proposedCredential().password());
    bool ok = m_view->client()->authenticate(toStd(space.host()).c_str(), toStd(space.realm()).c_str(), challenge.previousFailureCount() > 0, user, password);
    if (!ok) {
        challenge.authenticationClient()->receivedCancellation(challenge);
        return;
    }
    if (user.empty() && password.empty()) {
        challenge.authenticationClient()->receivedRequestToContinueWithoutCredential(challenge);
        return;
    }
    Credential credential(String::fromUTF8(user.c_str()), String::fromUTF8(password.c_str()), m_view->privateBrowsing() ? CredentialPersistence::None : CredentialPersistence::ForSession);
    challenge.authenticationClient()->receivedCredential(challenge, credential);
}

bool FrameLoaderClientAplus::dispatchDidReceiveInvalidCertificate(DocumentLoader* loader, const CertificateInfo&, const char* message)
{
    return m_view->client()->acceptCertificate(toStd(loader->url().string()).c_str(), message);
}

void FrameLoaderClientAplus::dispatchDidReceiveResponse(DocumentLoader* loader, ResourceLoaderIdentifier, const ResourceResponse& response)
{
    loader->writer().setEncoding(response.textEncodingName(), DocumentWriter::IsEncodingUserChosen::No);
}

void FrameLoaderClientAplus::dispatchDidReceiveContentLength(DocumentLoader*, ResourceLoaderIdentifier, int)
{
}

void FrameLoaderClientAplus::dispatchDidFinishLoading(DocumentLoader*, ResourceLoaderIdentifier)
{
}

void FrameLoaderClientAplus::dispatchDidFailLoading(DocumentLoader*, ResourceLoaderIdentifier, const ResourceError&)
{
}

void FrameLoaderClientAplus::dispatchDidDispatchOnloadEvents()
{
}

void FrameLoaderClientAplus::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
}

void FrameLoaderClientAplus::dispatchDidCancelClientRedirect()
{
}

void FrameLoaderClientAplus::dispatchWillPerformClientRedirect(const URL&, double, WallTime, LockBackForwardList)
{
}

void FrameLoaderClientAplus::dispatchDidChangeLocationWithinPage()
{
    if (!isMainFrame() || !coreFrame().document()) {
        return;
    }
    m_view->didChangeURL(toStd(coreFrame().document()->url().string()));
}

void FrameLoaderClientAplus::dispatchDidPushStateWithinPage()
{
    dispatchDidChangeLocationWithinPage();
}

void FrameLoaderClientAplus::dispatchDidReplaceStateWithinPage()
{
    dispatchDidChangeLocationWithinPage();
}

void FrameLoaderClientAplus::dispatchDidPopStateWithinPage()
{
    dispatchDidChangeLocationWithinPage();
}

void FrameLoaderClientAplus::dispatchWillClose()
{
}

void FrameLoaderClientAplus::dispatchDidReceiveIcon()
{
}

void FrameLoaderClientAplus::dispatchDidStartProvisionalLoad()
{
    m_loadingErrorPage = false;
    if (!isMainFrame()) {
        return;
    }
    auto* loader = coreFrame().loader().provisionalDocumentLoader();
    m_view->didStartLoad(loader ? toStd(loader->request().url().string()) : std::string());
}

void FrameLoaderClientAplus::dispatchDidReceiveTitle(const StringWithDirection& title)
{
    if (m_loadingErrorPage || !isMainFrame()) {
        return;
    }
    m_view->didChangeTitle(toStd(title.string));
}

void FrameLoaderClientAplus::dispatchDidCommitLoad(std::optional<HasInsecureContent>, std::optional<UsedLegacyTLS>, std::optional<WasPrivateRelayed>)
{
    if (m_loadingErrorPage || !isMainFrame()) {
        return;
    }
    URL url = coreFrame().loader().documentLoader()->request().url();
    String visible = WTF::URLHelpers::userVisibleURL(url.string().utf8());
    m_view->didCommitLoad(toStd(visible.isEmpty() ? url.string() : visible));
    m_view->didChangeTitle(std::string());
}

void FrameLoaderClientAplus::dispatchDidFailProvisionalLoad(const ResourceError& error, WillContinueLoading, WillInternallyHandleFailure)
{
    dispatchDidFailLoad(error);
}

void FrameLoaderClientAplus::dispatchDidFailLoad(const ResourceError& error)
{
    if (m_loadingErrorPage || !isMainFrame()) {
        return;
    }
    bool fallBack = shouldFallBack(error);
    if (fallBack) {
        m_loadingErrorPage = true;
    }
    m_view->didFailLoad(toStd(error.failingURL().string()), toStd(error.localizedDescription()), !fallBack);
}

void FrameLoaderClientAplus::dispatchDidFinishDocumentLoad()
{
}

void FrameLoaderClientAplus::dispatchDidFinishLoad()
{
    if (m_loadingErrorPage) {
        m_loadingErrorPage = false;
        return;
    }
    if (isMainFrame()) {
        m_view->didFinishLoad();
    }
}

void FrameLoaderClientAplus::dispatchWillSubmitForm(FormState&, URL&&, String&&, CompletionHandler<void()>&& function)
{
    function();
}

LocalFrame* FrameLoaderClientAplus::dispatchCreatePage(const NavigationAction&, NewFrameOpenerPolicy, const String&)
{
    AplusWebView* view = m_view->client()->createView(nullptr, false);
    if (!view) {
        return nullptr;
    }
    return view->mainFrame();
}

void FrameLoaderClientAplus::dispatchShow()
{
}

void FrameLoaderClientAplus::dispatchDecidePolicyForResponse(const ResourceResponse& response, const ResourceRequest& request, const String&, FramePolicyFunction&& function)
{
    if (request.isNull()) {
        function(PolicyAction::Ignore);
        return;
    }
    if (!response.isAttachment() && canShowMIMEType(response.mimeType())) {
        function(PolicyAction::Use);
    } else if (!request.url().protocolIsFile()) {
        function(PolicyAction::Download);
    } else {
        function(PolicyAction::Ignore);
    }
}

static bool isHandledScheme(const URL& url)
{
    return url.protocolIsInHTTPFamily() || url.protocolIsFile() || url.protocolIsAbout() || url.protocolIsData() || url.protocolIsBlob() || url.protocolIsJavaScript() || url.protocolIs("ftp"_s) || url.protocolIs("view-source"_s);
}

bool FrameLoaderClientAplus::openInNewView(const NavigationAction& action, const ResourceRequest& request)
{
    if (!action.mouseEventData().has_value() || action.mouseEventData()->button != MouseButton::Middle) {
        return false;
    }
    bool background = !action.mouseEventData()->shiftKey;
    m_view->client()->createView(toStd(request.url().string()).c_str(), background);
    if (action.type() == NavigationType::FormSubmitted || action.type() == NavigationType::FormResubmitted) {
        coreFrame().loader().resetMultipleFormSubmissionProtection();
    }
    if (action.type() == NavigationType::LinkClicked) {
        if (auto* loader = coreFrame().loader().activeDocumentLoader()) {
            loader->setLastCheckedRequest(ResourceRequest());
        }
    }
    return true;
}

void FrameLoaderClientAplus::dispatchDecidePolicyForNewWindowAction(const NavigationAction& action, const ResourceRequest& request, FormState*, const String&, std::optional<HitTestResult>&&, FramePolicyFunction&& function)
{
    if (request.isNull()) {
        function(PolicyAction::Ignore);
        return;
    }
    if (!isHandledScheme(request.url())) {
        m_view->client()->openExternalURL(toStd(request.url().string()).c_str());
        function(PolicyAction::Ignore);
        return;
    }
    if (openInNewView(action, request)) {
        function(PolicyAction::Ignore);
        return;
    }
    function(PolicyAction::Use);
}

void FrameLoaderClientAplus::dispatchDecidePolicyForNavigationAction(const NavigationAction& action, const ResourceRequest& request, const ResourceResponse&, FormState* formState, const String&, std::optional<NavigationIdentifier>, std::optional<HitTestResult>&& hit, bool, NavigationUpgradeToHTTPSBehavior, SandboxFlags, PolicyDecisionMode, FramePolicyFunction&& function)
{
    dispatchDecidePolicyForNewWindowAction(action, request, formState, String(), WTF::move(hit), WTF::move(function));
}

void FrameLoaderClientAplus::cancelPolicyCheck()
{
}

void FrameLoaderClientAplus::dispatchUnableToImplementPolicy(const ResourceError&)
{
}

void FrameLoaderClientAplus::revertToProvisionalState(DocumentLoader*)
{
}

void FrameLoaderClientAplus::setMainDocumentError(DocumentLoader*, const ResourceError&)
{
}

void FrameLoaderClientAplus::setMainFrameDocumentReady(bool)
{
}

void FrameLoaderClientAplus::startDownload(const ResourceRequest& request, const String& suggestedName, FromDownloadAttribute)
{
    m_view->client()->downloadRequested(toStd(request.url().string()).c_str(), suggestedName.isEmpty() ? nullptr : toStd(suggestedName).c_str());
}

void FrameLoaderClientAplus::willChangeTitle(DocumentLoader*)
{
}

void FrameLoaderClientAplus::didChangeTitle(DocumentLoader*)
{
}

void FrameLoaderClientAplus::committedLoad(DocumentLoader* loader, const SharedBuffer& buffer)
{
    loader->commitData(buffer);
}

void FrameLoaderClientAplus::finishedLoading(DocumentLoader*)
{
}

void FrameLoaderClientAplus::updateGlobalHistory()
{
    if (!isMainFrame() || m_view->privateBrowsing()) {
        return;
    }
    auto* loader = coreFrame().loader().documentLoader();
    if (!loader) {
        return;
    }
    m_view->client()->historyVisited(toStd(loader->urlForHistory().string()).c_str(), toStd(loader->title().string).c_str());
}

void FrameLoaderClientAplus::updateGlobalHistoryRedirectLinks()
{
    updateGlobalHistory();
}

void FrameLoaderClientAplus::updateOpener(std::optional<FrameIdentifier>)
{
}

ShouldGoToHistoryItem FrameLoaderClientAplus::shouldGoToHistoryItem(HistoryItem&, IsSameDocumentNavigation) const
{
    return ShouldGoToHistoryItem::Yes;
}

void FrameLoaderClientAplus::shouldGoToHistoryItemAsync(HistoryItem&, CompletionHandler<void(ShouldGoToHistoryItem)>&& handler) const
{
    handler(ShouldGoToHistoryItem::Yes);
}

RefPtr<HistoryItem> FrameLoaderClientAplus::createHistoryItemTree(bool clipAtTarget, BackForwardItemIdentifier identifier) const
{
    LocalFrame& frame = coreFrame();
    return frame.loader().history().createItemTree(frame, clipAtTarget, identifier);
}

void FrameLoaderClientAplus::convertMainResourceLoadToDownload(DocumentLoader*, const ResourceRequest& request, const ResourceResponse& response)
{
    startDownload(request, response.suggestedFilename(), FromDownloadAttribute::No);
}

bool FrameLoaderClientAplus::shouldFallBack(const ResourceError& error) const
{
    return !(error.isCancellation() || error.errorCode() == WebKitErrorFrameLoadInterruptedByPolicyChange || error.errorCode() == WebKitErrorPlugInWillHandleLoad);
}

bool FrameLoaderClientAplus::canHandleRequest(const ResourceRequest&) const
{
    return true;
}

bool FrameLoaderClientAplus::canShowMIMETypeAsHTML(const String&) const
{
    return false;
}

bool FrameLoaderClientAplus::canShowMIMEType(const String& mimeType) const
{
    if (mimeType.isEmpty()) {
        return true;
    }
    return MIMETypeRegistry::canShowMIMEType(mimeType);
}

bool FrameLoaderClientAplus::representationExistsForURLScheme(StringView) const
{
    return false;
}

String FrameLoaderClientAplus::generatedMIMETypeForURLScheme(StringView) const
{
    return String();
}

void FrameLoaderClientAplus::frameLoadCompleted()
{
}

void FrameLoaderClientAplus::saveViewStateToItem(HistoryItem&)
{
}

void FrameLoaderClientAplus::restoreViewState()
{
}

void FrameLoaderClientAplus::provisionalLoadStarted()
{
}

void FrameLoaderClientAplus::didFinishLoad()
{
}

void FrameLoaderClientAplus::prepareForDataSourceReplacement()
{
}

Ref<DocumentLoader> FrameLoaderClientAplus::createDocumentLoader(ResourceRequest&& request, SubstituteData&& substituteData)
{
    return DocumentLoader::create(WTF::move(request), WTF::move(substituteData));
}

Ref<DocumentLoader> FrameLoaderClientAplus::createDocumentLoader(ResourceRequest&& request, SubstituteData&& substituteData, ResourceRequest&&)
{
    return DocumentLoader::create(WTF::move(request), WTF::move(substituteData));
}

void FrameLoaderClientAplus::setTitle(const StringWithDirection&, const URL&)
{
}

void FrameLoaderClientAplus::savePlatformDataToCachedFrame(CachedFrame*)
{
}

void FrameLoaderClientAplus::transitionToCommittedFromCachedFrame(CachedFrame*)
{
}

void FrameLoaderClientAplus::transitionToCommittedForNewPage(InitializingIframe)
{
    LocalFrame& frame = coreFrame();
    std::optional<Color> backgroundColor;
    if (m_view->transparent()) {
        backgroundColor = Color(Color::transparentBlack);
    } else if (frame.isMainFrame()) {
        RefPtr loader = frame.loader().documentLoader();
        URL url = loader ? loader->url() : URL();
        if (url.isEmpty() || url.protocolIsAbout()) {
            uint32_t color = m_view->placeholderColor();
            backgroundColor = Color(SRGBA<uint8_t> { static_cast<uint8_t>(color >> 16), static_cast<uint8_t>(color >> 8), static_cast<uint8_t>(color) });
        }
    }
    frame.createView(IntSize(std::max(m_view->width(), 1), std::max(m_view->height(), 1)), backgroundColor, { }, { });
}

String FrameLoaderClientAplus::userAgent(const URL&) const
{
    if (!m_view->customUserAgent().empty()) {
        return String::fromUTF8(m_view->customUserAgent().c_str());
    }
    if (const char* agent = m_view->client()->userAgent()) {
        return String::fromUTF8(agent);
    }
    return standardUserAgent(String(), String());
}

bool FrameLoaderClientAplus::canCachePage() const
{
    return true;
}

RefPtr<LocalFrame> FrameLoaderClientAplus::createFrame(const AtomString& name, HTMLFrameOwnerElement& ownerElement)
{
    RefPtr page = coreFrame().page();
    if (!page) {
        return nullptr;
    }
    AplusWebView* view = m_view;
    Ref<LocalFrame> subframe = LocalFrame::createSubframe(*page,
        CompletionHandler<UniqueRef<LocalFrameLoaderClient>(LocalFrame&, FrameLoader&)> {
            [view](auto&, auto& frameLoader) { return makeUniqueRefWithoutRefCountedCheck<FrameLoaderClientAplus>(frameLoader, view); } },
        FrameIdentifier::generate(), { }, { }, ownerElement, FrameTreeSyncData::create());
    subframe->tree().setSpecifiedName(name);
    subframe->init();
    if (!subframe->page()) {
        return nullptr;
    }
    return subframe;
}

ObjectContentType FrameLoaderClientAplus::objectContentType(const URL& url, const String& originalMimeType)
{
    if (url.isEmpty() && originalMimeType.isEmpty()) {
        return ObjectContentType::None;
    }
    String mimeType = originalMimeType;
    if (mimeType.isEmpty()) {
        String path = url.path().toString();
        size_t dot = path.reverseFind('.');
        if (dot != notFound) {
            mimeType = MIMETypeRegistry::mimeTypeForExtension(StringView(path).substring(dot + 1));
        }
    }
    if (mimeType.isEmpty()) {
        return ObjectContentType::Frame;
    }
    if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType)) {
        return ObjectContentType::Image;
    }
    if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType)) {
        return ObjectContentType::Frame;
    }
    if (url.protocolIsAbout()) {
        return ObjectContentType::Frame;
    }
    return ObjectContentType::None;
}

RefPtr<Widget> FrameLoaderClientAplus::createPlugin(HTMLPlugInElement&, const URL&, const Vector<AtomString>&, const Vector<AtomString>&, const String&, bool)
{
    return nullptr;
}

void FrameLoaderClientAplus::redirectDataToPlugin(Widget&)
{
}

AtomString FrameLoaderClientAplus::overrideMediaType() const
{
    return nullAtom();
}

void FrameLoaderClientAplus::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld&)
{
}

void FrameLoaderClientAplus::dispatchLoadEventToOwnerElementInAnotherProcess()
{
}

Ref<FrameNetworkingContext> FrameLoaderClientAplus::createNetworkingContext()
{
    return FrameNetworkingContextAplus::create(&coreFrame());
}

void FrameLoaderClientAplus::sendH2Ping(const URL& url, CompletionHandler<void(Expected<Seconds, ResourceError>&&)>&& completionHandler)
{
    completionHandler(makeUnexpected(ResourceError(String(), 0, url, "H2 ping not supported"_s)));
}

}
