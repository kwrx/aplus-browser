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

#include "BackForwardList.h"
#include "ChromeClientAplus.h"
#include "ContextMenuClientAplus.h"
#include "DragClientAplus.h"
#include "EditorClientAplus.h"
#include "FrameLoaderClientAplus.h"
#include "InspectorClientAplus.h"
#include "LegacyHistoryItemClient.h"
#include "LegacySocketProvider.h"
#include "PageStorageSessionProvider.h"
#include "PlatformStrategiesAplus.h"
#include "PopupMenuAplus.h"
#include "ProgressTrackerAplus.h"
#include "WebBroadcastChannelRegistry.h"
#include "../WebCoreSupport/WebCryptoClient.h"
#include "WebDatabaseProvider.h"
#include "WebDiagnosticLoggingClient.h"
#include "WebStorageNamespaceProvider.h"
#include "WebViewGroup.h"
#include "WebVisitedLinkStore.h"
#include "NetworkStorageSessionMap.h"

#include <WebCore/BackForwardController.h>
#include <WebCore/CacheStorageProvider.h>
#include <WebCore/ContextMenu.h>
#include <WebCore/ContextMenuController.h>
#include <WebCore/CookieJar.h>
#include <WebCore/CookieJarDB.h>
#include <WebCore/CurlCacheManager.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentFullscreen.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentSyncClient.h>
#include <WebCore/DummyModelPlayerProvider.h>
#include <WebCore/DummySpeechRecognitionProvider.h>
#include <WebCore/DummyStorageProvider.h>
#include <WebCore/DummyWebRTCProvider.h>
#include <WebCore/Editor.h>
#include <WebCore/EmptyBadgeClient.h>
#include <WebCore/EventHandler.h>
#include <WebCore/FindOptions.h>
#include <WebCore/FocusController.h>
#include <WebCore/FontCache.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/GraphicsContextCairo.h>
#include <WebCore/HandleUserInputEventResult.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/MediaPlayerAplus.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/LogInitialization.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/MemoryRelease.h>
#include <WebCore/Page.h>
#include <WebCore/PageConfiguration.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/PlatformWheelEvent.h>
#include <WebCore/PluginInfoProvider.h>
#include <WebCore/ProgressTracker.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/ScriptController.h>
#include <WebCore/Settings.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/SubstituteData.h>
#include <WebCore/ScrollingCoordinatorTypes.h>
#include <WebCore/ThreadGlobalData.h>
#include <WebCore/UserContentController.h>
#include <WebCore/WindowsKeyboardCodes.h>
#include <WebCore/markup.h>
#include <pal/text/TextEncoding.h>
#include <wtf/Language.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/Threading.h>
#include <wtf/threads/BinarySemaphore.h>
#include <wtf/text/MakeString.h>

#if ENABLE(MHTML)
#include <WebCore/MHTMLArchive.h>
#include <WebCore/DocumentPage.h>
#include <WebCore/DocumentView.h>
#include <WebCore/NodeDocument.h>
#endif

#include <atomic>
#include <errno.h>
#include <map>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace WebCore;

void WebKitInitializeLogChannelsIfNecessary();

