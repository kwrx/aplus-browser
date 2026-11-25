/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com> All rights reserved.
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com> All rights reserved.
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
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

#pragma once

#include <WebCore/FrameLoader.h>
#include <WebCore/LocalFrameLoaderClient.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceError.h>
#include <wtf/Forward.h>
#include <wtf/URL.h>

class AplusWebView;

namespace WebCore {

class AuthenticationChallenge;
class DocumentLoader;
class Element;
class FormState;
class HistoryItem;
class NavigationAction;
class ResourceLoader;
class ResourceResponse;

class FrameLoaderClientAplus final : public LocalFrameLoaderClient {
public:
    FrameLoaderClientAplus(FrameLoader&, AplusWebView*);
    ~FrameLoaderClientAplus();

    LocalFrame& coreFrame() const;
    bool isMainFrame() const;
    AplusWebView* view() const { return m_view; }

    bool hasWebView() const override;

    void makeRepresentation(DocumentLoader*) override;
    void forceLayoutForNonHTML() override;
    void setCopiesOnScroll() override;
    void detachedFromParent2() override;
    void detachedFromParent3() override;

    void assignIdentifierToInitialRequest(ResourceLoaderIdentifier, DocumentLoader*, const ResourceRequest&) override;
    void dispatchWillSendRequest(DocumentLoader*, ResourceLoaderIdentifier, ResourceRequest&, const ResourceResponse&) override;
    bool shouldUseCredentialStorage(DocumentLoader*, ResourceLoaderIdentifier) override;
    void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, ResourceLoaderIdentifier, const AuthenticationChallenge&) override;
    bool dispatchDidReceiveInvalidCertificate(DocumentLoader*, const CertificateInfo&, const char* message) override;
    void dispatchDidCommitLoad(std::optional<HasInsecureContent>, std::optional<UsedLegacyTLS>, std::optional<WasPrivateRelayed>) override;
    void dispatchDidReceiveResponse(DocumentLoader*, ResourceLoaderIdentifier, const ResourceResponse&) override;
    void dispatchDidReceiveContentLength(DocumentLoader*, ResourceLoaderIdentifier, int) override;
    void dispatchDidFinishLoading(DocumentLoader*, ResourceLoaderIdentifier) override;
    void dispatchDidFailLoading(DocumentLoader*, ResourceLoaderIdentifier, const ResourceError&) override;

    void dispatchDidDispatchOnloadEvents() override;
    void dispatchDidReceiveServerRedirectForProvisionalLoad() override;
    void dispatchDidCancelClientRedirect() override;
    void dispatchWillPerformClientRedirect(const URL&, double, WallTime, LockBackForwardList) override;
    void dispatchDidChangeLocationWithinPage() override;
    void dispatchDidPushStateWithinPage() override;
    void dispatchDidReplaceStateWithinPage() override;
    void dispatchDidPopStateWithinPage() override;
    void dispatchWillClose() override;
    void dispatchDidReceiveIcon() override;
    void dispatchDidStartProvisionalLoad() override;
    void dispatchDidReceiveTitle(const StringWithDirection&) override;
    void dispatchDidFailProvisionalLoad(const ResourceError&, WillContinueLoading, WillInternallyHandleFailure) override;
    void dispatchDidFailLoad(const ResourceError&) override;
    void dispatchDidFinishDocumentLoad() override;
    void dispatchDidFinishLoad() override;
    void dispatchGoToBackForwardItemAtIndex(int) override { }
    void dispatchEnqueueHistoryTraversalDelta(int) override { }

    LocalFrame* dispatchCreatePage(const NavigationAction&, NewFrameOpenerPolicy, const String&) override;
    void dispatchShow() override;

    void dispatchDecidePolicyForResponse(const ResourceResponse&, const ResourceRequest&, const String& downloadAttribute, FramePolicyFunction&&) override;
    void dispatchDecidePolicyForNewWindowAction(const NavigationAction&, const ResourceRequest&, FormState*, const String& formName, std::optional<HitTestResult>&&, FramePolicyFunction&&) override;
    void dispatchDecidePolicyForNavigationAction(const NavigationAction&, const ResourceRequest&, const ResourceResponse&, FormState*, const String& clientRedirectSourceForHistory, std::optional<NavigationIdentifier>, std::optional<HitTestResult>&&, bool hasOpener, NavigationUpgradeToHTTPSBehavior, SandboxFlags, PolicyDecisionMode, FramePolicyFunction&&) override;
    void cancelPolicyCheck() override;
    void updateSandboxFlags(SandboxFlags) override { }

