#pragma once

#ifdef _WIN32

#include <string>
#include <memory>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Graphics.Imaging.h>

/**
 * @class WindowsMediaTransportControls
 * @brief Integration with Windows System Media Transport Controls (SMTC) API
 * 
 * This class publishes now-playing metadata to Windows UI (lock screen, taskbar, etc.)
 * Only available on Windows 10/11 and later.
 */
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

    /**
     * Initialize the SMTC integration
     * @return true if initialization was successful, false otherwise
     */
    bool Initialize() noexcept;

    /**
     * Shut down the SMTC integration
     */
    void Shutdown() noexcept;

    /**
     * Update the current playback state
     */
    void SetPlaybackState(PlaybackState state) noexcept;

    /**
     * Update metadata for currently playing media
     */
    void UpdateMetadata(const MediaMetadata& metadata) noexcept;

    /**
     * Update the album art thumbnail
     * @param imageData Raw image data (JPEG, PNG, etc.)
     * @param imageDataSize Size of the image data
     * @param mimeType MIME type of the image (e.g., "image/jpeg")
     */
    void SetAlbumArt(const unsigned char* imageData, size_t imageDataSize, const std::string& mimeType) noexcept;

    /**
     * Clear the album art
     */
    void ClearAlbumArt() noexcept;

    /**
     * Check if SMTC is initialized and ready
     */
    bool IsInitialized() const noexcept;

    /**
     * Check if SMTC is available on this system
     */
    static bool IsAvailable() noexcept;

private:
    /**
     * Internal helper to create album art thumbnail from raw data
     */
    winrt::Windows::Storage::Streams::IRandomAccessStreamReference CreateThumbnail(
        const unsigned char* imageData,
        size_t imageDataSize,
        const std::string& mimeType) noexcept;

    /**
     * Convert PlaybackState to WinRT MediaPlaybackState
     */
    static winrt::Windows::Media::MediaPlaybackState ToWinRTState(PlaybackState state) noexcept;

private:
    winrt::Windows::Media::SystemMediaTransportControls m_smtc = nullptr;
    winrt::Windows::Media::MediaPlaybackAutoRepeatMode m_autoRepeat = 
        winrt::Windows::Media::MediaPlaybackAutoRepeatMode::None;
    bool m_initialized = false;
    mutable std::mutex m_mutex;
};

using WindowsMediaTransportControlsPtr = std::shared_ptr<WindowsMediaTransportControls>;

#endif // _WIN32