namespace {

constexpr double zoomRatio = 1.1;
constexpr double zoomMinimum = 0.5;
constexpr double zoomMaximum = 3;

class EmptyPluginInfoProvider final : public PluginInfoProvider {
    void refreshPlugins() final { }
    Vector<PluginInfo> pluginInfo(Page&, std::optional<Vector<SupportedPluginIdentifier>>&) final { return { }; }
    Vector<PluginInfo> webVisiblePluginInfo(Page&, const URL&) final { return { }; }
};

struct FdWatch {
    int fd { -1 };
    int wake[2] { -1, -1 };
    std::function<void()> handler;
    BinarySemaphore done;
    std::atomic<bool> stopped { false };
};

std::map<int, std::shared_ptr<FdWatch>>& fdWatches()
{
    static NeverDestroyed<std::map<int, std::shared_ptr<FdWatch>>> watches;
    return watches.get();
}

std::string toStdString(const String& string)
{
    if (string.isNull()) {
        return { };
    }
    auto utf8 = string.utf8();
    return std::string(utf8.data(), utf8.length());
}

std::vector<uint8_t> toStdVector(const Vector<uint8_t>& data)
{
    return std::vector<uint8_t>(data.begin(), data.end());
}

AplusHitTest toAplusHitTest(const HitTestResult& result)
{
    AplusHitTest hit;
    hit.linkURL = toStdString(result.absoluteLinkURL().string());
    hit.linkTitle = toStdString(result.titleDisplayString());
    hit.imageURL = toStdString(result.absoluteImageURL().string());
    hit.mediaURL = toStdString(result.absoluteMediaURL().string());
    hit.selectedText = toStdString(result.selectedText());
    hit.mediaIsVideo = result.mediaIsVideo();
    hit.editable = result.isContentEditable();
    hit.selected = result.isSelected();
    return hit;
}

int windowsKeyCode(const AplusKeyEvent& event)
{
    return PlatformKeyboardEvent(event).windowsVirtualKeyCode();
}

ASCIILiteral editCommandName(AplusEditCommand command)
{
    switch (command) {
    case AplusEditCommand::Cut:
        return "Cut"_s;
    case AplusEditCommand::Copy:
        return "Copy"_s;
    case AplusEditCommand::Paste:
        return "Paste"_s;
    case AplusEditCommand::Undo:
        return "Undo"_s;
    case AplusEditCommand::Redo:
        return "Redo"_s;
    case AplusEditCommand::SelectAll:
        return "SelectAll"_s;
    case AplusEditCommand::Unselect:
        return "Unselect"_s;
    }
    return "SelectAll"_s;
}

FindOptions toFindOptions(unsigned options)
{
    FindOptions result;
    if (options & APLUS_FIND_BACKWARDS) {
        result.add(FindOption::Backwards);
    }
    if (!(options & APLUS_FIND_CASE_SENSITIVE)) {
        result.add(FindOption::CaseInsensitive);
    }
    if (options & APLUS_FIND_WRAP) {
        result.add(FindOption::WrapAround);
    }
    if (options & APLUS_FIND_START_IN_SELECTION) {
        result.add(FindOption::StartInSelection);
    }
    return result;
}

std::string trim(const char* text)
{
    std::string string(text ? text : "");
    size_t begin = string.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return { };
    }
    size_t end = string.find_last_not_of(" \t\r\n");
    return string.substr(begin, end - begin + 1);
}

}

void AplusWebView::initialize()
{
    static bool initialized;
    if (initialized) {
        return;
    }
    initialized = true;

#if !LOG_DISABLED
    WebKitInitializeLogChannelsIfNecessary();
#endif
    PlatformStrategiesAplus::initialize();
    WTF::initializeMainThread();
    ScriptController::initializeMainThread();
    PAL::UTF8Encoding();
    WebVisitedLinkStore::setShouldTrackVisitedLinks(true);
    NetworkStorageSessionMap::defaultStorageSession().setCookieDatabase(makeUniqueRef<CookieJarDB>(makeString(String::fromUTF8(aplusConfigDir()), "/cookies.db"_s)));
#if ENABLE(VIDEO)
    MediaPlayerAplusSettings::settings().m_storageSession = [] {
        return &NetworkStorageSessionMap::defaultStorageSession();
    };
#endif
}

void AplusWebView::setCacheModel(size_t memoryCapacity, size_t diskCapacity)
{
    auto& cache = MemoryCache::singleton();
    if (!memoryCapacity) {
        cache.setCapacities(0, 0, 0);
        cache.setDeadDecodedDataDeletionInterval(0_s);
    } else {
        unsigned total = static_cast<unsigned>(std::min<size_t>(memoryCapacity, std::numeric_limits<unsigned>::max()));
        cache.setCapacities(total / 4, total / 2, total);
        cache.setDeadDecodedDataDeletionInterval(60_s);
    }

    if (diskCapacity) {
        CurlCacheManager::singleton().setCacheDirectory(makeString(String::fromUTF8(aplusConfigDir()), "/Cache"_s));
        CurlCacheManager::singleton().setStorageSizeLimit(diskCapacity);
    }
}

void AplusWebView::releaseMemory(bool critical)
{
    WebCore::releaseMemory(critical ? Critical::Yes : Critical::No, Synchronous::Yes);
}

void AplusWebView::shutdown()
{
    for (auto& [fd, watch] : fdWatches()) {
        watch->stopped = true;
        [[maybe_unused]] auto written = write(watch->wake[1], "q", 1);
    }
    fdWatches().clear();

    FontCache::invalidateAllFontCaches(FontCache::ShouldRunInvalidationCallback::No);
    threadGlobalDataSingleton().destroy();
}

void AplusWebView::run()
{
    RunLoop::run();
}

void AplusWebView::quit()
{
    RunLoop::mainSingleton().stop();
}

void AplusWebView::dispatch(std::function<void()> function)
{
    callOnMainThread([function = WTF::move(function)] {
        function();
    });
}

void AplusWebView::dispatchAfter(double seconds, std::function<void()> function)
{
    RunLoop::mainSingleton().dispatchAfter(Seconds(seconds), [function = WTF::move(function)] {
        function();
    });
}

