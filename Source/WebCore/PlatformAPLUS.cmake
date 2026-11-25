include(platform/Adwaita.cmake)
include(platform/Cairo.cmake)
include(platform/Curl.cmake)
include(platform/FreeType.cmake)
include(platform/ImageDecoders.cmake)
include(platform/OpenSSL.cmake)

if (NOT TARGET SQLite3::SQLite3)
    add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
endif ()

add_definitions(-D_DEFAULT_SOURCE)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/aplus"
    "${WEBCORE_DIR}/platform/audio/aplus"
    "${WEBCORE_DIR}/platform/graphics/aplus"
    "${WEBCORE_DIR}/platform/glib"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/network/aplus"
    "${WEBCORE_DIR}/platform/video-codecs"
)

list(APPEND WebCore_SOURCES
    inspector/LegacyWebSocketInspectorInstrumentation.cpp

    platform/aplus/GraphicsLayerAplus.cpp
    accessibility/playstation/AXObjectCachePlayStation.cpp
    accessibility/playstation/AccessibilityObjectPlayStation.cpp

    editing/glib/EditorGLib.cpp
    editing/glib/WebContentReaderGLib.cpp

    page/glib/DragControllerGLib.cpp

    platform/aplus/AplusClipboard.cpp
    platform/aplus/AplusPaths.cpp
    platform/aplus/CursorAplus.cpp
    platform/aplus/DragDataAplus.cpp
    platform/aplus/DragImageAplus.cpp
    platform/aplus/MIMETypeRegistryAplus.cpp
    platform/aplus/PasteboardAplus.cpp
    platform/aplus/PlatformKeyboardEventAplus.cpp
    platform/aplus/PlatformMouseEventAplus.cpp
    platform/aplus/PlatformScreenAplus.cpp
    platform/aplus/PlatformWheelEventAplus.cpp
    platform/aplus/UserAgentAplus.cpp

    platform/aplus/AplusAudioOutput.cpp
    platform/aplus/PlatformMediaEngineConfigurationFactoryAplus.cpp

    platform/audio/aplus/AudioBusAplus.cpp
    platform/audio/aplus/AudioDestinationAplus.cpp
    platform/audio/aplus/AudioDestinationOutputAplus.cpp
    platform/audio/aplus/AudioFileReaderAplus.cpp
    platform/audio/aplus/FFTFrameAplus.cpp

    platform/graphics/aplus/acinerella.c
    platform/graphics/aplus/AcinerellaAudioDecoder.cpp
    platform/graphics/aplus/AcinerellaBuffer.cpp
    platform/graphics/aplus/AcinerellaContainer.cpp
    platform/graphics/aplus/AcinerellaDecoder.cpp
    platform/graphics/aplus/AcinerellaHLS.cpp
    platform/graphics/aplus/AcinerellaMuxer.cpp
    platform/graphics/aplus/AcinerellaPointer.cpp
    platform/graphics/aplus/AcinerellaVideoDecoder.cpp
    platform/graphics/aplus/AudioTrackPrivateAplus.cpp
    platform/graphics/aplus/MediaDescriptionAplus.cpp
    platform/graphics/aplus/MediaPlayerPrivateAplus.cpp
    platform/graphics/aplus/MediaSampleAplus.cpp
    platform/graphics/aplus/MediaSourcePrivateAplus.cpp
    platform/graphics/aplus/SourceBufferPrivateAplus.cpp
    platform/graphics/aplus/VideoTrackPrivateAplus.cpp

    platform/generic/KeyedDecoderGeneric.cpp
    platform/generic/KeyedEncoderGeneric.cpp
    platform/generic/ScrollbarsControllerGeneric.cpp

    platform/glib/SSLKeyGeneratorGLib.cpp
    platform/glib/SystemSettings.cpp

    platform/graphics/glib/SystemFontDatabaseGLib.cpp

    platform/graphics/opentype/OpenTypeVerticalData.cpp

    platform/network/aplus/CurlSSLHandleAplus.cpp
    platform/network/aplus/NetworkStateNotifierAplus.cpp
    platform/network/curl/CurlCacheEntry.cpp
    platform/network/curl/CurlCacheManager.cpp

    platform/text/Hyphenation.cpp
    platform/text/LocaleICU.cpp

    platform/unix/LoggingUnix.cpp
    platform/unix/SharedMemoryUnix.cpp
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/aplus/AplusBrowserHost.h
    platform/aplus/AplusEvents.h
    platform/graphics/aplus/MediaPlayerAplus.h
)

list(APPEND WebCore_LIBRARIES
    JPEG::JPEG
    LibPSL::LibPSL
    LibXml2::LibXml2
    LibXslt::LibXslt
    PNG::PNG
    SQLite3::SQLite3
    WebP::libwebp
    WebP::demux
    ZLIB::ZLIB
)

set(CSS_VALUE_PLATFORM_DEFINES "HAVE_OS_DARK_MODE_SUPPORT=1")

file(GLOB _aplus_media_controls_icons "${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/*")
add_custom_command(
    OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/MediaControlsIconsAplus.cpp
    DEPENDS ${WEBCORE_DIR}/platform/aplus/GenerateMediaControlsIconsAplus.cmake ${_aplus_media_controls_icons}
    COMMAND ${CMAKE_COMMAND} -DINPUT_DIR=${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita -DOUTPUT=${WebCore_DERIVED_SOURCES_DIR}/MediaControlsIconsAplus.cpp -P ${WEBCORE_DIR}/platform/aplus/GenerateMediaControlsIconsAplus.cmake
    VERBATIM)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/MediaControlsIconsAplus.cpp)
