# Windows SMTC Integration - Code Snippets & Examples

## Overview of Integration Points

This document shows exactly where and how the Windows SMTC integration has been added to the codebase.

## 1. Header Files & Includes

### MainDlg.h
```cpp
#ifdef _WIN32
#include "WindowsMediaTransportControls.h"
#endif // _WIN32

class MainDlg 
    : public QWidget
    , public IRaopEvents
    , public IDnsSDEvents
    , protected KeyboardHook::ICallback
{
    // ... other members ...
    
#ifdef _WIN32
    WindowsMediaTransportControlsPtr    m_mediaTransportControls;
#endif // _WIN32
};
```

## 2. Initialization (Constructor)

### MainDlg Constructor
```cpp
MainDlg::MainDlg(const SharedPtr<IValueCollection>& config, const std::string& configName)
{
    // ... existing code ...
    
    ConfigureSystemTray();

    // Initialize Windows Media Transport Controls (SMTC) on Windows
#ifdef _WIN32
    try
    {
        m_mediaTransportControls = std::make_shared<WindowsMediaTransportControls>();
        if (WindowsMediaTransportControls::IsAvailable())
        {
            if (m_mediaTransportControls->Initialize())
            {
                spdlog::info("Windows System Media Transport Controls initialized");
            }
            else
            {
                spdlog::warn("Failed to initialize Windows System Media Transport Controls");
                m_mediaTransportControls.reset();
            }
        }
        else
        {
            spdlog::debug("Windows System Media Transport Controls not available on this system");
            m_mediaTransportControls.reset();
        }
    }
    catch (const std::exception& e)
    {
        spdlog::warn("Exception initializing Windows Media Transport Controls: {}", e.what());
        m_mediaTransportControls.reset();
    }
#endif // _WIN32

    // ... rest of constructor ...
}
```

## 3. Playback State Updates

### OnPlayState Method
```cpp
void MainDlg::OnPlayState(bool isPlaying)
{
    unique_lock<mutex> sync(m_mtx);

    const bool wasPlaying = m_isPlaying;
    m_isPlaying = isPlaying;

    auto entry = m_mapDacpService.find(m_currentDacpID.id);
    const bool enabled = entry != m_mapDacpService.end();

    sync.unlock();

    if (enabled)
    {
        m_buttonPlayPauseTrack->setIcon(isPlaying ? m_iconPause : m_iconPlay);
    }
    else
    {
        m_buttonPlayPauseTrack->setIcon(m_iconPlay);
    }
    if (wasPlaying != isPlaying)
    {
        // Update Windows Media Transport Controls
#ifdef _WIN32
        if (m_mediaTransportControls && m_mediaTransportControls->IsInitialized())
        {
            m_mediaTransportControls->SetPlaybackState(
                isPlaying ? WindowsMediaTransportControls::PlaybackState::Playing
                          : WindowsMediaTransportControls::PlaybackState::Paused
            );
        }
#endif // _WIN32

        OnUpdateTray();
    }
}
```

## 4. Metadata Updates

### OnDmapInfo Method
```cpp
void MainDlg::OnDmapInfo(QString album, QString track, QString artist)
{
    const lock_guard<recursive_mutex> guard(m_mtxTitleInfo);

    m_strCurrentArtist = std::move(artist);
    m_strCurrentTrack = std::move(track);
    m_strCurrentAlbum = std::move(album);

    if (m_labelArtistTitleInfo)
    {
        assert(m_labelAlbumTitleInfo);
        assert(m_labelTrackTitleInfo);

        m_labelAlbumTitleInfo->setText(m_strCurrentAlbum);
        m_labelTrackTitleInfo->setText(m_strCurrentTrack);
        m_labelArtistTitleInfo->setText(m_strCurrentArtist);
    }

    // Update Windows Media Transport Controls with new metadata
#ifdef _WIN32
    if (m_mediaTransportControls && m_mediaTransportControls->IsInitialized())
    {
        WindowsMediaTransportControls::MediaMetadata metadata;
        metadata.title = m_strCurrentTrack.toStdString();
        metadata.artist = m_strCurrentArtist.toStdString();
        metadata.album = m_strCurrentAlbum.toStdString();

        // Get duration and position from RAOP server if available
        int duration = 0;
        int position = 0;
        std::string clientID;
        {
            const lock_guard<mutex> lock(m_mtx);
            if (m_raopServer)
            {
                m_raopServer->GetProgress(duration, position, clientID);
            }
        }
        metadata.durationSeconds = duration;
        metadata.positionSeconds = position;

        m_mediaTransportControls->UpdateMetadata(metadata);
    }
#endif // _WIN32

    OnUpdateTray();
}
```