void AplusWebView::watchFd(int fd, std::function<void()> handler)
{
    unwatchFd(fd);

    auto watch = std::make_shared<FdWatch>();
    watch->fd = fd;
    watch->handler = WTF::move(handler);
    if (pipe(watch->wake) < 0) {
        return;
    }
    fdWatches()[fd] = watch;

    Thread::create("AplusFdWatch"_s, [watch] {
        while (!watch->stopped) {
            struct pollfd fds[2] = {
                { watch->fd, POLLIN, 0 },
                { watch->wake[0], POLLIN, 0 },
            };
            int result = poll(fds, 2, -1);
            if (result < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
            if (fds[1].revents || watch->stopped) {
                break;
            }
            if (!fds[0].revents) {
                continue;
            }
            bool hangup = fds[0].revents & (POLLHUP | POLLERR | POLLNVAL);
            callOnMainThread([watch] {
                if (!watch->stopped) {
                    watch->handler();
                }
                watch->done.signal();
            });
            watch->done.wait();
            if (hangup) {
                break;
            }
        }
        close(watch->wake[0]);
        close(watch->wake[1]);
    })->detach();
}

void AplusWebView::unwatchFd(int fd)
{
    auto it = fdWatches().find(fd);
    if (it == fdWatches().end()) {
        return;
    }
    it->second->stopped = true;
    [[maybe_unused]] auto written = write(it->second->wake[1], "q", 1);
    fdWatches().erase(it);
}

AplusWebSettings& AplusWebView::defaultSettings()
{
    static NeverDestroyed<AplusWebSettings> settings;
    return settings.get();
}

AplusWebView::AplusWebView(AplusWebViewClient* client, bool privateBrowsing)
    : m_client(client)
    , m_private(privateBrowsing)
{
    initialize();

    RefPtr<WebViewGroup> viewGroup = WebViewGroup::getOrCreate("default"_s, makeString(String::fromUTF8(aplusConfigDir()), "/LocalStorage"_s));

    PAL::SessionID sessionID = privateBrowsing ? PAL::SessionID::legacyPrivateSessionID() : PAL::SessionID::defaultSessionID();
    if (privateBrowsing) {
        NetworkStorageSessionMap::ensureSession(sessionID);
    }

    auto storageProvider = PageStorageSessionProvider::create();
    AplusWebView* view = this;

    PageConfiguration configuration(
        PageIdentifier::generate(), std::nullopt,
        sessionID,
        makeUniqueRef<EditorClientAplus>(view),
        LegacySocketProvider::create(),
        makeUniqueRef<DummyWebRTCProvider>(),
        CacheStorageProvider::create(),
        viewGroup->userContentController(),
        BackForwardList::create(),
        CookieJar::create(storageProvider.copyRef()),
        makeUniqueRef<ProgressTrackerClientAplus>(view),
        PageConfiguration::LocalMainFrameCreationParameters {
            CompletionHandler<UniqueRef<LocalFrameLoaderClient>(LocalFrame&, FrameLoader&)> { [view](LocalFrame&, auto& frameLoader) {
                return makeUniqueRefWithoutRefCountedCheck<FrameLoaderClientAplus>(frameLoader, view);
            } },
            SandboxFlags { }
        },
        FrameIdentifier::generate(),
        nullptr,
        makeUniqueRef<DummySpeechRecognitionProvider>(),
        WebBroadcastChannelRegistry::getOrCreate(privateBrowsing),
        makeUniqueRef<DummyStorageProvider>(),
        DummyModelPlayerProvider::create(),
        EmptyBadgeClient::create(),
        LegacyHistoryItemClient::singleton(),
        makeUniqueRef<ContextMenuClientAplus>(view),
        makeUniqueRef<ChromeClientAplus>(view),
        makeUniqueRef<WebCryptoClient>(),
        makeUniqueRef<DocumentSyncClient>());

    configuration.dragClient = makeUnique<DragClientAplus>(view);
    configuration.inspectorBackendClient = std::make_unique<InspectorClientAplus>();
    configuration.diagnosticLoggingClient = makeUnique<WebKit::WebDiagnosticLoggingClient>();
    configuration.databaseProvider = &WebDatabaseProvider::singleton();
    configuration.pluginInfoProvider = adoptRef(*new EmptyPluginInfoProvider);
    configuration.storageNamespaceProvider = &viewGroup->storageNamespaceProvider();
    configuration.visitedLinkStore = &viewGroup->visitedLinkStore();

    m_page = &Page::create(WTF::move(configuration)).leakRef();
    storageProvider->setPage(*m_page);

    applySettings(defaultSettings());

    if (RefPtr frame = mainFrame()) {
        frame->init();
    }
}

AplusWebView::~AplusWebView()
{
    if (m_popup) {
        m_popup->disconnectClient();
        m_popup = nullptr;
    }
    if (RefPtr frame = mainFrame()) {
        frame->loader().detachFromParent();
    }
    m_page->deref();
    m_page = nullptr;
}

LocalFrame* AplusWebView::mainFrame() const
{
    if (!m_page) {
        return nullptr;
    }
    return dynamicDowncast<LocalFrame>(m_page->mainFrame());
}

LocalFrame* AplusWebView::focusedFrame() const
{
    if (!m_page) {
        return nullptr;
    }
    return m_page->focusController().focusedOrMainFrame();
}

void AplusWebView::loadURL(const char* urlString)
{
    RefPtr frame = mainFrame();
    std::string text = trim(urlString);
    if (!frame || text.empty()) {
        return;
    }

    URL url;
    struct stat st;
    if (text[0] == '/' && !stat(text.c_str(), &st)) {
        url = URL::fileURLWithFileSystemPath(String::fromUTF8(text.c_str()));
    } else {
        url = URL(URL(), String::fromUTF8(text.c_str()));
    }
    if (!url.isValid()) {
        url = URL(URL(), String::fromUTF8(("http://" + text).c_str()));
    }
    if (!url.isValid()) {
        return;
    }

    frame->loader().load(FrameLoadRequest(*frame, ResourceRequest(WTF::move(url))));
}

void AplusWebView::loadHTML(const char* html, const char* baseURL)
{
    RefPtr frame = mainFrame();
    if (!frame || !html) {
        return;
    }

    URL base = baseURL && *baseURL ? URL(URL(), String::fromUTF8(baseURL)) : aboutBlankURL();
    size_t length = strlen(html);
    Ref buffer = SharedBuffer::create(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(html), length));
    ResourceResponse response(URL(base), "text/html"_s, static_cast<long long>(length), "UTF-8"_s);
    SubstituteData data(WTF::move(buffer), URL(), WTF::move(response), SubstituteData::SessionHistoryVisibility::Hidden);
    frame->loader().load(FrameLoadRequest(*frame, ResourceRequest(WTF::move(base)), WTF::move(data)));
}

