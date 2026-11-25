add_custom_command(
    OUTPUT ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebKitVersion.h
    MAIN_DEPENDENCY ${WEBKITLEGACY_DIR}/scripts/generate-webkitversion.pl
    DEPENDS ${WEBKITLEGACY_DIR}/../../Configurations/Version.xcconfig
    COMMAND ${PERL_EXECUTABLE} ${WEBKITLEGACY_DIR}/scripts/generate-webkitversion.pl --config ${WEBKITLEGACY_DIR}/../../Configurations/Version.xcconfig --outputDir ${WebKitLegacy_DERIVED_SOURCES_DIR}
    VERBATIM)
list(APPEND WebKitLegacy_SOURCES ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebKitVersion.h)

list(APPEND WebKitLegacy_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBKITLEGACY_DIR}/aplus"
    "${WEBKITLEGACY_DIR}/aplus/API"
    "${WEBKITLEGACY_DIR}/aplus/WebCoreSupport"
    "${WTF_DIR}"
    "${WEBCORE_DIR}/platform/aplus"
)

add_definitions("-include WebKitPrefix.h")
add_definitions(-D_DEFAULT_SOURCE)

list(APPEND WebKitLegacy_SOURCES
    aplus/API/AplusDownload.cpp
    aplus/API/AplusWebView.cpp

    aplus/WebCoreSupport/BackForwardList.cpp
    aplus/WebCoreSupport/ChromeClientAplus.cpp
    aplus/WebCoreSupport/ContextMenuClientAplus.cpp
    aplus/WebCoreSupport/DragClientAplus.cpp
    aplus/WebCoreSupport/EditorClientAplus.cpp
    aplus/WebCoreSupport/FrameLoaderClientAplus.cpp
    aplus/WebCoreSupport/FrameNetworkingContextAplus.cpp
    aplus/WebCoreSupport/InspectorClientAplus.cpp
    aplus/WebCoreSupport/LegacyHistoryItemClient.cpp
    aplus/WebCoreSupport/PlatformStrategiesAplus.cpp
    aplus/WebCoreSupport/PopupMenuAplus.cpp
    aplus/WebCoreSupport/ProgressTrackerAplus.cpp
    aplus/WebCoreSupport/SearchPopupMenuAplus.cpp
    aplus/WebCoreSupport/SocketStreamHandleImplAplus.cpp
    aplus/WebCoreSupport/WebCryptoClient.cpp
    aplus/WebCoreSupport/WebDatabaseProvider.cpp
    aplus/WebCoreSupport/WebDiagnosticLoggingClient.cpp
    aplus/WebCoreSupport/WebKitLogging.cpp
    aplus/WebCoreSupport/WebResourceLoadSchedulerAplus.cpp
    aplus/WebCoreSupport/WebVisitedLinkStore.cpp

    WebCoreSupport/LegacySocketProvider.cpp
    WebCoreSupport/SocketStreamHandle.cpp
    WebCoreSupport/SocketStreamHandleImpl.cpp
    WebCoreSupport/WebSocketChannel.cpp
)