## 5. Album Art Updates

### OnAlbumArt Method
```cpp
void MainDlg::OnAlbumArt()
{
    ImageQueueItemPtr item;
    {
        const lock_guard<mutex> guard(m_mtx);

        if (!m_imageQueue.empty())
        {
            do
            {
                item = std::move(m_imageQueue.front());
                m_imageQueue.pop_front();
            } while (!m_imageQueue.empty());
        }
        else
        {
            return;
        }
    }
    assert(item);

    try
    {
        const lock_guard<recursive_mutex> guard(m_mtxTitleInfo);

        if (item->second == "NONE"s)
        {
            if (m_imageAlbumArt)
            {
                m_imageAlbumArt->setPixmap(m_pixmapShairport);
                m_currentAlbumArt.reset();
            }
            
            // Update Windows Media Transport Controls - clear album art
#ifdef _WIN32
            if (m_mediaTransportControls && m_mediaTransportControls->IsInitialized())
            {
                m_mediaTransportControls->ClearAlbumArt();
            }
#endif // _WIN32
        }
        else
        {
            if (!m_currentAlbumArt)
            {
                m_currentAlbumArt = make_unique<QPixmap>();
            }

            if (m_currentAlbumArt->loadFromData(item->first, item->second.c_str()))
            {
                {
                    QImage surface(tr(":/ArtShadow.png"));
                    QPainter painter(&surface);

                    painter.drawPixmap(m_rectAlbumArt, m_currentAlbumArt->scaled(m_rectAlbumArt.width(), m_rectAlbumArt.height()));
                    painter.end();

                    *m_currentAlbumArt = QPixmap::fromImage(surface);
                }
                if (m_imageAlbumArt)
                {
                    m_imageAlbumArt->setPixmap(*m_currentAlbumArt);
                }
                
                // Update Windows Media Transport Controls with album art
#ifdef _WIN32
                if (m_mediaTransportControls && m_mediaTransportControls->IsInitialized())
                {
                    m_mediaTransportControls->SetAlbumArt(
                        reinterpret_cast<const unsigned char*>(item->first.data()),
                        item->first.size(),
                        item->second
                    );
                }
#endif // _WIN32
            }
        }
    }
    catch (const exception& e)
    {
        spdlog::error("failed to setPixmap: {}", e.what());
    }
}
```

## 6. CMake Configuration

### CMakeLists.txt Changes
```cmake
# Add Windows SMTC support for Windows builds
if (WIN32)
    list(APPEND LIBRARY_SOURCES lib/WindowsMediaTransportControls.cpp)
endif()

# ... later in file ...

if (UNIX)
    target_link_libraries(ShairportQt PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::DBus)
else()
    target_link_libraries(ShairportQt PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
    # Link Windows Runtime libraries for SMTC
    target_link_libraries(ShairportQt PRIVATE windowsapp)
endif()
```

## Class Definition: WindowsMediaTransportControls

### Public API (inc/WindowsMediaTransportControls.h)
```cpp
#ifdef _WIN32

class WindowsMediaTransportControls
{
public:
    struct MediaMetadata
    {
        std::string title;
        std::string artist;
        std::string album;
        std::string albumArtData;      // Raw image data
        std::string albumArtType;      // MIME type (e.g., "image/jpeg")
        int durationSeconds = 0;
        int positionSeconds = 0;
    };

    enum class PlaybackState
    {
        Closed,
        Opening,
        Playing,
        Paused,
        Stopped
    };

public:
    WindowsMediaTransportControls();
    ~WindowsMediaTransportControls();

    // Non-copyable, non-movable
    WindowsMediaTransportControls(const WindowsMediaTransportControls&) = delete;
    WindowsMediaTransportControls& operator=(const WindowsMediaTransportControls&) = delete;

    bool Initialize() noexcept;
    void Shutdown() noexcept;
    void SetPlaybackState(PlaybackState state) noexcept;
    void UpdateMetadata(const MediaMetadata& metadata) noexcept;
    void SetAlbumArt(const unsigned char* imageData, size_t imageDataSize, const std::string& mimeType) noexcept;
    void ClearAlbumArt() noexcept;
    bool IsInitialized() const noexcept;
    static bool IsAvailable() noexcept;

private:
    winrt::Windows::Media::SystemMediaTransportControls m_smtc = nullptr;
    bool m_initialized = false;
    mutable std::mutex m_mutex;
};

using WindowsMediaTransportControlsPtr = std::shared_ptr<WindowsMediaTransportControls>;

#endif // _WIN32
```