void AplusWebView::reload(bool bypassCache)
{
    if (RefPtr frame = mainFrame()) {
        OptionSet<ReloadOption> options;
        if (bypassCache) {
            options.add(ReloadOption::FromOrigin);
        }
        frame->loader().reload(options, true);
    }
}

void AplusWebView::stop()
{
    if (RefPtr frame = mainFrame()) {
        frame->loader().stopForUserCancel();
    }
}

void AplusWebView::goBack()
{
    if (canGoBack()) {
        m_page->backForward().goBack();
    }
}

void AplusWebView::goForward()
{
    if (canGoForward()) {
        m_page->backForward().goForward();
    }
}

bool AplusWebView::canGoBack() const
{
    return m_page && m_page->backForward().canGoBackOrForward(-1);
}

bool AplusWebView::canGoForward() const
{
    return m_page && m_page->backForward().canGoBackOrForward(1);
}

std::string AplusWebView::url() const
{
    RefPtr frame = mainFrame();
    if (frame && frame->document() && !frame->document()->url().isEmpty()) {
        return toStdString(frame->document()->url().string());
    }
    return m_url;
}

std::string AplusWebView::title() const
{
    return m_title;
}

std::string AplusWebView::mimeType() const
{
    RefPtr frame = mainFrame();
    if (!frame) {
        return { };
    }
    if (RefPtr loader = frame->loader().documentLoader()) {
        return toStdString(loader->responseMIMEType());
    }
    return { };
}

std::string AplusWebView::responseURL() const
{
    RefPtr frame = mainFrame();
    if (!frame) {
        return { };
    }
    if (RefPtr loader = frame->loader().documentLoader()) {
        return toStdString(loader->response().url().string());
    }
    return { };
}

std::string AplusWebView::suggestedFilename() const
{
    RefPtr frame = mainFrame();
    if (!frame) {
        return { };
    }
    if (RefPtr loader = frame->loader().documentLoader()) {
        return toStdString(loader->response().suggestedFilename());
    }
    return { };
}

std::string AplusWebView::preferredExtensionForMIMEType(const char* type)
{
    return toStdString(MIMETypeRegistry::preferredExtensionForMIMEType(String::fromUTF8(type ? type : "")));
}

std::string AplusWebView::selectedText() const
{
    RefPtr frame = focusedFrame();
    if (!frame) {
        return { };
    }
    return toStdString(frame->editor().selectedText());
}

std::string AplusWebView::source() const
{
    RefPtr frame = mainFrame();
    if (!frame || !frame->document()) {
        return { };
    }
    return toStdString(serializeFragment(*frame->document(), SerializedNodes::SubtreeIncludingNode));
}

