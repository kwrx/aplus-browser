#pragma once

#if ENABLE(VIDEO)

#include <wtf/Function.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#define EP_EVENT(x)
#define EP_SCOPE(x)

namespace WebCore {

class NetworkStorageSession;
class MediaPlayer;

struct MediaPlayerAplusInfoTrack
{
	String      m_url;
	WTF::Vector<WTF::String> m_codecs;
	int         m_fps = 0;
	int         m_width = 0;
	int         m_height = 0;
	int         m_bitRate = 0;
};

struct MediaPlayerAplusInfo
{
	WTF::String m_audioCodec;
	WTF::String m_videoCodec;
	float       m_duration = 0;
	int         m_frequency = 0;
	int         m_bits;
	int         m_channels = 0;
	int         m_width = 0;
	int         m_height;
	int         m_bitRate = 0;
	bool        m_isLive = false;
	bool        m_isHLS = false;
    bool        m_isDownloadable = false;
    bool        m_isMediaSource = false;
    bool        m_clearKeyDRM = false;
    WTF::Vector<MediaPlayerAplusInfoTrack> m_hlsStreams;
    String      m_selectedHLSStreamURL;
};

struct MediaPlayerAplusStreamSettings
{
	bool m_decodeVideo = true;
	
    // NOTE: keep in sync with WkGlobalSettings_LoopFilter
    enum class SkipLoopFilter {
        Default,
        NonRef,
        BiDirectional,
        NonIntra,
        NonKey,
        All
    };

    SkipLoopFilter m_loopFilter = SkipLoopFilter::NonKey;
};

struct MediaPlayerAplusSettings
{
public:
    static MediaPlayerAplusSettings &settings();
    Function<NetworkStorageSession*()> m_storageSession;
};

}

#endif