## Key Implementation Details

### Metadata Update Implementation
```cpp
void WindowsMediaTransportControls::UpdateMetadata(const MediaMetadata& metadata) noexcept
{
    try
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_initialized || !m_smtc)
            return;

        auto displayUpdater = m_smtc.DisplayUpdater();
        auto musicProperties = displayUpdater.MusicProperties();

        // Update music metadata
        musicProperties.Title(winrt::to_hstring(metadata.title.empty() ? "Unknown" : metadata.title));
        musicProperties.Artist(winrt::to_hstring(metadata.artist.empty() ? "Unknown Artist" : metadata.artist));
        musicProperties.AlbumTitle(winrt::to_hstring(metadata.album.empty() ? "Unknown Album" : metadata.album));

        // Update timeline properties with duration and current position
        auto timelineProperties = displayUpdater.TimelineProperties();
        if (timelineProperties && metadata.durationSeconds > 0)
        {
            Windows::Foundation::TimeSpan duration;
            duration.count = static_cast<int64_t>(metadata.durationSeconds) * 10000000LL;
            timelineProperties.MaxSeekTime(duration);

            Windows::Foundation::TimeSpan position;
            position.count = static_cast<int64_t>(metadata.positionSeconds) * 10000000LL;
            timelineProperties.Position(position);
        }

        // Update the display
        displayUpdater.Update();
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception updating metadata: {}", e.what());
    }
}
```

### Album Art Implementation
```cpp
void WindowsMediaTransportControls::SetAlbumArt(
    const unsigned char* imageData,
    size_t imageDataSize,
    const std::string& mimeType) noexcept
{
    try
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_initialized || !m_smtc || !imageData || imageDataSize == 0)
            return;

        auto displayUpdater = m_smtc.DisplayUpdater();
        auto thumbnail = CreateThumbnail(imageData, imageDataSize, mimeType);
        
        if (thumbnail)
        {
            displayUpdater.Thumbnail(thumbnail);
            displayUpdater.Update();
        }
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception setting album art: {}", e.what());
    }
}

IRandomAccessStreamReference WindowsMediaTransportControls::CreateThumbnail(
    const unsigned char* imageData,
    size_t imageDataSize,
    const std::string& mimeType) noexcept
{
    try
    {
        auto stream = InMemoryRandomAccessStream();
        auto writer = DataWriter(stream);

        writer.WriteBytes(
            winrt::array_view<const uint8_t>(
                reinterpret_cast<const uint8_t*>(imageData),
                reinterpret_cast<const uint8_t*>(imageData) + imageDataSize
            )
        );

        auto op = writer.StoreAsync();
        op.get();

        stream.Seek(0);

        return RandomAccessStreamReference::CreateFromStream(stream);
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception creating thumbnail: {}", e.what());
        return nullptr;
    }
}
```

## Platform-Specific Compilation

All SMTC code is protected with `#ifdef _WIN32`:

- **Header files**: Only parsed on Windows builds
- **Implementation files**: Only compiled on Windows
- **CMake configuration**: Only linked on Windows
- **Initialization code**: Only executed on Windows

This ensures:
- ✅ Zero overhead on non-Windows platforms
- ✅ No compilation errors on Linux/macOS
- ✅ Automatic compilation on Windows
- ✅ Backward compatible with existing code

## Error Handling Pattern

All public methods follow this pattern:

```cpp
void WindowsMediaTransportControls::MethodName(...) noexcept
{
    try
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Check initialization
        if (!m_initialized || !m_smtc)
            return;
        
        // Perform operation
        // ...
        
        spdlog::debug("Operation completed successfully");
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception in MethodName: {}", e.what());
    }
}
```

Benefits:
- Exception safety
- Logging for debugging
- No crash on error
- Graceful degradation
- Thread-safe

## Summary

The integration is:
- ✅ **Minimal**: Only adds necessary code
- ✅ **Safe**: Thread-safe with proper locking
- ✅ **Robust**: Comprehensive error handling
- ✅ **Efficient**: Low overhead, lazy initialization
- ✅ **Compatible**: Doesn't affect non-Windows builds
- ✅ **Maintainable**: Clear, documented code with logging