std::vector<uint8_t> AplusWebView::documentData() const
{
    RefPtr frame = mainFrame();
    if (!frame) {
        return { };
    }
    RefPtr loader = frame->loader().documentLoader();
    if (!loader) {
        return { };
    }
    RefPtr data = loader->mainResourceData();
    if (!data) {
        return { };
    }
    return toStdVector(data->copyData());
}

std::vector<uint8_t> AplusWebView::archive() const
{
#if ENABLE(MHTML)
    if (m_page) {
        return toStdVector(MHTMLArchive::generateMHTMLData(m_page)->copyData());
    }
#endif
    return { };
}

double AplusWebView::progress() const
{
    return m_progress;
}

bool AplusWebView::loading() const
{
    return m_loading;
}

void AplusWebView::resize(int width, int height)
{
    m_width = std::max(width, 1);
    m_height = std::max(height, 1);

    RefPtr frame = mainFrame();
    if (!frame || !frame->view()) {
        return;
    }
    RefPtr view = frame->view();
    view->resize(m_width, m_height);
    view->forceLayout();
    view->adjustViewSize();
}

void AplusWebView::layoutIfNeeded()
{
    RefPtr frame = mainFrame();
    if (!frame || !frame->view() || !frame->contentRenderer()) {
        return;
    }
    m_page->isolatedUpdateRendering();
    if (RefPtr view = frame->view()) {
        view->updateLayoutAndStyleIfNeededRecursive();
    }
}

void AplusWebView::paint(cairo_t* cr, int x, int y, int width, int height)
{
    RefPtr frame = mainFrame();
    if (!cr || !frame || !frame->view() || !frame->contentRenderer()) {
        return;
    }

    layoutIfNeeded();

    RefPtr view = frame->view();
    if (!view) {
        return;
    }

    cairo_save(cr);
    cairo_rectangle(cr, x, y, width, height);
    cairo_clip(cr);
    {
        GraphicsContextCairo context { RefPtr<cairo_t>(cr) };
        view->paint(context, IntRect(x, y, width, height));
    }
    cairo_restore(cr);
}

void AplusWebView::setFocused(bool focused)
{
    if (!m_page) {
        return;
    }
    auto& focusController = m_page->focusController();
    focusController.setFocused(focused);
    if (focused && !focusController.focusedFrame()) {
        focusController.setFocusedFrame(mainFrame());
    }
}

void AplusWebView::setActive(bool active)
{
    if (m_page) {
        m_page->focusController().setActive(active);
    }
}

void AplusWebView::setVisible(bool visible)
{
    if (!m_page) {
        return;
    }
    m_page->setIsVisible(visible);
    RefPtr frame = mainFrame();
    if (frame && frame->view()) {
        frame->view()->setParentVisible(visible);
    }
    if (visible) {
        m_client->invalidate(0, 0, m_width, m_height);
    }
}

bool AplusWebView::handleShortcut(const AplusKeyEvent& event, bool beforePage)
{
    uint8_t modifiers = event.modifiers & (APLUS_MOD_SHIFT | APLUS_MOD_CTRL | APLUS_MOD_ALT | APLUS_MOD_META);
    int key = windowsKeyCode(event);

    if (!beforePage) {
        if (key == VK_BACK && !modifiers) {
            goBack();
            return true;
        }
        if (key == VK_BACK && modifiers == APLUS_MOD_SHIFT) {
            goForward();
            return true;
        }
        return false;
    }

    if (modifiers == APLUS_MOD_ALT) {
        if (key == VK_LEFT) {
            goBack();
            return true;
        }
        if (key == VK_RIGHT) {
            goForward();
            return true;
        }
        return false;
    }

    if (key == VK_F5 && !(modifiers & ~(APLUS_MOD_SHIFT | APLUS_MOD_CTRL))) {
        reload(modifiers != 0);
        return true;
    }

    if (!(modifiers & APLUS_MOD_CTRL) || (modifiers & (APLUS_MOD_ALT | APLUS_MOD_META))) {
        return false;
    }

    switch (key) {
    case 'R':
        reload(modifiers & APLUS_MOD_SHIFT);
        return true;
    case VK_OEM_PLUS:
    case VK_ADD:
        zoomIn();
        return true;
    case VK_OEM_MINUS:
    case VK_SUBTRACT:
        zoomOut();
        return true;
    case '0':
    case VK_NUMPAD0:
        resetZoom();
        return true;
    default:
        return false;
    }
}