    void dispatchUnableToImplementPolicy(const ResourceError&) override;
    void dispatchWillSendSubmitEvent(Ref<FormState>&&) override { }
    void dispatchWillSubmitForm(FormState&, URL&&, String&&, CompletionHandler<void()>&&) override;

    void revertToProvisionalState(DocumentLoader*) override;
    void setMainDocumentError(DocumentLoader*, const ResourceError&) override;
    bool dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int) override;

    void setMainFrameDocumentReady(bool) override;
    void startDownload(const ResourceRequest&, const String& suggestedName, FromDownloadAttribute) override;

    void willChangeTitle(DocumentLoader*) override;
    void didChangeTitle(DocumentLoader*) override;
    void willReplaceMultipartContent() override { }
    void didReplaceMultipartContent() override { }

    void committedLoad(DocumentLoader*, const SharedBuffer&) override;
    void finishedLoading(DocumentLoader*) override;

    void updateGlobalHistory() override;
    void updateGlobalHistoryRedirectLinks() override;
    void updateOpener(std::optional<FrameIdentifier>) override;
    ShouldGoToHistoryItem shouldGoToHistoryItem(HistoryItem&, IsSameDocumentNavigation) const override;
    bool supportsAsyncShouldGoToHistoryItem() const override { return false; }
    void shouldGoToHistoryItemAsync(HistoryItem&, CompletionHandler<void(ShouldGoToHistoryItem)>&&) const override;
    RefPtr<HistoryItem> createHistoryItemTree(bool clipAtTarget, BackForwardItemIdentifier) const override;

    bool canCachePage() const override;
    void convertMainResourceLoadToDownload(DocumentLoader*, const ResourceRequest&, const ResourceResponse&) override;
    bool shouldFallBack(const ResourceError&) const override;
    String userAgent(const URL&) const override;

    void savePlatformDataToCachedFrame(CachedFrame*) override;
    void transitionToCommittedFromCachedFrame(CachedFrame*) override;
    void transitionToCommittedForNewPage(InitializingIframe) override;

    bool canHandleRequest(const ResourceRequest&) const override;
    bool canShowMIMEType(const String&) const override;
    bool canShowMIMETypeAsHTML(const String&) const override;
    bool representationExistsForURLScheme(StringView) const override;
    String generatedMIMETypeForURLScheme(StringView) const override;

    void frameLoadCompleted() override;
    void saveViewStateToItem(HistoryItem&) override;
    void restoreViewState() override;
    void provisionalLoadStarted() override;
    void didFinishLoad() override;
    void prepareForDataSourceReplacement() override;

    Ref<DocumentLoader> createDocumentLoader(ResourceRequest&&, SubstituteData&&) override;
    Ref<DocumentLoader> createDocumentLoader(ResourceRequest&&, SubstituteData&&, ResourceRequest&&) override;
    void setTitle(const StringWithDirection&, const URL&) override;

    RefPtr<LocalFrame> createFrame(const AtomString& name, HTMLFrameOwnerElement&) override;
    RefPtr<Widget> createPlugin(HTMLPlugInElement&, const URL&, const Vector<AtomString>&, const Vector<AtomString>&, const String&, bool) override;
    void redirectDataToPlugin(Widget&) override;
    ObjectContentType objectContentType(const URL&, const String& mimeType) override;
    AtomString overrideMediaType() const override;

    void dispatchDidClearWindowObjectInWorld(DOMWrapperWorld&) override;
    void dispatchLoadEventToOwnerElementInAnotherProcess() override;
    Ref<FrameNetworkingContext> createNetworkingContext() override;

    void updateCachedDocumentLoader(DocumentLoader&) override { }
    void prefetchDNS(const String&) override { }
    void setPrinting(bool, FloatSize, FloatSize, float, AdjustViewSize) override { }
    void didRestoreFromBackForwardCache() final { }
    void sendH2Ping(const URL&, CompletionHandler<void(Expected<Seconds, ResourceError>&&)>&&) final;

private:
    void loadStorageAccessQuirksIfNeeded() final { }
    bool openInNewView(const NavigationAction&, const ResourceRequest&);

    FrameLoader& m_frameLoader;
    AplusWebView* m_view;
    bool m_loadingErrorPage { false };
};

}