bool AplusWebView::handleKeyScroll(const AplusKeyEvent& event)
{
    RefPtr frame = focusedFrame();
    if (!frame) {
        return false;
    }
    if (event.modifiers & (APLUS_MOD_CTRL | APLUS_MOD_ALT | APLUS_MOD_META)) {
        return false;
    }

    ScrollDirection direction;
    ScrollGranularity granularity;
    switch (windowsKeyCode(event)) {
    case VK_UP:
        direction = ScrollDirection::ScrollUp;
        granularity = ScrollGranularity::Line;
        break;
    case VK_DOWN:
        direction = ScrollDirection::ScrollDown;
        granularity = ScrollGranularity::Line;
        break;
    case VK_LEFT:
        direction = ScrollDirection::ScrollLeft;
        granularity = ScrollGranularity::Line;
        break;
    case VK_RIGHT:
        direction = ScrollDirection::ScrollRight;
        granularity = ScrollGranularity::Line;
        break;
    case VK_HOME:
        direction = ScrollDirection::ScrollUp;
        granularity = ScrollGranularity::Document;
        break;
    case VK_END:
        direction = ScrollDirection::ScrollDown;
        granularity = ScrollGranularity::Document;
        break;
    case VK_PRIOR:
        direction = ScrollDirection::ScrollUp;
        granularity = ScrollGranularity::Page;
        break;
    case VK_NEXT:
        direction = ScrollDirection::ScrollDown;
        granularity = ScrollGranularity::Page;
        break;
    case VK_SPACE:
        direction = (event.modifiers & APLUS_MOD_SHIFT) ? ScrollDirection::ScrollUp : ScrollDirection::ScrollDown;
        granularity = ScrollGranularity::Page;
        break;
    default:
        return false;
    }

    return frame->eventHandler().scrollRecursively(direction, granularity);
}

bool AplusWebView::keyEvent(const AplusKeyEvent& event)
{
    if (event.down && handleShortcut(event, true)) {
        return true;
    }

    RefPtr frame = focusedFrame();
    if (!frame || !frame->view() || !frame->document()) {
        return false;
    }

    if (frame->eventHandler().keyEvent(PlatformKeyboardEvent(event))) {
        return true;
    }
    if (!event.down) {
        return false;
    }
    if (handleShortcut(event, false)) {
        return true;
    }
    return handleKeyScroll(event);
}

bool AplusWebView::showContextMenu(const AplusPointerEvent& event)
{
    auto& controller = m_page->contextMenuController();
    controller.clearContextMenu();

    RefPtr frame = m_page->focusController().focusedOrMainFrame();
    if (!frame) {
        return false;
    }

    frame->eventHandler().sendContextMenuEvent(PlatformMouseEvent(event));
    if (!controller.contextMenu()) {
        return true;
    }

    AplusHitTest hit = toAplusHitTest(controller.hitTestResult());
    controller.clearContextMenu();
    m_client->showContextMenu(hit, event.x, event.y);
    return true;
}

bool AplusWebView::pointerEvent(const AplusPointerEvent& event)
{
    RefPtr frame = mainFrame();
    if (!frame || !frame->view() || !frame->document()) {
        return false;
    }

    switch (event.kind) {
    case APLUS_POINTER_PRESS: {
        m_pressed = true;
        if (event.button == APLUS_BUTTON_BACK) {
            goBack();
            return true;
        }
        if (event.button == APLUS_BUTTON_FORWARD) {
            goForward();
            return true;
        }
        if (event.button == APLUS_BUTTON_MIDDLE) {
            AplusHitTest hit = hitTest(event.x, event.y);
            if (!hit.linkURL.empty() && !hit.editable) {
                m_client->createView(hit.linkURL.c_str(), true);
                return true;
            }
        }
        bool handled = frame->eventHandler().handleMousePressEvent(PlatformMouseEvent(event)).wasHandled();
        if (event.button == APLUS_BUTTON_RIGHT) {
            return showContextMenu(event);
        }
        return handled;
    }
    case APLUS_POINTER_RELEASE:
        m_pressed = false;
        if (event.button == APLUS_BUTTON_BACK || event.button == APLUS_BUTTON_FORWARD) {
            return true;
        }
        return frame->eventHandler().handleMouseReleaseEvent(PlatformMouseEvent(event)).wasHandled();
    case APLUS_POINTER_MOVE:
        return frame->eventHandler().mouseMoved(PlatformMouseEvent(event)).wasHandled();
    }
    return false;
}

bool AplusWebView::scrollEvent(const AplusScrollEvent& event)
{
    if ((event.modifiers & APLUS_MOD_CTRL) && !(event.modifiers & (APLUS_MOD_ALT | APLUS_MOD_META))) {
        if (event.dy > 0) {
            zoomIn();
        } else if (event.dy < 0) {
            zoomOut();
        }
        return true;
    }

    RefPtr frame = mainFrame();
    if (!frame || !frame->view() || !frame->document()) {
        return false;
    }

    auto result = frame->eventHandler().handleWheelEvent(PlatformWheelEvent(event), { WheelEventProcessingSteps::SynchronousScrolling, WheelEventProcessingSteps::NonBlockingDOMEventDispatch });
    return result.first.wasHandled();
}

void AplusWebView::pointerLeave()
{
    if (m_pressed) {
        return;
    }
    AplusPointerEvent event { };
    event.kind = APLUS_POINTER_MOVE;
    event.x = -1;
    event.y = -1;
    event.globalX = -1;
    event.globalY = -1;
    pointerEvent(event);
}

void AplusWebView::scrollBy(int dx, int dy)
{
    RefPtr frame = mainFrame();
    if (frame && frame->view()) {
        frame->view()->scrollBy(IntSize(dx, dy));
    }
}

void AplusWebView::scrollTo(int x, int y)
{
    RefPtr frame = mainFrame();
    if (frame && frame->view()) {
        frame->view()->setScrollPosition(IntPoint(x, y));
    }
}

int AplusWebView::scrollX() const
{
    RefPtr frame = mainFrame();
    if (frame && frame->view()) {
        return frame->view()->scrollPosition().x();
    }
    return 0;
}

int AplusWebView::scrollY() const
{
    RefPtr frame = mainFrame();
    if (frame && frame->view()) {
        return frame->view()->scrollPosition().y();
    }
    return 0;
}

void AplusWebView::setZoom(double factor, bool textOnly)
{
    factor = std::clamp(factor, zoomMinimum, zoomMaximum);
    m_zoom = factor;
    m_textZoom = textOnly;
    if (RefPtr frame = mainFrame()) {
        frame->setPageAndTextZoomFactors(textOnly ? 1 : factor, textOnly ? factor : 1);
    }
}

void AplusWebView::zoomIn()
{
    setZoom(m_zoom * zoomRatio, m_textZoom);
}

void AplusWebView::zoomOut()
{
    setZoom(m_zoom / zoomRatio, m_textZoom);
}

void AplusWebView::resetZoom()
{
    setZoom(1, m_textZoom);
}

bool AplusWebView::find(const char* text, unsigned options)
{
    if (!m_page || !text || !*text) {
        return false;
    }
    return !!m_page->findString(String::fromUTF8(text), toFindOptions(options)).range;
}

unsigned AplusWebView::markAllMatches(const char* text, unsigned options, unsigned limit)
{
    if (!m_page || !text || !*text) {
        return 0;
    }
    m_page->unmarkAllTextMatches();
    return m_page->markAllMatchesForText(String::fromUTF8(text), toFindOptions(options), true, limit);
}

void AplusWebView::unmarkAllMatches()
{
    if (m_page) {
        m_page->unmarkAllTextMatches();
    }
}

bool AplusWebView::canEdit(AplusEditCommand command) const
{
    RefPtr frame = focusedFrame();
    if (!frame) {
        return false;
    }
    return frame->editor().command(editCommandName(command)).isEnabled();
}

void AplusWebView::edit(AplusEditCommand command)
{
    RefPtr frame = focusedFrame();
    if (!frame) {
        return;
    }
    frame->editor().command(editCommandName(command)).execute();
}

AplusHitTest AplusWebView::hitTest(int x, int y) const
{
    RefPtr frame = mainFrame();
    if (!frame || !frame->view()) {
        return { };
    }
    IntPoint point = frame->view()->windowToContents(IntPoint(x, y));
    constexpr OptionSet<HitTestRequest::Type> types { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::IgnoreClipping, HitTestRequest::Type::DisallowUserAgentShadowContent, HitTestRequest::Type::AllowChildFrameContent };
    return toAplusHitTest(frame->eventHandler().hitTestResultAtPoint(point, types));
}

void AplusWebView::exitFullScreen()
{
    RefPtr frame = mainFrame();
    if (frame && frame->document()) {
        frame->document()->fullscreen().fullyExitFullscreen();
    }
}

void AplusWebView::setUserAgent(const char* userAgent)
{
    m_userAgent = userAgent ? userAgent : "";
}

void AplusWebView::applySettings(const AplusWebSettings& values)
{
    if (!m_page) {
        return;
    }
    auto& settings = m_page->settings();
    settings.setScriptEnabled(values.javascriptEnabled);
    settings.setImagesEnabled(values.imagesEnabled);
    settings.setLoadsImagesAutomatically(values.imagesEnabled);
    settings.setLocalStorageEnabled(values.localStorageEnabled);
    settings.setSessionStorageEnabled(true);
    settings.setVisualViewportAPIEnabled(true);
    settings.setDataTransferItemsEnabled(true);
    settings.setWebSecurityEnabled(values.webSecurityEnabled);
#if ENABLE(VIDEO)
    settings.setMediaEnabled(values.mediaEnabled);
#endif
#if ENABLE(WEB_AUDIO)
    settings.setWebAudioEnabled(values.mediaEnabled);
#endif
    settings.setScrollAnimatorEnabled(values.smoothScrolling);
    settings.setJavaScriptCanOpenWindowsAutomatically(values.javascriptCanOpenWindows);
    settings.setDefaultFontSize(values.defaultFontSize);
    settings.setDefaultFixedFontSize(values.defaultFixedFontSize);
    settings.setMinimumFontSize(values.minimumFontSize);
    if (!values.standardFontFamily.empty()) {
        settings.setStandardFontFamily(String::fromUTF8(values.standardFontFamily.c_str()));
    }
    if (!values.fixedFontFamily.empty()) {
        settings.setFixedFontFamily(String::fromUTF8(values.fixedFontFamily.c_str()));
    }
    if (!values.serifFontFamily.empty()) {
        settings.setSerifFontFamily(String::fromUTF8(values.serifFontFamily.c_str()));
    }
    if (!values.sansSerifFontFamily.empty()) {
        settings.setSansSerifFontFamily(String::fromUTF8(values.sansSerifFontFamily.c_str()));
    }
    if (!values.defaultTextEncoding.empty()) {
        settings.setDefaultTextEncodingName(String::fromUTF8(values.defaultTextEncoding.c_str()));
    }
}

void AplusWebView::setTransparent(bool transparent)
{
    m_transparent = transparent;
    RefPtr frame = mainFrame();
    if (frame && frame->view()) {
        frame->view()->setTransparent(transparent);
        frame->view()->setBaseBackgroundColor(transparent ? Color(Color::transparentBlack) : Color(Color::white));
    }
}

void AplusWebView::setPlaceholderColor(double r, double g, double b)
{
    auto channel = [](double v) -> uint32_t {
        return static_cast<uint32_t>(std::clamp(v, 0.0, 1.0) * 255.0 + 0.5);
    };
    m_placeholderColor = 0xFF000000 | (channel(r) << 16) | (channel(g) << 8) | channel(b);

    RefPtr frame = mainFrame();
    if (m_transparent || !frame || !frame->view()) {
        return;
    }

    RefPtr loader = frame->loader().documentLoader();
    URL url = loader ? loader->url() : URL();
    if (url.isEmpty() || url.protocolIsAbout()) {
        frame->view()->setBaseBackgroundColor(Color(SRGBA<uint8_t> { static_cast<uint8_t>(m_placeholderColor >> 16), static_cast<uint8_t>(m_placeholderColor >> 8), static_cast<uint8_t>(m_placeholderColor) }));
    }
}

void AplusWebView::popupMenuDidSelect(int index)
{
    if (RefPtr popup = m_popup) {
        popup->didSelect(index);
    }
}

void AplusWebView::popupMenuDidHide()
{
    if (RefPtr popup = m_popup) {
        popup->didHide();
    }
}

void AplusWebView::setPopupMenu(PopupMenuAplus* popup)
{
    m_popup = popup;
}

void AplusWebView::didStartProgress()
{
    m_loading = true;
    m_progress = 0;
    m_client->progressChanged(0);
    m_client->navigationStateChanged();
}

void AplusWebView::didChangeProgress(double progress)
{
    m_progress = progress;
    m_client->progressChanged(progress);
}

void AplusWebView::didFinishProgress()
{
    m_loading = false;
    m_progress = 1;
    m_client->progressChanged(1);
    m_client->navigationStateChanged();
}

void AplusWebView::didChangeTitle(const std::string& title)
{
    m_title = title;
    m_client->titleChanged(title.c_str());
}

void AplusWebView::didStartLoad(const std::string& url)
{
    m_client->loadStarted(url.c_str());
}

void AplusWebView::didCommitLoad(const std::string& url)
{
    m_url = url;
    m_client->loadCommitted(url.c_str());
    m_client->urlChanged(url.c_str());
    m_client->navigationStateChanged();
}

void AplusWebView::didFinishLoad()
{
    m_client->loadFinished();
    m_client->navigationStateChanged();
}

void AplusWebView::didFailLoad(const std::string& url, const std::string& description, bool cancelled)
{
    m_client->loadFailed(url.c_str(), description.c_str(), cancelled);
    m_client->navigationStateChanged();
}

void AplusWebView::didChangeURL(const std::string& url)
{
    m_url = url;
    m_client->urlChanged(url.c_str());
    m_client->navigationStateChanged();
}
